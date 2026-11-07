#include "MoveProtocol.h"

#include <cstdio>

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
        int result = recv(
            clientSocket,
            reinterpret_cast<char*>(packet.data()),
            12,
            MSG_WAITALL
        );

        if (result != 12) {
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
        if (recv(clientSocket, reinterpret_cast<char*>(packet.data() + 12), 5, MSG_WAITALL) != 5) {
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

        int64_t nextX = playerX;
        int64_t nextZ = playerZ;

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

        Protocol::MoveStatus status = Protocol::MoveStatus::Accepted;
        if (nextX < INT32_MIN || nextX > INT32_MAX ||
            nextZ < INT32_MIN || nextZ > INT32_MAX) {

            status = Protocol::MoveStatus::OutOfBounds;
        }
        else {
            playerX = static_cast<int32_t>(nextX);
            playerZ = static_cast<int32_t>(nextZ);
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

        const int length = static_cast<int>(response.size());
        const int sent = send(
            clientSocket,
            reinterpret_cast<const char*>(response.data()),
            length,
            0
        );

        if (sent != length) {

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