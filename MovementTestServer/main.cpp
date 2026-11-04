#include "MoveProtocol.h"

#include <cstdio>

bool SendAll(
    SOCKET socket,
    const uint8_t* data,
    int length)
{
    int sent = 0;

    while (sent < length) {
        int result = send(
            socket,
            reinterpret_cast<const char*>(data + sent),
            length - sent,
            0
        );

        if (result <= 0)
            return false;

        sent += result;
    }

    return true;
}

// 1: 성공
// 0: 수신 시작 전 정상 종료
// -1: 오류 또는 수신 도중 종료
int RecvExact(
    SOCKET socket,
    uint8_t* data,
    int length)
{
    int received = 0;

    while (received < length) {
        int result = recv(
            socket,
            reinterpret_cast<char*>(data + received),
            length - received,
            0
        );

        if (result == SOCKET_ERROR)
            return -1;

        if (result == 0)
            return received == 0 ? 0 : -1;

        received += result;
    }

    return 1;
}

void ServeClient(SOCKET clientSocket)
{
    // 새 클라이언트는 원점에서 시작한다.
    int32_t playerX = 0;
    int32_t playerZ = 0;

    uint32_t lastPacketSequence = 0;
    uint32_t lastInputSequence = 0;

    uint32_t responseSequence = 0;

    while (true) {
        Protocol::MovePacket packet{};

        // 먼저 공통 헤더 12바이트를 받는다.
        int result = RecvExact(
            clientSocket,
            packet.data(),
            12
        );

        if (result != 1) {
            std::printf("[server] client closed or receive failed\n");
            break;
        }

        uint16_t size =
            Protocol::Get16(packet.data());

        uint8_t version = packet[2];
        uint8_t type = packet[3];

        uint32_t packetSequence =
            Protocol::Get32(packet.data() + 4);

        uint32_t roundId =
            Protocol::Get32(packet.data() + 8);

        if (size != Protocol::MOVE_PACKET_SIZE ||
            version != Protocol::VERSION ||
            type != Protocol::TYPE_MOVE ||
            roundId != Protocol::TEST_ROUND_ID) {

            std::printf("[server] invalid header\n");
            break;
        }

        // MOVE 본문 5바이트를 받는다.
        if (RecvExact(
            clientSocket,
            packet.data() + 12,
            5) != 1) {

            std::printf("[server] incomplete body\n");
            break;
        }

        uint32_t inputSequence =
            Protocol::Get32(packet.data() + 12);

        uint8_t directionValue = packet[16];

        if (packetSequence <= lastPacketSequence ||
            inputSequence <= lastInputSequence ||
            directionValue < 1 ||
            directionValue > 4) {

            std::printf("[server] invalid input\n");
            break;
        }

        lastPacketSequence = packetSequence;
        lastInputSequence = inputSequence;

        auto direction =
            static_cast<Protocol::Direction>(
                directionValue
                );

        int32_t nextX = playerX;
        int32_t nextZ = playerZ;

        switch (direction) {
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

        Protocol::MoveStatus status =
            Protocol::MoveStatus::Accepted;

        // 이동 승인/거절 확인을 위한 임시 경계.
        if (nextX < -2 || nextX > 2 ||
            nextZ < -100 || nextZ > 100) {

            status =
                Protocol::MoveStatus::OutOfBounds;
        }
        else {
            playerX = nextX;
            playerZ = nextZ;
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

        if (!SendAll(
            clientSocket,
            response.data(),
            static_cast<int>(response.size()))) {

            std::printf("[server] send failed\n");
            break;
        }

        std::printf(
            "[MOVE] input=%u direction=%u position=(%d,%d) %s\n",
            static_cast<unsigned>(inputSequence),
            static_cast<unsigned>(directionValue),
            static_cast<int>(playerX),
            static_cast<int>(playerZ),
            status == Protocol::MoveStatus::Accepted
            ? "ACCEPTED"
            : "REJECTED"
        );
    }
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