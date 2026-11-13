#include "ServerNetwork.h"
#include <cstring>
#include <chrono>
#include <cmath>
#include <iostream>

#pragma comment(lib, "Ws2_32.lib")

bool ServerNetwork::Initialize(std::uint16_t port) {
    if (winsockInitialized || port == 0) {
        return false;
    }

    WSADATA wsaData{};
    const int startupError = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (startupError != 0) {
        std::cerr << "WSAStartup failed: " << startupError << '\n';
        return false;
    }
    winsockInitialized = true;

    listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        std::cerr << "socket failed: " << WSAGetLastError() << '\n';
        Shutdown();
        return false;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listenSocket, reinterpret_cast<const sockaddr*>(&address),
             sizeof(address)) == SOCKET_ERROR) {
        std::cerr << "bind failed on port " << port
                  << ": " << WSAGetLastError() << '\n';
        Shutdown();
        return false;
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "listen failed: " << WSAGetLastError() << '\n';
        Shutdown();
        return false;
    }

    u_long nonBlocking = 1;
    if (ioctlsocket(listenSocket, FIONBIO, &nonBlocking) == SOCKET_ERROR) {
        std::cerr << "ioctlsocket failed: " << WSAGetLastError() << '\n';
        Shutdown();
        return false;
    }

    stopping.store(false);
    return true;
}

bool ServerNetwork::Flush(TransportSession& session) {
    while (!session.sendBuffer.empty()) {
        const int n = send(session.socket, reinterpret_cast<const char*>(session.sendBuffer.data()),
                           static_cast<int>(session.sendBuffer.size()), 0);
        if (n == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK) return true;
        if (n <= 0) return false;
        session.sendBuffer.erase(session.sendBuffer.begin(), session.sendBuffer.begin() + n);
    }
    return true;
}

bool ServerNetwork::ProcessPackets(TransportSession& s, std::uint64_t tick) {
    auto& bytes = s.receiveBuffer;
    std::size_t offset = 0;
    while (bytes.size() - offset >= 2) {
        const auto* p = bytes.data() + offset;
        const std::size_t size = (std::size_t(p[0]) << 8) | p[1];
        if (size < Wire::HeaderSize || size > 1024) return false;
        if (bytes.size() - offset < size) break; // TCP fragment
        if (p[2] != 1) return false;
        const auto seq = Wire::Read32(p + 4);
        const auto roundId = Wire::ReadRoundId(p);
        if (Wire::Newer(seq, s.sequence)) {
            s.sequence = seq;
            if (p[3] == Wire::Reset && size == Wire::HeaderSize) {
                if (roundId == s.roundId + 1) {
                    ++s.roundId; s.x = s.z = 0; s.alive = true;
                    s.inputSeq = 0; s.nextMoveTick = 0; s.status = Wire::MoveStatus::Idle;
                }
            } else if (p[3] == Wire::WorldPosition && size == Wire::PositionSize) {
                // World, logs, banana displacement and death still belong to m_merge's local world.
                const float x = Wire::ReadFloat(p + 12), z = Wire::ReadFloat(p + 16);
                if (roundId == s.roundId && std::isfinite(x) && std::isfinite(z) &&
                    std::abs(x) < 1000000 && std::abs(z) < 1000000 && p[20] <= 1) {
                    s.x = x; s.z = z; s.alive = p[20] != 0;
                }
            } else if (p[3] == 9 && size == 17) {
                const auto input = Wire::Read32(p + 12);
                if (roundId == s.roundId && Wire::Newer(input, s.inputSeq)) {
                    s.inputSeq = input;
                    if (!s.alive || p[16] > 3) s.status = Wire::MoveStatus::Invalid;
                    else if (tick < s.nextMoveTick) s.status = Wire::MoveStatus::Cooldown;
                    else {
                        s.x = std::round(s.x); s.z = std::round(s.z);
                        switch (static_cast<Direction>(p[16])) {
                        case Direction::UP: --s.z; break;
                        case Direction::DOWN: ++s.z; break;
                        case Direction::LEFT: --s.x; break;
                        case Direction::RIGHT: ++s.x; break;
                        }
                        s.nextMoveTick = tick + Wire::MoveCooldownTicks;
                        s.status = Wire::MoveStatus::Accepted;
                    }
                }
            } else return false;
        }
        offset += size;
    }
    bytes.erase(bytes.begin(), bytes.begin() + offset);
    return true;
}

