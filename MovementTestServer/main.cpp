#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "MoveProtocol.h"
#include <windows.h>
#include <atomic>
#include <chrono>
#include <deque>
#include <cstdio>
#include <limits>

namespace
{
    constexpr std::size_t MAX_COMMANDS = 1024;
    constexpr std::size_t MAX_SEND_BYTES = 2 * 1024 * 1024;

    struct TestSession
    {
        SOCKET socket = INVALID_SOCKET;

        std::atomic_bool stopping{ false };

        HANDLE stopEvent = nullptr;
        HANDLE sendEvent = nullptr;

        CRITICAL_SECTION commandCs;
        CRITICAL_SECTION sendCs;

        std::deque<Protocol::MovePacket> commands;
        std::deque<Protocol::ResultPacket> responses;

        // 현재 send 중인 패킷도 포함한다.
        std::size_t queuedSendBytes = 0;
    };

    void RequestStop(TestSession& session)
    {
        if (session.stopping.exchange(true))
            return;

        SetEvent(session.stopEvent);

        // 블로킹 recv/send를 깨운다.
        // closesocket은 기존 main()에서 한 번만 수행한다.
        shutdown(session.socket, SD_BOTH);
    }

    const char* StatusName(Protocol::MoveStatus status)
    {
        switch (status) {
        case Protocol::MoveStatus::Accepted:
            return "ACCEPTED";

        case Protocol::MoveStatus::OutOfBounds:
            return "OUT_OF_BOUNDS";

        case Protocol::MoveStatus::InvalidDirection:
            return "INVALID_DIRECTION";

        case Protocol::MoveStatus::StaleSequence:
            return "STALE_SEQUENCE";

        case Protocol::MoveStatus::WrongRound:
            return "WRONG_ROUND";

        case Protocol::MoveStatus::TooSoon:
            return "TOO_SOON";
        }

        return "UNKNOWN";
    }

    DWORD WINAPI ServerRecvThreadProc(LPVOID context)
    {
        auto& session = *static_cast<TestSession*>(context);

        // TCP 수신 누적 버퍼.
        std::array<uint8_t, 4096> buffer{};
        std::size_t used = 0;

        while (!session.stopping.load()) {
            int received = recv(
                session.socket,
                reinterpret_cast<char*>(buffer.data() + used),
                static_cast<int>(buffer.size() - used),
                0
            );

            if (received <= 0) {
                RequestStop(session);
                break;
            }

            used += static_cast<std::size_t>(received);

            std::size_t consumed = 0;

            // 한 번의 recv에 여러 MOVE가 들어온 경우도 처리한다.
            while (used - consumed >= 12) {
                const uint8_t* frame =
                    buffer.data() + consumed;

                uint16_t size = Protocol::Get16(frame);

                // 이 테스트 서버는 MOVE만 받는다.
                if (size != Protocol::MOVE_PACKET_SIZE ||
                    frame[2] != Protocol::VERSION ||
                    frame[3] != Protocol::TYPE_MOVE) {

                    std::printf("[server] invalid MOVE header\n");

                    RequestStop(session);
                    return 0;
                }

                // 본문이 덜 왔으면 다음 recv까지 보존한다.
                if (used - consumed < size)
                    break;

                Protocol::MovePacket packet{};

                std::memcpy(
                    packet.data(),
                    frame,
                    packet.size()
                );

                bool queued = false;

                EnterCriticalSection(&session.commandCs);

                if (!session.stopping.load() &&
                    session.commands.size() < MAX_COMMANDS) {

                    session.commands.push_back(packet);
                    queued = true;
                }

                LeaveCriticalSection(&session.commandCs);

                if (!queued) {
                    RequestStop(session);
                    return 0;
                }

                consumed += size;
            }

            if (consumed > 0) {
                used -= consumed;

                std::memmove(
                    buffer.data(),
                    buffer.data() + consumed,
                    used
                );
            }
        }

        return 0;
    }

    DWORD WINAPI ServerSendThreadProc(LPVOID context)
    {
        auto& session = *static_cast<TestSession*>(context);

        HANDLE events[] = {
            session.stopEvent,
            session.sendEvent
        };

        while (!session.stopping.load()) {
            DWORD waitResult = WaitForMultipleObjects(
                2,
                events,
                FALSE,
                INFINITE
            );

            if (waitResult != WAIT_OBJECT_0 + 1) {
                RequestStop(session);
                break;
            }

            while (!session.stopping.load()) {
                Protocol::ResultPacket response{};
                bool hasResponse = false;

                EnterCriticalSection(&session.sendCs);

                if (!session.stopping.load() &&
                    !session.responses.empty()) {

                    response = session.responses.front();
                    session.responses.pop_front();

                    hasResponse = true;
                }

                LeaveCriticalSection(&session.sendCs);

                if (!hasResponse)
                    break;

                int sent = 0;
                const int length =
                    static_cast<int>(response.size());

                // 부분 송신은 송신 스레드에서 처리한다.
                // GameThread는 여기서 기다리지 않는다.
                while (sent < length) {
                    if (session.stopping.load())
                        return 0;

                    int result = send(
                        session.socket,
                        reinterpret_cast<const char*>(
                            response.data() + sent),
                        length - sent,
                        0
                    );

                    if (result <= 0) {
                        RequestStop(session);
                        return 0;
                    }

                    sent += result;
                }

                EnterCriticalSection(&session.sendCs);

                session.queuedSendBytes -= response.size();

                LeaveCriticalSection(&session.sendCs);
            }
        }

        return 0;
    }

