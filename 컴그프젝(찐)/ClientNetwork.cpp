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
    return SendJoin("Player"); //닉네임 입력 UI 전까지 테스트용 이름 사용
}

bool ClientNetwork::SendJoin(const std::string& nickname) {
    if (nickname.empty() || nickname.size() > Wire::MaxNicknameBytes) return false;

    std::vector<std::uint8_t> packet(Wire::HeaderSize + 1 + nickname.size());
    Wire::Header(packet.data(), packet.size(), Wire::Join, ++sequence, 0);
    packet[Wire::HeaderSize] = static_cast<std::uint8_t>(nickname.size());
    for (std::size_t i = 0; i < nickname.size(); ++i) {
        packet[Wire::HeaderSize + 1 + i] = static_cast<std::uint8_t>(nickname[i]);
    }
    return Queue(packet.data(), packet.size());
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
bool ClientNetwork::ApplyMatchState(const std::uint8_t* p, std::size_t size) {
    if (!joined || Wire::ReadRoundId(p) != 0) return false;
    if (p[3] == Wire::QueueState) {
        if (size != Wire::QueueStateSize || p[16] != Wire::MatchingPhase) return false;
        const auto position = (std::uint16_t(p[12]) << 8) | p[13];
        const auto count = (std::uint16_t(p[14]) << 8) | p[15];
        if (position < 1 || position > count || count > 3) return false;
        queuePosition = static_cast<std::uint16_t>(position);
        waitingCount = static_cast<std::uint16_t>(count);
        lobbyEpoch = 0;
        lobbyMembers.clear(); //매칭 큐로 돌아오면 이전 대기실 정보 제거
        std::cout << "QUEUE_STATE position=" << queuePosition
                  << " waiting=" << waitingCount << std::endl;
        return true;
    }

    if (size < Wire::LobbyPrefixSize || p[16] != Wire::LobbyEntered || p[17] != 3) {
        return false;
    }
    std::vector<LobbyMember> members;
    std::size_t offset = Wire::LobbyPrefixSize;
    for (std::size_t i = 0; i < p[17]; ++i) {
        if (size - offset < Wire::LobbyMemberPrefixSize) return false;
        const auto nicknameSize = p[offset + 6];
        if (p[offset + 5] > 1 || nicknameSize == 0 ||
            nicknameSize > Wire::MaxNicknameBytes ||
            size - offset - Wire::LobbyMemberPrefixSize < nicknameSize) return false;

        members.push_back({Wire::Read32(p + offset), p[offset + 4], p[offset + 5] != 0,
            std::string(reinterpret_cast<const char*>(p + offset + Wire::LobbyMemberPrefixSize),
                        nicknameSize)});
        offset += Wire::LobbyMemberPrefixSize + nicknameSize;
    }
    if (offset != size) return false;

    // 전체 멤버를 확인한 뒤 대기실 상태를 한 번에 반영
    lobbyEpoch = Wire::Read32(p + Wire::HeaderSize);
    lobbyMembers.swap(members);
    queuePosition = waitingCount = 0;
    std::cout << "READY_LOBBY_STATE epoch=" << lobbyEpoch
              << " members=" << lobbyMembers.size() << '\n';
    for (const auto& member : lobbyMembers) {
        std::cout << "  player=" << member.playerId << " nickname=" << member.nickname
                  << " ready=" << member.ready << '\n';
    }
    std::cout.flush();
    return true;
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
        if (p[3] == Wire::Welcome && size == Wire::WelcomeSize &&
            Wire::ReadRoundId(p) == 0 && !joined) {
            sessionId = Wire::Read32(p + Wire::HeaderSize);
            playerId = Wire::Read32(p + Wire::HeaderSize + 4);
            joined = true; //경기 번호는 이동 테스트용 첫 STATE에서 설정
            std::cout << "JOIN accepted. Session " << sessionId
                      << ", player " << playerId << '\n';
        } else if (p[3] == Wire::Error && size == Wire::ErrorSize &&
                   Wire::ReadRoundId(p) == 0 && p[14] == Wire::Join) {
            const auto error = (std::uint16_t(p[12]) << 8) | p[13];
            std::cerr << "JOIN rejected (code " << error << ").\n";
            if (!joined) { Disconnect(); return true; }
        } else if (p[3] == Wire::QueueState || p[3] == Wire::ReadyLobbyState) {
            if (!ApplyMatchState(p, size)) { Disconnect(); return true; }
        } else if (joined && p[3] == Wire::State && size == Wire::StateSize) {
            lastState = now;
            const auto round = Wire::ReadRoundId(p);
            if (roundId == 0) roundId = round;
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
    if (!Ready()) return;
    std::array<std::uint8_t, 12> p{};
    Wire::Header(p.data(), p.size(), Wire::Reset, ++sequence, ++roundId);
    inputSeq = pendingInput = 0; moveReady = false;
    Queue(p.data(), p.size());
}

void ClientNetwork::Disconnect() {
    receiveBuffer.clear(); sendBuffer.clear();
    joined = false;
    lobbyMembers.clear();
    lobbyEpoch = 0;
    queuePosition = waitingCount = 0;
    sessionId = playerId = 0;
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
