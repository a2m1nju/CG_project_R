#include "ClientNetwork.h"
#include <ws2tcpip.h>
#include <iostream>

#pragma comment(lib, "Ws2_32.lib")

bool ClientNetwork::Connect(const char* serverIp, std::uint16_t port) {
    if (winsockInitialized || serverIp == nullptr || port == 0) {
        return false;
    }

    WSADATA wsaData{};
    const int startupError = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (startupError != 0) {
        std::cerr << "WSAStartup failed: " << startupError << '\n';
        return false;
    }
    winsockInitialized = true;

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);

    if (InetPtonA(AF_INET, serverIp, &address.sin_addr) != 1) {
        std::cerr << "Invalid IPv4 address: " << serverIp << '\n';
        Disconnect();
        return false;
    }

    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "socket failed: " << WSAGetLastError() << '\n';
        Disconnect();
        return false;
    }

    if (connect(serverSocket, reinterpret_cast<const sockaddr*>(&address),
                sizeof(address)) == SOCKET_ERROR) {
        std::cerr << "connect failed for " << serverIp << ':' << port
                  << " (error " << WSAGetLastError() << ")\n";
        Disconnect();
        return false;
    }

    u_long nonBlocking = 1;
    if (ioctlsocket(serverSocket, FIONBIO, &nonBlocking) == SOCKET_ERROR) {
        std::cerr << "ioctlsocket failed: " << WSAGetLastError() << '\n';
        Disconnect();
        return false;
    }

    return true;
}

bool ClientNetwork::PollDisconnected() {
    if (serverSocket == INVALID_SOCKET) {
        return false;
    }

    char byte;
    const int received = recv(serverSocket, &byte, sizeof(byte), MSG_PEEK);
    if (received > 0) {
        return false;
    }
    if (received == SOCKET_ERROR) {
        const int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK) {
            return false;
        }
        std::cerr << "recv failed: " << error << '\n';
    }

    Disconnect();
    return true;
}

void ClientNetwork::Disconnect() {
    if (serverSocket != INVALID_SOCKET) {
        closesocket(serverSocket);
        serverSocket = INVALID_SOCKET;
    }

    if (winsockInitialized) {
        WSACleanup();
        winsockInitialized = false;
    }
}
