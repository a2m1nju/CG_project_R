#include "ServerNetwork.h"
#include <cstring>
#include <chrono>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <utility>

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

ServerNetwork::TransportSession*
ServerNetwork::FindSession(SessionId id)
{
    for (std::size_t i = 0; i < sessionCount; ++i) {
        if (sessions[i].id == id) {
            return &sessions[i];
        }
    }
    return nullptr;
}
bool ServerNetwork::IsLobbyMember(SessionId id) const
{
    if (!lobbyRoom) {
        return false;
    }

    const auto& room = *lobbyRoom;

    return
        std::find(room.queue.begin(), room.queue.end(), id)
        != room.queue.end() ||
        std::find(room.members.begin(), room.members.end(), id)
        != room.members.end();
}
void ServerNetwork::SendLobby(TransportSession& session, std::vector<std::uint8_t> packet)
{
    if (session.sendBuffer.size() + packet.size() > 16384) {
        // 배열을 순회하는 도중 세션을 제거하지 않음.
        session.drop = true;
        return;
    }

    const auto type = packet[3];

    Wire::Header(
        packet.data(), packet.size(),
        type, ++session.sendSeq, 0);

    session.sendBuffer.insert(
        session.sendBuffer.end(),
        packet.begin(), packet.end());
}

void ServerNetwork::SendLobbyError(TransportSession& session, std::uint16_t code, std::uint8_t rejectedType, std::uint32_t rejectedSequence)
{
    auto packet = LobbyWire::MakePacket(LobbyWire::Error, 7);
    LobbyWire::Write16(packet.data() + 12, code);
    packet[14] = rejectedType;
    Wire::Write32(packet.data() + 15, rejectedSequence);

    SendLobby(session, std::move(packet));
}

void ServerNetwork::PublishLobby()
{
    if (!lobbyRoom) {
        return;
    }
    const auto& room = *lobbyRoom;
    if (room.phase == ServicePhase::MATCHING) {
        // 매칭 대기 중에는 순번·인원만 전달.
        for (std::size_t i = 0; i < room.queue.size(); ++i) {
            auto* session = FindSession(room.queue[i]);
            if (!session) {
                continue;
            }
            auto packet =
                LobbyWire::MakePacket(LobbyWire::QueueState, 5);
            LobbyWire::Write16(
                packet.data() + 12,
                static_cast<std::uint16_t>(i + 1));
            LobbyWire::Write16(
                packet.data() + 14,
                static_cast<std::uint16_t>(room.queue.size()));
            packet[16] = 0; // MATCHING
            SendLobby(*session, std::move(packet));
        }
        return;
    }
    if (room.phase != ServicePhase::READY_LOBBY) {
        return;
    }
    auto packet =
        LobbyWire::MakePacket(LobbyWire::ReadyLobbyState, 6);
    Wire::Write32(packet.data() + 12, room.lobbyEpoch);
    packet[16] = 0; // 활성 준비 대기실
    packet[17] = 3; // 참가자 수

    for (std::size_t i = 0; i < room.members.size(); ++i) {
        const auto id = room.members[i];
        const auto* session = FindSession(id);
        const std::string nickname =
            session ? session->nickname : "Player";
        LobbyWire::Append32(packet, id);
        packet.push_back(0); // characterId: 현재는 모두 닭
        packet.push_back(room.ready[i] ? 1 : 0);
        packet.push_back(
            static_cast<std::uint8_t>(nickname.size()));
        packet.insert(
            packet.end(), nickname.begin(), nickname.end());
    }
    for (const auto id : room.members) {
        if (auto* session = FindSession(id)) {
            SendLobby(*session, packet);
        }
    }
}

