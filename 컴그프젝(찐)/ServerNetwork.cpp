#include "ServerNetwork.h"
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

void ServerNetwork::RunAcceptLoop() {
    while (!stopping.load()) {
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(listenSocket, &readable);

        // 종료 요청을 확인할 수 있도록 최대 100ms만 대기
        timeval timeout{0, 100000};
        const int ready = select(0, &readable, nullptr, nullptr, &timeout);
        if (ready == SOCKET_ERROR) {
            std::cerr << "select failed: " << WSAGetLastError() << '\n';
            return;
        }
        if (ready == 0 || stopping.load()) {
            continue;
        }

        const SOCKET clientSocket = accept(listenSocket, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET) {
            const int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK || error == WSAECONNRESET) {
                continue;
            }
            std::cerr << "accept failed: " << error << '\n';
            return;
        }
        if (sessionCount == sessions.size()) {
            closesocket(clientSocket);
            continue;
        }

        const auto id = nextSessionId++;
        sessions[sessionCount++] = {id, clientSocket};
        std::cout << "TCP session connected: " << id << std::endl;
    }
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