void ServerNetwork::Tick(std::uint64_t tick) {
    for (std::size_t i = sessionCount; i > 0; --i) {
        auto& s = sessions[i - 1];
        if (!ProcessPackets(s, tick) || s.sendBuffer.size() + Wire::StateSize > 16384) {
            RemoveSession(i - 1); continue;
        }
        std::array<std::uint8_t, Wire::StateSize> packet{}; // 29바이트 = 헤더 12 + 본문 17
        Wire::Header(packet.data(), packet.size(), Wire::State, ++s.sendSeq, s.roundId); // 0~11: 서버 송신 순번과 경기 번호를 포함한 공통 헤더
        Wire::Write32(packet.data() + 12, s.inputSeq); // 12~15: 마지막으로 처리한 이동 요청의 순번
        Wire::Write32(packet.data() + 16, static_cast<std::uint32_t>(tick)); // 16~19: 서버 틱의 하위 32비트
        Wire::WriteFloat(packet.data() + 20, s.x); Wire::WriteFloat(packet.data() + 24, s.z); // 20~23: X, 24~27: Z
        packet[28] = static_cast<std::uint8_t>(s.status); // 28: 이동 처리 결과 (승인, 이동 간격 제한 등)
        s.sendBuffer.insert(s.sendBuffer.end(), packet.begin(), packet.end());
        if (tick % Wire::TickRate == 0) {
            std::cout << "[TICK " << tick << "] player=" << s.id << " round=" << s.roundId
                << " x=" << s.x << " z=" << s.z << " alive=" << s.alive << '\n';
        }
        if (!Flush(s)) RemoveSession(i - 1);
    }
    std::cout.flush();
}

void ServerNetwork::RunAcceptLoop() {
    using Clock = std::chrono::steady_clock;
    const auto interval = std::chrono::nanoseconds(1000000000 / Wire::TickRate);
    auto nextTick = Clock::now() + interval;
    std::uint64_t tick = 0;
    while (!stopping.load()) {
        fd_set readable, writable;
        FD_ZERO(&readable); FD_ZERO(&writable); FD_SET(listenSocket, &readable);
        for (std::size_t i = 0; i < sessionCount; ++i) {
            FD_SET(sessions[i].socket, &readable);
            if (!sessions[i].sendBuffer.empty()) FD_SET(sessions[i].socket, &writable);
        }
        const auto remaining = std::chrono::duration_cast<std::chrono::microseconds>(nextTick - Clock::now()).count();
        timeval timeout{0, static_cast<long>(remaining > 0 ? remaining : 0)};
        const int ready = select(0, &readable, &writable, nullptr, &timeout);
        if (ready == SOCKET_ERROR) {
            std::cerr << "select failed: " << WSAGetLastError() << '\n'; return;
        }
        if (stopping.load()) break;
        for (std::size_t i = sessionCount; i > 0; --i) {
            auto& s = sessions[i - 1];
            if (FD_ISSET(s.socket, &writable) && !Flush(s)) { RemoveSession(i - 1); continue; }
            if (!FD_ISSET(s.socket, &readable)) continue;
            std::uint8_t buffer[1024];
            const int n = recv(s.socket, reinterpret_cast<char*>(buffer), sizeof(buffer), 0);
            if (n == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK) continue;
            if (n <= 0 || s.receiveBuffer.size() + n > 16384) { RemoveSession(i - 1); continue; }
            s.receiveBuffer.insert(s.receiveBuffer.end(), buffer, buffer + n);
        }
        if (FD_ISSET(listenSocket, &readable)) {
            const SOCKET client = accept(listenSocket, nullptr, nullptr);
            if (client != INVALID_SOCKET) {
                u_long nonBlocking = 1;
                if (sessionCount == sessions.size() || ioctlsocket(client, FIONBIO, &nonBlocking) == SOCKET_ERROR)
                    closesocket(client);
                else {
                    auto& s = sessions[sessionCount++]; s = TransportSession{};
                    s.id = nextSessionId++; s.socket = client;
                    std::cout << "TCP session connected: " << s.id << std::endl;
                }
            }
        }
        // Fixed-step catch-up; I/O and game state share this one owning thread.
        while (!stopping.load() && Clock::now() >= nextTick) {
            Tick(++tick); nextTick += interval;
        }
    }
}

void ServerNetwork::RemoveSession(std::size_t index) {
    const auto id = sessions[index].id;
    closesocket(sessions[index].socket);
    --sessionCount;
    if (index != sessionCount) {
        sessions[index] = sessions[sessionCount];
    }
    sessions[sessionCount] = TransportSession{};
    std::cout << "TCP session disconnected: " << id << std::endl;
}

void ServerNetwork::StopAccepting() {
    stopping.store(true);
}

void ServerNetwork::Shutdown() {
    // 접속 대기 스레드가 끝난 뒤 등록된 연결 정리
    for (std::size_t i = 0; i < sessionCount; ++i) {
        closesocket(sessions[i].socket);
        sessions[i] = TransportSession{};
    }
    sessionCount = 0;
    nextSessionId = 1;

    if (listenSocket != INVALID_SOCKET) {
        closesocket(listenSocket);
        listenSocket = INVALID_SOCKET;
    }

    if (winsockInitialized) {
        WSACleanup();
        winsockInitialized = false;
    }
}