void ServerNetwork::CloseLobby(SessionId leaverId)
{
    if (!lobbyRoom || !IsLobbyMember(leaverId)) {
        return;
    }
    auto& room = *lobbyRoom;
    if (room.phase == ServicePhase::PLAYING) {
        const auto it = std::find(
            room.members.begin(),
            room.members.end(),
            leaverId);
        if (it != room.members.end()) {
            const auto index =
                static_cast<std::size_t>(
                    it - room.members.begin());
            room.players[index].alive = false;
            room.players[index].departed = true;
            room.records[index].departed = true;
            room.members[index] = 0;
        }
        if (auto* leaver = FindSession(leaverId)) {
            leaver->alive = false;
            leaver->snapshotting = false;
            leaver->roundId = 0;

            auto packet = LobbyWire::MakePacket(
                LobbyWire::RoomClose, 1);

            SendLobby(*leaver, std::move(packet));
        }
        const bool hasMembers = std::any_of(
            room.members.begin(),
            room.members.end(),
            [](SessionId id) {
                return id != 0;
            });

        if (hasMembers) {
            return;
        }
    }
    std::vector<SessionId> recipients;

    if (room.phase == ServicePhase::MATCHING) {
        recipients = room.queue;
    }
    else {
        for (const auto id : room.members) {
            if (id != 0) {
                recipients.push_back(id);
            }
        }
    }
    const auto nextEpoch = room.lobbyEpoch + 1;
    const auto previousRound = room.roundId;
    room = RoomState{};
    room.lobbyEpoch = nextEpoch;
    room.roundId = previousRound;
    for (const auto id : recipients) {
        if (auto* session = FindSession(id)) {
            session->snapshotting = false;
            session->roundId = 0;
            session->alive = false;

            auto packet = LobbyWire::MakePacket(
                LobbyWire::RoomClose, 1);

            SendLobby(*session, std::move(packet));
        }
    }
    std::cout << "Room closed. Return to title.\n";
}

