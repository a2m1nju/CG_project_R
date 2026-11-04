#include "MoveProtocol.h"
#include <ws2tcpip.h>
#include <cstdio>

#define SERVERPORT 9000

// 같은 PC에서 서버와 클라이언트를 실행하는 테스트 주소
const char* SERVERIP = "127.0.0.1";

int main()
{
    WSADATA wsa;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("윈속 초기화 실패\n");
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock == INVALID_SOCKET) {
        printf("socket 오류: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(SERVERPORT);

    if (inet_pton(
        AF_INET,
        SERVERIP,
        &serverAddress.sin_addr) != 1) {

        printf("서버 IP 주소가 올바르지 않습니다.\n");
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    if (connect(
        sock,
        reinterpret_cast<sockaddr*>(&serverAddress),
        sizeof(serverAddress)) == SOCKET_ERROR) {

        printf("connect 오류: %d\n", WSAGetLastError());
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    printf("[클라이언트] 서버 연결 성공\n");

    const Protocol::Direction directions[] = {
     Protocol::Direction::Up,
     Protocol::Direction::Up,
     Protocol::Direction::Right,
     Protocol::Direction::Down,
     Protocol::Direction::Left
    };

    uint32_t packetSequence = 0;
    uint32_t inputSequence = 0;
    const uint32_t roundId = 1;
    bool success = true;

    for (auto direction : directions) {
        auto packet = Protocol::MakeMovePacket(
            ++packetSequence,
            roundId,
            ++inputSequence,
            direction
        );

        const int length = static_cast<int>(packet.size());
        const int sent = send(
            sock,
            reinterpret_cast<const char*>(packet.data()),
            length,
            0
        );

        if (sent != length) {

            success = false;
            break;
        }

        printf("[SEND] packet=%u input=%u direction=%u\n",
            static_cast<unsigned>(packetSequence),
            static_cast<unsigned>(inputSequence),
            static_cast<unsigned>(direction));
    }

    // 송신 종료를 서버에 알린다.
    shutdown(sock, SD_SEND);

    closesocket(sock);
    WSACleanup();

    return success ? 0 : 1;
}