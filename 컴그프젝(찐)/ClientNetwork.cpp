#include "ClientNetwork.h"
#include <ws2tcpip.h>
#include <iostream>
#include <cmath>

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

    lastState = std::chrono::steady_clock::now();
    return true;
}

ClientNetwork* activeClientNetwork = nullptr;

bool ClientNetwork::Flush() {
    while (!sendBuffer.empty()) {
        const int n = send(serverSocket, reinterpret_cast<const char*>(sendBuffer.data()),
                           static_cast<int>(sendBuffer.size()), 0);
        if (n == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK) return true;
        if (n <= 0) { Disconnect(); return false; }
        sendBuffer.erase(sendBuffer.begin(), sendBuffer.begin() + n);
    }
    return true;
}
bool ClientNetwork::Queue(const std::uint8_t* p, std::size_t size) {
    if (!IsConnected() || sendBuffer.size() + size > 16384) { Disconnect(); return false; }
    sendBuffer.insert(sendBuffer.end(), p, p + size); return Flush();
}
bool ClientNetwork::PollDisconnected() {
    if (!IsConnected()) return false;
    const auto now = std::chrono::steady_clock::now();
    if (!pollingStarted) { lastState = now; pollingStarted = true; }
    if (now - lastState > std::chrono::seconds(5) ||
        (pendingInput && now - pendingSince > std::chrono::seconds(5))) {
        std::cerr << "Server movement response timed out.\n";
        Disconnect(); return true;
    }
    if (!Flush()) return true;
    std::uint8_t buffer[4096];
    const int n = recv(serverSocket, reinterpret_cast<char*>(buffer), sizeof(buffer), 0);
    if (n == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK) return false;
    if (n <= 0 || receiveBuffer.size() + n > 16384) { Disconnect(); return true; }
    receiveBuffer.insert(receiveBuffer.end(), buffer, buffer + n);
    std::size_t offset = 0;
    while (receiveBuffer.size() - offset >= 2) {
        const auto* p = receiveBuffer.data() + offset;
        const std::size_t size = (std::size_t(p[0]) << 8) | p[1];
        if (size < 8 || size > 1024) { Disconnect(); return true; }
        if (receiveBuffer.size() - offset < size) break;
        if (p[2] != 1) { Disconnect(); return true; }
        if (p[3] == Wire::AssignedId) {
            // ID 메시지는 정확히 12바이트여야 함
            if (size != Wire::AssignedIdSize) {
                std::cerr << "Invalid assigned ID packet size."
                    << std::endl;
                Disconnect();
                return true;
            }
            const std::uint32_t receivedId =
                Wire::Read32(p + 8);
            // 0은 미수신 상태로 사용하므로 유효한 ID로 받지 않음.
            // 같은 연결에서 ID가 갑자기 달라지는 것도 허용하지 않음.
            if (receivedId == 0 ||
                (myPlayerId != 0 && myPlayerId != receivedId)) {

                std::cerr << "Invalid assigned player ID."
                    << std::endl;
                Disconnect();
                return true;
            }
            // 처음 받았을 때만 저장하고 출력
            if (myPlayerId == 0) {
                myPlayerId = receivedId;
                std::cout << "My player ID: "
                    << myPlayerId << std::endl;
            }
        }
        else if (p[3] == Wire::State &&
            size == Wire::StateSize) {

            lastState = now;

            const auto round = Wire::Read32(p + 8);

            if (roundId == 0) {
                roundId = round;
            }
            // 현재 라운드에서 내가 기다리던 이동 응답인지 확인
            if (round == roundId &&
                pendingInput &&
                Wire::Read32(p + 12) == pendingInput) {
                pendingInput = 0;
                if (p[28] ==
                    static_cast<std::uint8_t>(
                        Wire::MoveStatus::Accepted)) {
                    // 서버가 승인한 목표 좌표
                    moveX = Wire::ReadFloat(p + 20);
                    moveZ = Wire::ReadFloat(p + 24);
                    moveReady =
                        std::isfinite(moveX) &&
                        std::isfinite(moveZ);
                    // 유효한 승인 좌표와 자기 ID가 있을 때만 출력
                    if (moveReady && myPlayerId != 0) {
                        const auto receivedInputSeq =
                            Wire::Read32(p + 12);
                        const auto serverTick =
                            Wire::Read32(p + 16);
                        std::cout
                            << "[MOVE ACCEPTED]"
                            << " id=" << myPlayerId
                            << " inputSeq=" << receivedInputSeq
                            << " tick=" << serverTick
                            << " x=" << moveX
                            << " z=" << moveZ
                            << std::endl;
                    }
                }
            }
        }
        offset += size;
    }
    receiveBuffer.erase(receiveBuffer.begin(), receiveBuffer.begin() + offset);
    return false;
}
void ClientNetwork::SyncWorld(float x, float z, bool alive) {
    if (!Ready() || pendingInput || moveReady) return;
    std::array<std::uint8_t, Wire::PositionSize> p{};
    Wire::Header(p.data(), p.size(), Wire::WorldPosition, ++sequence);
    Wire::Write32(p.data() + 8, roundId);
    Wire::WriteFloat(p.data() + 12, x); Wire::WriteFloat(p.data() + 16, z);
    p[20] = alive ? 1 : 0; Queue(p.data(), p.size());
}
bool ClientNetwork::RequestMove(Direction direction, float x, float z) {
    if (!Ready() || pendingInput || moveReady) return false;
    SyncWorld(x, z, true);
    const auto p = EncodeMovePacket(++sequence, roundId, ++inputSeq, direction);
    pendingInput = inputSeq; pendingSince = std::chrono::steady_clock::now();
    return Queue(p.data(), p.size());
}
bool ClientNetwork::TakeMove(float& x, float& z) {
    if (!moveReady) return false;
    x = moveX; z = moveZ; moveReady = false; return true;
}
void ClientNetwork::ResetRound() {
    if (!Ready()) return;
    std::array<std::uint8_t, 12> p{};
    Wire::Header(p.data(), p.size(), Wire::Reset, ++sequence);
    Wire::Write32(p.data() + 8, ++roundId);
    inputSeq = pendingInput = 0; moveReady = false;
    Queue(p.data(), p.size());
}

void ClientNetwork::Disconnect() {
    myPlayerId = 0;
    receiveBuffer.clear(); 
    sendBuffer.clear();
    sequence = inputSeq = pendingInput = roundId = 0; pollingStarted = moveReady = false;
    if (serverSocket != INVALID_SOCKET) {
        closesocket(serverSocket);
        serverSocket = INVALID_SOCKET;
    }

    if (winsockInitialized) {
        WSACleanup();
        winsockInitialized = false;
    }
}