int ServerNetwork::HandleLobbyPacket(
    TransportSession& session,
    const std::uint8_t* packet,
    std::size_t size)
{
    const auto type = packet[3];
    if (type != LobbyWire::Join && type != LobbyWire::Ready && type != LobbyWire::Leave)
    {
        return 0; // 기존 이동 처리에 전달
    }
    if (!lobbyRoom || Wire::ReadRoundId(packet) != 0) {
        return -1;
    }
    auto& room = *lobbyRoom;
    const auto sequence = Wire::Read32(packet + 4);
    if (type == LobbyWire::Join) {
        if (size < 13) {
            return -1;
        }
        const std::size_t nameLength = packet[12];
        if (nameLength == 0 ||
            nameLength > 24 ||
            size != 13 + nameLength)
        {
            return -1;
        }
        // 현재 입력은 ASCII 이름으로 제한.
        for (std::size_t i = 0; i < nameLength; ++i) {
            const auto ch = packet[13 + i];

            if (ch < 32 || ch > 126) {
                return -1;
            }
        }
        if (IsLobbyMember(session.id)) {
            SendLobbyError(session, LobbyWire::Duplicate, type, sequence);
            return 1;
        }
        MatchCoordinator coordinator(room);
        const auto result = coordinator.Enqueue(session.id);
        if (result.code != 0) {
            SendLobbyError(session, LobbyWire::Busy, type, sequence);
            return 1;
        }
        session.nickname.assign(reinterpret_cast<const char*>(packet + 13), nameLength);
        auto welcome = LobbyWire::MakePacket(LobbyWire::Welcome, 8);
        Wire::Write32(welcome.data() + 12, session.id);
        Wire::Write32(welcome.data() + 16, session.id);
        SendLobby(session, std::move(welcome));
        coordinator.TryPromote();
        PublishLobby();
        std::cout << "JOIN accepted: " << session.id << '\n';
        return 1;
    }
    if (type == LobbyWire::Ready) {
        if (size != 17 || packet[16] > 1) {
            return -1;
        }

        if (room.phase != ServicePhase::READY_LOBBY ||
            !IsLobbyMember(session.id))
        {
            SendLobbyError(session, LobbyWire::BadPhase, type, sequence);
            return 1;
        }
        const auto epoch = Wire::Read32(packet + 12);
        if (epoch != room.lobbyEpoch) {
            SendLobbyError(session, LobbyWire::Stale, type, sequence);
            return 1;
        }

        MatchCoordinator coordinator(room);

        if (!coordinator.SetReady(
            session.id, epoch, packet[16] != 0))
        {
            SendLobbyError(session, LobbyWire::BadPhase, type, sequence);
            return 1;
        }
        PublishLobby();
        const bool allReady = std::all_of(
            room.ready.begin(), room.ready.end(),
            [](bool ready) { return ready; });
        if (allReady) {
            BeginLoading();
        }

        return 1;
    }
    // LEAVE
    if (size != 13) {
        return -1;
    }
    if (IsLobbyMember(session.id)) {
        CloseLobby(session.id);
    }
    else {
        auto close =LobbyWire::MakePacket(LobbyWire::RoomClose, 1);

        close[12] = 0;
        SendLobby(session, std::move(close));
    }
    return 1;
}
bool ServerNetwork::ProcessPackets(
    TransportSession& s,
    std::uint64_t tick)
{
    auto& bytes = s.receiveBuffer;
    std::size_t offset = 0;

    while (bytes.size() - offset >= 2) {
        const auto* p = bytes.data() + offset;
        const std::size_t size = (std::size_t(p[0]) << 8) | p[1];
        if (size < Wire::HeaderSize || size > 1024) {
            return false;
        }
        if (bytes.size() - offset < size) {
            break;
        }
        if (p[2] != 1) {
            return false;
        }
        const auto seq = Wire::Read32(p + 4);
        const auto round = Wire::ReadRoundId(p);

        if (!Wire::Newer(seq, s.sequence)) {
            offset += size;
            continue;
        }
        s.sequence = seq;
        const int lobbyResult = HandleLobbyPacket(s, p, size);
        if (lobbyResult < 0) {
            return false;
        }
        if (lobbyResult > 0) {
            offset += size;
            continue;
        }
        if (p[3] == Wire::Loaded) {
            if (size != Wire::HeaderSize) {
                return false;
            }
            if (lobbyRoom &&
                lobbyRoom->phase == ServicePhase::LOADING &&
                IsRoundMember(s.id) &&
                round == lobbyRoom->roundId &&
                !s.snapshotting)
            {
                RoundController controller(*lobbyRoom);
                controller.MarkLoaded(s.id, round);

                if (lobbyRoom->phase ==
                    ServicePhase::PLAYING)
                {
                    SendStart(
                        static_cast<std::uint32_t>(tick));
                }
            }
            offset += size;
            continue;
        }

        if (p[3] != Wire::Reset &&
            p[3] != Wire::WorldPosition &&
            p[3] != 9)
        {
            return false;
        }
        if (!lobbyRoom ||
            lobbyRoom->phase != ServicePhase::PLAYING ||
            !IsRoundMember(s.id) ||
            round != lobbyRoom->roundId ||
            round != s.roundId)
        {
            SendLobbyError(s, LobbyWire::BadPhase, p[3], seq);
            offset += size;
            continue;
        }
        if (p[3] == Wire::Reset) {
            if (size != Wire::HeaderSize) {
                return false;
            }
            SendLobbyError(s, LobbyWire::BadPhase, p[3], seq);
        }
        else if (p[3] == Wire::WorldPosition) {
            if (size != Wire::PositionSize || p[20] > 1)
            {
                return false;
            }
            const float x = Wire::ReadFloat(p + 12);
            const float z = Wire::ReadFloat(p + 16);
            // 기존 통나무 변위·사망 판정과 호환하는 경로.
            if (std::isfinite(x) &&
                std::isfinite(z) &&
                std::abs(x) < 100.0f &&
                std::abs(z) < 1000000.0f)
            {
                s.x = x;
                s.z = z;
                s.alive = s.alive && p[20] != 0;
            }
        }
        else {
            if (size != 17) {
                return false;
            }
            const auto input = Wire::Read32(p + 12);
            if (Wire::Newer(input, s.inputSeq)) {
                s.inputSeq = input;
                if (!s.alive || p[16] > 3) {
                    s.status = Wire::MoveStatus::Invalid;
                }
                else if (tick < s.nextMoveTick) {
                    s.status = Wire::MoveStatus::Cooldown;
                }
                else {
                    float x = std::round(s.x);
                    float z = std::round(s.z);
                    float rotation = 0;
                    switch (
                        static_cast<Direction>(p[16]))
                    {
                    case Direction::UP:
                        --z;
                        rotation = 0;
                        break;

                    case Direction::DOWN:
                        ++z;
                        rotation = 180;
                        break;

                    case Direction::LEFT:
                        --x;
                        rotation = 90;
                        break;

                    case Direction::RIGHT:
                        ++x;
                        rotation = -90;
                        break;
                    }

                    const auto* lane = world.FindLane(static_cast<std::int32_t>(z));
                    const int gridX = static_cast<int>(x);
                    const bool blocked =
                        gridX < -15 ||
                        gridX > 15 ||
                        !lane ||
                        (lane->treeMask &
                            (std::uint32_t{ 1 } <<
                                (gridX + 15))) != 0;

                    if (blocked) {
                        s.status = Wire::MoveStatus::Invalid;
                    }
                    else {
                        s.x = x;
                        s.z = z;
                        s.rotation = rotation;
                        s.nextMoveTick = tick + Wire::MoveCooldownTicks;
                        s.status =  Wire::MoveStatus::Accepted;
                    }
                }
            }
        }

        offset += size;
    }
    bytes.erase(
        bytes.begin(),
        bytes.begin() + offset);

    return true;
}