    DWORD WINAPI GameThreadProc(LPVOID context)
    {
        auto& session = *static_cast<TestSession*>(context);

        // 위치와 이동 판정 상태는 GameThread만 변경한다.
        int32_t playerX = 0;
        int32_t playerZ = 0;

        uint32_t lastPacketSequence = 0;
        uint32_t lastInputSequence = 0;
        uint32_t responseSequence = 0;

        uint64_t tick = 0;
        uint64_t nextAllowedMoveTick = 0;

        using Clock = std::chrono::steady_clock;

        const auto started = Clock::now();

        while (!session.stopping.load()) {
            // 시작 시각 기준으로 매 틱의 시각을 계산한다.
            // 33ms를 반복해서 더하는 방식의 누적 오차를 피한다.
            const auto nextTickTime =
                started +
                std::chrono::duration_cast<Clock::duration>(
                    std::chrono::duration<double>(
                        static_cast<double>(tick + 1) /
                        static_cast<double>(Protocol::TICK_HZ)
                    )
                );

            if (Clock::now() < nextTickTime) {
                WaitForSingleObject(session.stopEvent, 1);
                continue;
            }

            ++tick;

            std::deque<Protocol::MovePacket> commands;

            // 이번 틱에서 처리할 명령만 가져온다.
            // 판정과 송신은 이 잠금 밖에서 수행한다.
            EnterCriticalSection(&session.commandCs);

            if (!session.stopping.load())
                commands.swap(session.commands);

            LeaveCriticalSection(&session.commandCs);

            for (const auto& packet : commands) {
                if (session.stopping.load())
                    break;

                uint32_t packetSequence =
                    Protocol::Get32(packet.data() + 4);

                uint32_t roundId =
                    Protocol::Get32(packet.data() + 8);

                uint32_t inputSequence =
                    Protocol::Get32(packet.data() + 12);

                uint8_t directionValue = packet[16];

                Protocol::MoveStatus status =
                    Protocol::MoveStatus::Accepted;

                if (roundId != Protocol::TEST_ROUND_ID) {
                    status = Protocol::MoveStatus::WrongRound;
                }
                else if (
                    packetSequence <= lastPacketSequence ||
                    inputSequence <= lastInputSequence) {

                    status =
                        Protocol::MoveStatus::StaleSequence;
                }
                else {
                    // 정상 라운드의 새 요청은 거절되더라도 소비한다.
                    lastPacketSequence = packetSequence;
                    lastInputSequence = inputSequence;

                    if (directionValue < 1 ||
                        directionValue > 4) {

                        status =
                            Protocol::MoveStatus::InvalidDirection;
                    }
                    else if (tick < nextAllowedMoveTick) {
                        status = Protocol::MoveStatus::TooSoon;
                    }
                    else {
                        int64_t nextX = playerX;
                        int64_t nextZ = playerZ;

                        switch (
                            static_cast<Protocol::Direction>(
                                directionValue)) {

                        case Protocol::Direction::Up:
                            --nextZ;
                            break;

                        case Protocol::Direction::Down:
                            ++nextZ;
                            break;

                        case Protocol::Direction::Left:
                            --nextX;
                            break;

                        case Protocol::Direction::Right:
                            ++nextX;
                            break;
                        }

                        if (
                            nextX < INT32_MIN ||
                            nextX > INT32_MAX ||
                            nextZ < INT32_MIN ||
                            nextZ > INT32_MAX) {

                            status =
                                Protocol::MoveStatus::OutOfBounds;
                        }
                        else {
                            playerX = static_cast<int32_t>(nextX);
                            playerZ = static_cast<int32_t>(nextZ);

                            // 승인했을 때만 이동 간격을 갱신한다.
                            nextAllowedMoveTick =
                                tick +
                                Protocol::MOVE_INTERVAL_TICKS;
                        }
                    }
                }

                Protocol::MoveResult moveResult;

                moveResult.inputSequence = inputSequence;
                moveResult.x = playerX;
                moveResult.z = playerZ;
                moveResult.status = status;

                auto response = Protocol::MakeMoveResult(
                    ++responseSequence,
                    Protocol::TEST_ROUND_ID,
                    moveResult
                );

                bool queued = false;

                EnterCriticalSection(&session.sendCs);

                if (!session.stopping.load() &&
                    session.queuedSendBytes <=
                    MAX_SEND_BYTES - response.size()) {

                    session.responses.push_back(response);
                    session.queuedSendBytes += response.size();

                    SetEvent(session.sendEvent);
                    queued = true;
                }

                LeaveCriticalSection(&session.sendCs);

                if (!queued) {
                    RequestStop(session);
                    break;
                }

                std::printf(
                    "[MOVE] tick=%llu packet=%u input=%u "
                    "position=(%d,%d) %s\n",
                    static_cast<unsigned long long>(tick),
                    static_cast<unsigned>(packetSequence),
                    static_cast<unsigned>(inputSequence),
                    static_cast<int>(playerX),
                    static_cast<int>(playerZ),
                    StatusName(status)
                );
            }

            // 입력이 없어도 이 경로는 매 틱 실행된다.
            // 이후 월드 갱신은 이 GameThread에 통합한다.
            if (tick % Protocol::TICK_HZ == 0) {
                std::printf(
                    "[TICK] tick=%llu position=(%d,%d)\n",
                    static_cast<unsigned long long>(tick),
                    static_cast<int>(playerX),
                    static_cast<int>(playerZ)
                );
            }
        }

        return 0;
    }
}

