#include "ClientNetwork.h"
#include <ws2tcpip.h>
#include <iostream>
#include <cmath>
#include <utility>

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
bool ClientNetwork::RequestJoin(const std::string& nickname)
{
    if (!IsConnected()) {
        lobby.message = "NOT CONNECTED TO SERVER";
        return false;
    }
    if (lobby.stage != LobbyStage::Title) {
        return false;
    }
    if (nickname.empty() || nickname.size() > 24) {
        return false;
    }
    for (const unsigned char ch : nickname) {
        if (ch < 32 || ch > 126) {
            return false;
        }
    }
    auto packet = LobbyWire::MakePacket(
        LobbyWire::Join, 1 + nickname.size(), ++sequence);
    packet[12] =
        static_cast<std::uint8_t>(nickname.size());
    for (std::size_t i = 0; i < nickname.size(); ++i) {
        packet[13 + i] =
            static_cast<std::uint8_t>(nickname[i]);
    }
    lobby = LobbyView{};
    lobby.stage = LobbyStage::Joining;
    lobby.message = "JOINING...";
    return Queue(packet.data(), packet.size());
}
bool ClientNetwork::RequestReady(bool ready)
{
    if (!IsConnected() ||
        lobby.stage != LobbyStage::Ready ||
        !lobby.joined ||
        lobby.readyPending ||
        lobby.leaving)
    {
        return false;
    }
    auto packet =
        LobbyWire::MakePacket(LobbyWire::Ready, 5, ++sequence);
    Wire::Write32(packet.data() + 12, lobby.epoch);
    packet[16] = ready ? 1 : 0;
    lobby.readyPending = true;
    return Queue(packet.data(), packet.size());
}
bool ClientNetwork::RequestLeave()
{
    if (!IsConnected() ||
        lobby.stage == LobbyStage::Title ||
        lobby.leaving)
    {
        return false;
    }
    auto packet =
        LobbyWire::MakePacket(LobbyWire::Leave, 1, ++sequence);
    packet[12] = 0;
    // 서버의 ROOM_CLOSE를 받은 뒤 타이틀로 복귀.
    lobby.leaving = true;
    lobby.message = "LEAVING...";

    return Queue(packet.data(), packet.size());
}
int ClientNetwork::HandleLobbyPacket(
    const std::uint8_t* packet,
    std::size_t size)
{
    const auto type = packet[3];
    if (type != LobbyWire::Welcome &&
        type != LobbyWire::QueueState &&
        type != LobbyWire::ReadyLobbyState &&
        type != LobbyWire::RoomClose &&
        type != LobbyWire::Error)
    {
        return 0;
    }
    if (Wire::ReadRoundId(packet) != 0) {
        return -1;
    }
    if (type == LobbyWire::Welcome) {
        if (size != 20) {
            return -1;
        }
        const auto playerId = Wire::Read32(packet + 16);
        if (playerId == 0 ||
            (myPlayerId != 0 && myPlayerId != playerId))
        {
            return -1;
        }
        if (lobby.stage == LobbyStage::Joining) {
            myPlayerId = playerId;
            lobby.joined = true;

            std::cout << "JOIN accepted. player="
                << myPlayerId << '\n';
        }
        return 1;
    }
    if (type == LobbyWire::QueueState) {
        if (size != 17 || packet[16] != 0) {
            return -1;
        }
        const auto position = LobbyWire::Read16(packet + 12);
        const auto count = LobbyWire::Read16(packet + 14);
        if (count < 1 || count > 3 ||
            position < 1 || position > count)
        {
            return -1;
        }
        if (!lobby.joined ||
            lobby.stage == LobbyStage::Title ||
            lobby.leaving)
        {
            return 1;
        }
        lobby.stage = LobbyStage::Matching;
        lobby.position = position;
        lobby.count = count;
        lobby.members.clear();
        lobby.message = "WAITING FOR PLAYERS";
        return 1;
    }
    if (type == LobbyWire::ReadyLobbyState) {
        if (size < 18 || packet[16] != 0 || packet[17] != 3) {
            return -1;
        }
        const auto epoch = Wire::Read32(packet + 12);
        std::vector<LobbyMemberView> members;
        std::size_t offset = 18;
        for (int i = 0; i < 3; ++i) {
            if (offset + 7 > size) {
                return -1;
            }
            LobbyMemberView member;
            member.id = Wire::Read32(packet + offset);
            const auto ready = packet[offset + 5];
            const std::size_t nameLength = packet[offset + 6];
            if (member.id == 0 ||
                ready > 1 ||
                nameLength > 24 ||
                offset + 7 + nameLength > size)
            {
                return -1;
            }
            for (const auto& previous : members) {
                if (previous.id == member.id) {
                    return -1;
                }
            }
            member.ready = ready != 0;
            member.nickname.assign(
                reinterpret_cast<const char*>(packet + offset + 7),
                nameLength);

            members.push_back(std::move(member));
            offset += 7 + nameLength;
        }

        if (offset != size) {
            return -1;
        }

        bool containsMe = false;
        bool allReady = true;

        for (const auto& member : members) {
            containsMe = containsMe || member.id == myPlayerId;
            allReady = allReady && member.ready;
        }

        if (!containsMe) {
            return -1;
        }

        if (!lobby.joined ||
            lobby.stage == LobbyStage::Title ||
            lobby.leaving)
        {
            return 1;
        }

        if (lobby.epoch != 0 &&
            epoch != lobby.epoch &&
            !Wire::Newer(epoch, lobby.epoch))
        {
            return 1;
        }

        lobby.stage = LobbyStage::Ready;
        lobby.epoch = epoch;
        lobby.count = 3;
        lobby.members = std::move(members);
        lobby.readyPending = false;

        lobby.message = allReady
            ? "ALL READY - WAITING FOR INIT"
            : "WAITING FOR READY";

        return 1;
    }

    if (type == LobbyWire::RoomClose) {
        if (size != 13) {
            return -1;
        }
        ClearWorld();
        lobby = LobbyView{};
        lobby.message = "ROOM CLOSED. JOIN AGAIN TO PARTICIPATE.";

        return 1;
    }

    // ERROR
    if (size != 19) {
        return -1;
    }

    const auto code = LobbyWire::Read16(packet + 12);

    if (lobby.stage == LobbyStage::Joining && !lobby.joined) {
        lobby = LobbyView{};
    }

    lobby.readyPending = false;
    lobby.leaving = false;

    if (code == LobbyWire::Busy) {
        lobby.message = "ROOM IS FULL OR UNAVAILABLE";
    }
    else if (code == LobbyWire::Duplicate) {
        lobby.message = "ALREADY JOINED";
    }
    else if (code == LobbyWire::Stale) {
        lobby.message = "OLD LOBBY REQUEST REJECTED";
    }
    else {
        lobby.message = "REQUEST NOT ALLOWED IN THIS PHASE";
    }

    return 1;
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
        if (size < Wire::HeaderSize || size > 1024) { Disconnect(); return true; }
        if (receiveBuffer.size() - offset < size) break;
        if (p[2] != 1) { Disconnect(); return true; }
        const int worldResult = HandleWorldPacket(p, size);
        const int lobbyResult = worldResult != 0 ? worldResult : HandleLobbyPacket(p, size);
        if (lobbyResult < 0) {
            Disconnect();
            return true;
        }

        // 대기방 응답도 서버 수신 시간에 반영.
        lastState = now;

        if (lobbyResult == 0 && p[3] == Wire::AssignedId) {
            // ID 메시지는 정확히 12바이트여야 함
            if (size != Wire::AssignedIdSize) {
                std::cerr << "Invalid assigned ID packet size." << std::endl;
                Disconnect();
                return true;
            }
            // ID 메시지는 공통 헤더 12바이트 + 플레이어 ID 4바이트
            const std::uint32_t receivedId = Wire::Read32(p + Wire::HeaderSize);
            if (receivedId == 0 ||
                (myPlayerId != 0 && myPlayerId != receivedId)) {
                std::cerr << "Invalid assigned player ID."  << std::endl;
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
        else if (lobbyResult == 0 && p[3] == Wire::State && size == Wire::StateSize) {

            lastState = now;
            const auto round = Wire::ReadRoundId(p);
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
    Wire::Header(p.data(), p.size(), Wire::WorldPosition, ++sequence, roundId);
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
}

void ClientNetwork::Disconnect() {
    ClearWorld();
    lobby = LobbyView{};
    lobby.message = "SERVER DISCONNECTED";
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