void ServerNetwork::Tick(std::uint64_t tick)
{
    currentTick = tick;
    for (std::size_t i = sessionCount; i > 0; --i) {
        auto& s = sessions[i - 1];

        if (s.drop ||
            !ProcessPackets(s, tick) ||
            s.drop)
        {
            RemoveSession(i - 1);
        }
    }
    if (lobbyRoom &&
        lobbyRoom->phase == ServicePhase::LOADING &&
        tick - loadingStartTick > 30 * Wire::TickRate)
    {
        for (const auto id : lobbyRoom->members) {
            if (id != 0) {
                CloseLobby(id);
                break;
            }
        }
    }
    if (lobbyRoom &&
        lobbyRoom->phase == ServicePhase::PLAYING)
    {
        auto minZ = world.MinZ();
        auto maxZ = world.MaxZ();

        for (std::size_t i = 0;
            i < sessionCount;
            ++i)
        {
            if (!IsRoundMember(sessions[i].id)) {
                continue;
            }

            const auto z =
                static_cast<std::int32_t>(
                    std::floor(sessions[i].z));

            minZ = (std::min)(minZ, z - 30);
            maxZ = (std::max)(maxZ, z + 10);
        }

        world.EnsureRange(minZ, maxZ, 8);
        world.Step(tick);
    }

    for (std::size_t i = 0; i < sessionCount; ++i)
    {
        auto& s = sessions[i];

        if (lobbyRoom &&
            IsRoundMember(s.id) &&
            (lobbyRoom->phase == ServicePhase::LOADING ||
                lobbyRoom->phase == ServicePhase::PLAYING))
        {
            PumpWorldPackets(
                s, static_cast<std::uint32_t>(tick));

            if (lobbyRoom->phase == ServicePhase::PLAYING &&
                !s.snapshotting &&
                tick % 3 == 0)
            {
                for (const auto& entry : world.Actors()) {
                    const auto& actor = entry.second;

                    if (actor.z >= s.z - 30 &&
                        actor.z <= s.z + 10 &&
                        !QueueWorldActor(
                            s,
                            actor,
                            static_cast<std::uint32_t>(tick)))
                    {
                        break;
                    }
                }
            }
        }

        std::array<std::uint8_t, Wire::StateSize> packet{};

        Wire::Header(
            packet.data(),
            packet.size(),
            Wire::State,
            ++s.sendSeq,
            s.roundId);

        Wire::Write32(
            packet.data() + 12, s.inputSeq);

        Wire::Write32(
            packet.data() + 16,
            static_cast<std::uint32_t>(tick));

        Wire::WriteFloat(packet.data() + 20, s.x);
        Wire::WriteFloat(packet.data() + 24, s.z);

        packet[28] =
            static_cast<std::uint8_t>(s.status);

        if (s.sendBuffer.size() + packet.size() > 16384) {
            s.drop = true;
        }
        else {
            s.sendBuffer.insert(
                s.sendBuffer.end(),
                packet.begin(),
                packet.end());
        }

        if (tick % Wire::TickRate == 0 &&
            IsRoundMember(s.id))
        {
            std::cout
                << "[TICK " << tick << "]"
                << " player=" << s.id
                << " round=" << s.roundId
                << " x=" << s.x
                << " z=" << s.z
                << " alive=" << s.alive
                << '\n';
        }
    }

    if (lobbyRoom &&
        lobbyRoom->phase == ServicePhase::PLAYING)
    {
        PublishPlayers(
            static_cast<std::uint32_t>(tick));
    }

    for (std::size_t i = sessionCount; i > 0; --i) {
        if (sessions[i - 1].drop ||
            !Flush(sessions[i - 1]))
        {
            RemoveSession(i - 1);
        }
    }
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
                    auto& s = sessions[sessionCount++];
                    s = TransportSession{};

                    s.id = nextSessionId++;
                    s.socket = client;

                    // 이 접속에 부여한 ID를 메시지에 기록
                    std::array<std::uint8_t, Wire::AssignedIdSize> packet{};

                    Wire::Header(
                        packet.data(),
                        packet.size(),
                        Wire::AssignedId,
                        ++s.sendSeq,
                        s.roundId
                    );

                    // 공통 헤더 뒤에 플레이어 ID 기록
                    Wire::Write32(
                        packet.data() + Wire::HeaderSize,
                        s.id
                    );

                    // 해당 클라이언트의 송신 대기열에 추가
                    s.sendBuffer.insert(
                        s.sendBuffer.end(),
                        packet.begin(),
                        packet.end()
                    );

                    std::cout << "TCP session connected: "
                        << s.id << std::endl;
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
    CloseLobby(sessions[index].id);
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

bool ServerNetwork::IsRoundMember(SessionId id) const
{
    return lobbyRoom &&
        id != 0 &&
        std::find(
            lobbyRoom->members.begin(),
            lobbyRoom->members.end(),
            id) != lobbyRoom->members.end();
}

void ServerNetwork::SendGame(
    TransportSession& session,
    std::vector<std::uint8_t> packet)
{
    if (!lobbyRoom ||
        session.sendBuffer.size() + packet.size() > 16384)
    {
        session.drop = true;
        return;
    }

    Wire::Header(
        packet.data(),
        packet.size(),
        packet[3],
        ++session.sendSeq,
        lobbyRoom->roundId);

    session.sendBuffer.insert(
        session.sendBuffer.end(),
        packet.begin(),
        packet.end());
}

void ServerNetwork::BeginLoading()
{
    if (!lobbyRoom) {
        return;
    }

    RoundController controller(*lobbyRoom);
    controller.BeginRound(false);

    if (lobbyRoom->phase != ServicePhase::LOADING) {
        return;
    }

    world.Reset(lobbyRoom->roundId);
    loadingStartTick = currentTick;

    for (std::size_t slot = 0;
        slot < lobbyRoom->members.size();
        ++slot)
    {
        auto* session =
            FindSession(lobbyRoom->members[slot]);

        if (!session) {
            CloseLobby(lobbyRoom->members[slot]);
            return;
        }

        session->roundId = lobbyRoom->roundId;

        session->x =
            static_cast<float>(
                static_cast<int>(slot) * 2 - 2);

        session->z = 0;
        session->rotation = 0;
        session->alive = true;

        session->inputSeq = 0;
        session->nextMoveTick = 0;
        session->status = Wire::MoveStatus::Idle;

        StartWorldSnapshot(*session);
    }
}

void ServerNetwork::SendStart(std::uint32_t tick)
{
    if (!lobbyRoom ||
        lobbyRoom->phase != ServicePhase::PLAYING)
    {
        return;
    }

    std::vector<std::uint8_t> packet(19 + 13 * 3, 0);

    packet[3] = Wire::Start;

    Wire::Write32(packet.data() + 12, tick);

    LobbyWire::Write16(
        packet.data() + 16,
        static_cast<std::uint16_t>(Wire::TickRate));

    packet[18] = 3;

    for (std::size_t slot = 0; slot < 3; ++slot) {
        auto* session =
            FindSession(lobbyRoom->members[slot]);

        if (!session) {
            CloseLobby(lobbyRoom->members[slot]);
            return;
        }

        auto* record =
            packet.data() + 19 + slot * 13;

        Wire::Write32(record, session->id);
        record[4] = static_cast<std::uint8_t>(slot);

        Wire::WriteFloat(record + 5, session->x);
        Wire::WriteFloat(record + 9, session->z);
    }

    for (const auto id : lobbyRoom->members) {
        if (auto* session = FindSession(id)) {
            SendGame(*session, packet);
        }
    }
}

void ServerNetwork::PublishPlayers(std::uint32_t tick)
{
    if (!lobbyRoom) {
        return;
    }

    std::vector<std::uint8_t> packet(17, 0);

    packet[3] = Wire::PlayerStates;
    Wire::Write32(packet.data() + 12, tick);

    std::uint8_t count = 0;

    for (std::size_t slot = 0;
        slot < lobbyRoom->members.size();
        ++slot)
    {
        auto* session =
            FindSession(lobbyRoom->members[slot]);

        if (!session || !IsRoundMember(session->id)) {
            continue;
        }

        lobbyRoom->players[slot].alive = session->alive;

        const auto offset = packet.size();
        packet.resize(offset + 17);

        auto* record = packet.data() + offset;

        Wire::Write32(record, session->id);
        Wire::WriteFloat(record + 4, session->x);
        Wire::WriteFloat(record + 8, session->z);
        Wire::WriteFloat(record + 12, session->rotation);

        record[16] = session->alive ? 1 : 0;
        ++count;
    }

    packet[16] = count;

    for (const auto id : lobbyRoom->members) {
        if (auto* session = FindSession(id)) {
            SendGame(*session, packet);
        }
    }
}

bool ServerNetwork::QueueWorldBegin(TransportSession& s)
{
    if (s.sendBuffer.size() +
        Wire::WorldBeginSize +
        Wire::StateSize > 16384)
    {
        return false;
    }

    std::array<std::uint8_t, Wire::WorldBeginSize> packet{};

    Wire::Header(
        packet.data(),
        packet.size(),
        Wire::WorldBegin,
        ++s.sendSeq,
        world.RoundId());

    Wire::Write32(packet.data() + 12, world.Generation());

    Wire::Write32(
        packet.data() + 16,
        static_cast<std::uint32_t>(s.snapshotMinZ));

    Wire::Write32(
        packet.data() + 20,
        static_cast<std::uint32_t>(s.snapshotMaxZ));

    s.sendBuffer.insert(
        s.sendBuffer.end(),
        packet.begin(),
        packet.end());

    return true;
}

bool ServerNetwork::QueueWorldLane(TransportSession& s, const WorldLaneData& lane)
{
    if (s.sendBuffer.size() +
        Wire::WorldLaneSize +
        Wire::StateSize > 16384)
    {
        return false;
    }

    std::array<std::uint8_t, Wire::WorldLaneSize> packet{};

    Wire::Header(
        packet.data(),
        packet.size(),
        Wire::WorldLane,
        ++s.sendSeq,
        world.RoundId());

    Wire::Write32(packet.data() + 12, world.Generation());

    Wire::Write32(
        packet.data() + 16,
        static_cast<std::uint32_t>(lane.z));

    packet[20] = lane.terrain;

    Wire::Write32(packet.data() + 21, lane.treeMask);
    Wire::Write32(packet.data() + 25, lane.padMask);
    Wire::Write32(packet.data() + 29, lane.spawnMask);

    s.sendBuffer.insert(
        s.sendBuffer.end(),
        packet.begin(),
        packet.end());

    return true;
}

bool ServerNetwork::QueueWorldActor(TransportSession& s, const WorldActorData& actor, std::uint32_t tick)
{
    if (s.sendBuffer.size() +
        Wire::WorldActorSize +
        Wire::StateSize > 16384)
    {
        return false;
    }

    std::array<std::uint8_t, Wire::WorldActorSize> packet{};

    Wire::Header(
        packet.data(),
        packet.size(),
        Wire::WorldActor,
        ++s.sendSeq,
        world.RoundId());

    Wire::Write32(packet.data() + 12, world.Generation());
    Wire::Write32(packet.data() + 16, tick);
    Wire::Write32(packet.data() + 20, actor.id);

    packet[24] = static_cast<std::uint8_t>(actor.kind);

    Wire::WriteFloat(packet.data() + 25, actor.z);
    Wire::WriteFloat(packet.data() + 29, actor.x);
    Wire::WriteFloat(packet.data() + 33, actor.speed);
    Wire::WriteFloat(packet.data() + 37, actor.width);

    packet[41] = actor.red;
    packet[42] = actor.green;
    packet[43] = actor.blue;
    packet[44] = actor.design;
    packet[45] = actor.trainState;

    packet[46] =
        static_cast<std::uint8_t>(actor.trainTimer >> 8);

    packet[47] =
        static_cast<std::uint8_t>(actor.trainTimer);

    packet[48] = actor.trainLight ? 1 : 0;

    s.sendBuffer.insert(
        s.sendBuffer.end(),
        packet.begin(),
        packet.end());

    return true;
}

bool ServerNetwork::QueueWorldEnd(TransportSession& s)
{
    if (s.sendBuffer.size() +
        Wire::WorldEndSize +
        Wire::StateSize > 16384)
    {
        return false;
    }

    std::array<std::uint8_t, Wire::WorldEndSize> packet{};

    Wire::Header(
        packet.data(),
        packet.size(),
        Wire::WorldEnd,
        ++s.sendSeq,
        world.RoundId());

    Wire::Write32(packet.data() + 12, world.Generation());

    s.sendBuffer.insert(
        s.sendBuffer.end(),
        packet.begin(),
        packet.end());

    return true;
}

void ServerNetwork::StartWorldSnapshot(TransportSession& s)
{
    s.snapshotting = true;
    s.snapshotBeginPending = true;

    s.snapshotMinZ = world.MinZ();
    s.snapshotMaxZ = world.MaxZ();
    s.snapshotNextZ = s.snapshotMinZ;
}

void ServerNetwork::PumpWorldPackets(
    TransportSession& s,
    std::uint32_t tick)
{
    constexpr int kMaxLanesPerTick = 12;

    if (s.snapshotting) {
        if (s.snapshotBeginPending) {
            if (!QueueWorldBegin(s)) {
                return;
            }

            s.snapshotBeginPending = false;
        }

        int sent = 0;

        while (s.snapshotNextZ <= s.snapshotMaxZ && sent < kMaxLanesPerTick)
        {
            const auto* lane =
                world.FindLane(s.snapshotNextZ);

            if (!lane ||
                s.sendBuffer.size() +
                Wire::WorldLaneSize +
                lane->actorIds.size() *
                Wire::WorldActorSize +
                Wire::StateSize > 16384 ||
                !QueueWorldLane(s, *lane))
            {
                return;
            }

            for (const auto id : lane->actorIds) {
                const auto* actor = world.FindActor(id);

                if (!actor ||
                    !QueueWorldActor(s, *actor, tick))
                {
                    return;
                }
            }

            ++s.snapshotNextZ;
            ++sent;
        }

        if (s.snapshotNextZ <= s.snapshotMaxZ ||
            !QueueWorldEnd(s))
        {
            return;
        }

        s.snapshotting = false;
        s.announcedMinZ = s.snapshotMinZ;
        s.announcedMaxZ = s.snapshotMaxZ;
    }

    int sent = 0;

    while (s.announcedMinZ > world.MinZ() &&
        sent < kMaxLanesPerTick)
    {
        const auto* lane =
            world.FindLane(s.announcedMinZ - 1);

        if (!lane ||
            s.sendBuffer.size() +
            Wire::WorldLaneSize +
            lane->actorIds.size() *
            Wire::WorldActorSize +
            Wire::StateSize > 16384 ||
            !QueueWorldLane(s, *lane))
        {
            return;
        }

        for (const auto id : lane->actorIds) {
            const auto* actor = world.FindActor(id);

            if (!actor ||
                !QueueWorldActor(s, *actor, tick))
            {
                return;
            }
        }

        --s.announcedMinZ;
        ++sent;
    }

    while (s.announcedMaxZ < world.MaxZ() &&
        sent < kMaxLanesPerTick)
    {
        const auto* lane =
            world.FindLane(s.announcedMaxZ + 1);

        if (!lane ||
            s.sendBuffer.size() +
            Wire::WorldLaneSize +
            lane->actorIds.size() *
            Wire::WorldActorSize +
            Wire::StateSize > 16384 ||
            !QueueWorldLane(s, *lane))
        {
            return;
        }

        for (const auto id : lane->actorIds) {
            const auto* actor = world.FindActor(id);

            if (!actor ||
                !QueueWorldActor(s, *actor, tick))
            {
                return;
            }
        }

        ++s.announcedMaxZ;
        ++sent;
    }
}