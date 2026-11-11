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
        if (p[3] == Wire::WorldBegin && size == Wire::WorldBeginSize) {
            const auto round = Wire::Read32(p + 8);
            const auto generation = Wire::Read32(p + 12);
            const auto minZ = static_cast<std::int32_t>(Wire::Read32(p + 16));
            const auto maxZ = static_cast<std::int32_t>(Wire::Read32(p + 20));
            if (round == 0 || generation == 0 || minZ > maxZ ||
                static_cast<std::int64_t>(maxZ) - minZ > 4096) {
                Disconnect(); return true;
            }
            if (roundId != 0 && !Wire::Newer(round, roundId) && round != roundId) {
                offset += size;
                continue;
            }
            if (round == roundId && generation < worldGeneration) {
                offset += size;
                continue;
            }
            if (round != roundId || generation != worldGeneration) {
                roundId = round;
                worldGeneration = generation;
                initialMinZ = minZ;
                initialMaxZ = maxZ;
                initialLanes.clear();
                pendingInput = inputSeq = 0;
                moveReady = worldReady = resetRequested = false;
                worldEvents.clear();
                WorldEvent event{};
                event.kind = WorldEvent::Kind::Begin;
                event.roundId = round;
                event.generation = generation;
                event.minZ = minZ;
                event.maxZ = maxZ;
                worldEvents.push_back(event);
            }
        } else if (p[3] == Wire::WorldLane && size == Wire::WorldLaneSize) {
            const auto round = Wire::Read32(p + 8);
            const auto generation = Wire::Read32(p + 12);
            if (round == roundId && generation == worldGeneration) {
                WorldEvent event{};
                event.kind = WorldEvent::Kind::Lane;
                event.roundId = round;
                event.generation = generation;
                event.z = static_cast<std::int32_t>(Wire::Read32(p + 16));
                event.terrain = p[20];
                event.treeMask = Wire::Read32(p + 21);
                event.padMask = Wire::Read32(p + 25);
                event.spawnMask = Wire::Read32(p + 29);
                if (event.terrain > 3 ||
                    ((event.treeMask | event.padMask | event.spawnMask) & 0x80000000u) != 0) {
                    Disconnect(); return true;
                }
                if (!worldReady && event.z >= initialMinZ && event.z <= initialMaxZ)
                    initialLanes.insert(event.z);
                worldEvents.push_back(event);
            }
        } else if (p[3] == Wire::WorldActor && size == Wire::WorldActorSize) {
            const auto round = Wire::Read32(p + 8);
            const auto generation = Wire::Read32(p + 12);
            if (round == roundId && generation == worldGeneration) {
                WorldEvent event{};
                event.kind = WorldEvent::Kind::Actor;
                event.roundId = round;
                event.generation = generation;
                event.tick = Wire::Read32(p + 16);
                event.actorId = Wire::Read32(p + 20);
                event.actorKind = p[24];
                event.actorZ = Wire::ReadFloat(p + 25);
                event.x = Wire::ReadFloat(p + 29);
                event.speed = Wire::ReadFloat(p + 33);
                event.width = Wire::ReadFloat(p + 37);
                event.red = p[41]; event.green = p[42]; event.blue = p[43];
                event.design = p[44]; event.trainState = p[45];
                event.trainTimer = (std::uint16_t(p[46]) << 8) | p[47];
                event.trainLight = p[48];
                if (event.actorId == 0 || event.actorKind < 1 || event.actorKind > 3 ||
                    event.design > 3 || event.trainState > 2 || event.trainLight > 1 ||
                    !std::isfinite(event.actorZ) || !std::isfinite(event.x) ||
                    !std::isfinite(event.speed) || !std::isfinite(event.width)) {
                    Disconnect(); return true;
                }
                worldEvents.push_back(event);
            }
        } else if (p[3] == Wire::WorldEnd && size == Wire::WorldEndSize) {
            const auto round = Wire::Read32(p + 8);
            const auto generation = Wire::Read32(p + 12);
            if (round == roundId && generation == worldGeneration) {
                if (initialLanes.size() !=
                    static_cast<std::size_t>(static_cast<std::int64_t>(initialMaxZ) - initialMinZ + 1)) {
                    Disconnect(); return true;
                }
                WorldEvent event{};
                event.kind = WorldEvent::Kind::End;
                event.roundId = round;
                event.generation = generation;
                worldEvents.push_back(event);
            }
        } else if (p[3] == Wire::State && size == Wire::StateSize) {
            lastState = now;
            const auto round = Wire::Read32(p + 8);
            if (round == roundId && pendingInput && Wire::Read32(p + 12) == pendingInput) {
                pendingInput = 0;
                if (p[28] == static_cast<std::uint8_t>(Wire::MoveStatus::Accepted)) {
                    moveX = Wire::ReadFloat(p + 20); moveZ = Wire::ReadFloat(p + 24);
                    moveReady = std::isfinite(moveX) && std::isfinite(moveZ);
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
bool ClientNetwork::TakeWorldEvent(WorldEvent& event) {
    if (worldEvents.empty()) return false;
    event = worldEvents.front();
    worldEvents.pop_front();
    if (event.kind == WorldEvent::Kind::End) worldReady = true;
    return true;
}
void ClientNetwork::ResetRound() {
    if (!Ready() || resetRequested) return;
    std::array<std::uint8_t, 12> p{};
    Wire::Header(p.data(), p.size(), Wire::Reset, ++sequence);
    Wire::Write32(p.data() + 8, roundId + 1);
    resetRequested = Queue(p.data(), p.size());
}

void ClientNetwork::Disconnect() {
    receiveBuffer.clear(); sendBuffer.clear();
    sequence = inputSeq = pendingInput = roundId = worldGeneration = 0;
    initialMinZ = 0; initialMaxZ = -1; initialLanes.clear();
    pollingStarted = moveReady = worldReady = resetRequested = false;
    worldEvents.clear();
    if (serverSocket != INVALID_SOCKET) {
        closesocket(serverSocket);
        serverSocket = INVALID_SOCKET;
    }

    if (winsockInitialized) {
        WSACleanup();
        winsockInitialized = false;
    }
}