void ServeClient(SOCKET clientSocket)
{
    TestSession session;
    session.socket = clientSocket;

    InitializeCriticalSection(&session.commandCs);
    InitializeCriticalSection(&session.sendCs);

    session.stopEvent = CreateEvent(
        nullptr,
        TRUE,       // 수동 리셋
        FALSE,
        nullptr
    );

    session.sendEvent = CreateEvent(
        nullptr,
        FALSE,      // 자동 리셋
        FALSE,
        nullptr
    );

    if (!session.stopEvent || !session.sendEvent) {
        std::printf("[server] event creation failed\n");

        if (session.sendEvent)
            CloseHandle(session.sendEvent);

        if (session.stopEvent)
            CloseHandle(session.stopEvent);

        DeleteCriticalSection(&session.sendCs);
        DeleteCriticalSection(&session.commandCs);

        return;
    }

    HANDLE threads[3]{};

    // 송신 스레드를 먼저 만든다.
    threads[0] = CreateThread(
        nullptr,
        0,
        ServerSendThreadProc,
        &session,
        0,
        nullptr
    );

    if (threads[0]) {
        threads[1] = CreateThread(
            nullptr,
            0,
            ServerRecvThreadProc,
            &session,
            0,
            nullptr
        );
    }

    if (threads[1]) {
        threads[2] = CreateThread(
            nullptr,
            0,
            GameThreadProc,
            &session,
            0,
            nullptr
        );
    }

    if (!threads[0] || !threads[1] || !threads[2]) {
        std::printf("[server] thread creation failed\n");
        RequestStop(session);
    }
    else {
        WaitForSingleObject(session.stopEvent, INFINITE);
    }

    RequestStop(session);

    // 잠금을 잡지 않은 상태에서 종료를 기다린다.
    for (HANDLE thread : threads) {
        if (thread) {
            WaitForSingleObject(thread, INFINITE);
            CloseHandle(thread);
        }
    }

    CloseHandle(session.sendEvent);
    CloseHandle(session.stopEvent);

    DeleteCriticalSection(&session.sendCs);
    DeleteCriticalSection(&session.commandCs);
}

int main()
{
    WSADATA wsa{};

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return 1;

    SOCKET listenSocket = socket(
        AF_INET,
        SOCK_STREAM,
        0
    );

    if (listenSocket == INVALID_SOCKET) {
        WSACleanup();
        return 1;
    }

    sockaddr_in address{};

    address.sin_family = AF_INET;
    address.sin_port = htons(9000);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(
        listenSocket,
        reinterpret_cast<sockaddr*>(&address),
        sizeof(address)) == SOCKET_ERROR) {

        std::printf(
            "[server] bind failed: %d\n",
            WSAGetLastError()
        );

        closesocket(listenSocket);
        WSACleanup();

        return 1;
    }

    if (listen(
        listenSocket,
        SOMAXCONN) == SOCKET_ERROR) {

        std::printf(
            "[server] listen failed: %d\n",
            WSAGetLastError()
        );

        closesocket(listenSocket);
        WSACleanup();

        return 1;
    }

    std::printf("[server] listening on port 9000\n");

    while (true) {
        SOCKET clientSocket = accept(
            listenSocket,
            nullptr,
            nullptr
        );

        if (clientSocket == INVALID_SOCKET)
            break;

        // 응답 송신이 무한히 대기하지 않도록 제한한다.
        DWORD sendTimeout = 3000;

        setsockopt(
            clientSocket,
            SOL_SOCKET,
            SO_SNDTIMEO,
            reinterpret_cast<const char*>(&sendTimeout),
            sizeof(sendTimeout)
        );

        std::printf(
            "[server] connected. Position reset to (0,0)\n"
        );

        ServeClient(clientSocket);

        closesocket(clientSocket);

        std::printf(
            "[server] waiting for next client\n"
        );
    }

    closesocket(listenSocket);
    WSACleanup();

    return 0;
}