#include "ServerNetwork.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <random>
#include <utility>

#pragma comment(lib, "Ws2_32.lib")

bool ServerNetwork::Initialize(std::uint16_t port)
{
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

    if (bind(
        listenSocket,
        reinterpret_cast<const sockaddr*>(&address),
        sizeof(address)) == SOCKET_ERROR)
    {
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

    if (ioctlsocket(
        listenSocket,
        FIONBIO,
        &nonBlocking) == SOCKET_ERROR)
    {
        std::cerr << "ioctlsocket failed: "
            << WSAGetLastError() << '\n';

        Shutdown();
        return false;
    }

    stopping.store(false);
    return true;
}

bool ServerNetwork::Flush(TransportSession& session)
{
    while (!session.sendBuffer.empty()) {
        const int n = send(
            session.socket,
            reinterpret_cast<const char*>(session.sendBuffer.data()),
            static_cast<int>(session.sendBuffer.size()),
            0);

        if (n == SOCKET_ERROR &&
            WSAGetLastError() == WSAEWOULDBLOCK)
        {
            return true;
        }

        if (n <= 0) {
            return false;
        }

        session.sendBuffer.erase(
            session.sendBuffer.begin(),
            session.sendBuffer.begin() + n);
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

bool ServerNetwork::IsRoundMember(SessionId id) const
{
    return lobbyRoom &&
        id != 0 &&
        std::find(
            lobbyRoom->members.begin(),
            lobbyRoom->members.end(),
            id) != lobbyRoom->members.end();
}

void ServerNetwork::SendLobby(
    TransportSession& session,
    std::vector<std::uint8_t> packet)
{
    if (session.sendBuffer.size() + packet.size() > 16384) {
        session.drop = true;
        return;
    }

    const auto type = packet[3];

    Wire::Header(
        packet.data(),
        packet.size(),
        type,
        ++session.sendSeq,
        0);

    session.sendBuffer.insert(
        session.sendBuffer.end(),
        packet.begin(),
        packet.end());
}

void ServerNetwork::SendLobbyError(
    TransportSession& session,
    std::uint16_t code,
    std::uint8_t rejectedType,
    std::uint32_t rejectedSequence)
{
    auto packet = LobbyWire::MakePacket(LobbyWire::Error, 7);

    LobbyWire::Write16(packet.data() + 12, code);
    packet[14] = rejectedType;
    Wire::Write32(packet.data() + 15, rejectedSequence);

    SendLobby(session, std::move(packet));
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

void ServerNetwork::PublishLobby()
{
    if (!lobbyRoom) {
        return;
    }

    const auto& room = *lobbyRoom;

    if (room.phase == ServicePhase::MATCHING) {
        for (std::size_t i = 0; i < room.queue.size(); ++i) {
            auto* session = FindSession(room.queue[i]);

            if (!session) {
                continue;
            }

            auto packet = LobbyWire::MakePacket(
                LobbyWire::QueueState, 5);

            LobbyWire::Write16(
                packet.data() + 12,
                static_cast<std::uint16_t>(i + 1));

            LobbyWire::Write16(
                packet.data() + 14,
                static_cast<std::uint16_t>(room.queue.size()));

            packet[16] = 0;
            SendLobby(*session, std::move(packet));
        }

        return;
    }

    if (room.phase != ServicePhase::READY_LOBBY) {
        return;
    }

    auto packet = LobbyWire::MakePacket(
        LobbyWire::ReadyLobbyState, 6);

    Wire::Write32(packet.data() + 12, room.lobbyEpoch);
    packet[16] = 0;
    packet[17] = 3;

    for (std::size_t i = 0; i < room.members.size(); ++i) {
        const auto id = room.members[i];
        const auto* session = FindSession(id);

        const std::string nickname =
            session ? session->nickname : "Player";

        LobbyWire::Append32(packet, id);
        packet.push_back(0);
        packet.push_back(room.ready[i] ? 1 : 0);
        packet.push_back(
            static_cast<std::uint8_t>(nickname.size()));
        packet.push_back(session ?
            static_cast<std::uint8_t>(session->characterId) : 0);

        packet.insert(
            packet.end(),
            nickname.begin(),
            nickname.end());
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
    items.RemovePlayer(leaverId);

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
            if (leaver->alive) {
                leaver->alive = false;
                leaver->deathReason = Wire::DeathReason::Other;
            }
            leaver->snapshotting = false;
            leaver->itemSnapshotPending = false;
            leaver->resetVote = false;
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
            session->itemSnapshotPending = false;
            session->resetVote = false;
            session->roundId = 0;
            session->characterId = CharacterId::Chicken;
            if (session->alive) {
                session->alive = false;
                session->deathReason = Wire::DeathReason::Other;
            }

            items.RemovePlayer(session->id);

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

    if (type != LobbyWire::Join &&
        type != LobbyWire::Ready &&
        type != LobbyWire::CharacterSelection &&
        type != LobbyWire::Leave)
    {
        return 0;
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

        for (std::size_t i = 0; i < nameLength; ++i) {
            const auto ch = packet[13 + i];

            if (ch < 32 || ch > 126) {
                return -1;
            }
        }

        if (IsLobbyMember(session.id)) {
            SendLobbyError(
                session,
                LobbyWire::Duplicate,
                type,
                sequence);
            return 1;
        }

        MatchCoordinator coordinator(room);
        const auto result = coordinator.Enqueue(session.id);

        if (result.code != 0) {
            SendLobbyError(
                session,
                LobbyWire::Busy,
                type,
                sequence);
            return 1;
        }

        session.nickname.assign(
            reinterpret_cast<const char*>(packet + 13),
            nameLength);
        session.characterId = CharacterId::Chicken;

        auto welcome = LobbyWire::MakePacket(
            LobbyWire::Welcome, 8);

        Wire::Write32(welcome.data() + 12, session.id);
        Wire::Write32(welcome.data() + 16, session.id);

        SendLobby(session, std::move(welcome));

        coordinator.TryPromote();
        PublishLobby();

        std::cout << "JOIN accepted: "
            << session.id << '\n';

        return 1;
    }

    if (type == LobbyWire::Ready) {
        if (size != 17 || packet[16] > 1) {
            return -1;
        }

        if (room.phase != ServicePhase::READY_LOBBY ||
            !IsLobbyMember(session.id))
        {
            SendLobbyError(
                session,
                LobbyWire::BadPhase,
                type,
                sequence);
            return 1;
        }

        const auto epoch = Wire::Read32(packet + 12);

        if (epoch != room.lobbyEpoch) {
            SendLobbyError(
                session,
                LobbyWire::Stale,
                type,
                sequence);
            return 1;
        }

        MatchCoordinator coordinator(room);

        const auto readyResult = coordinator.SetReady(
            session.id,
            epoch,
            packet[16] != 0);

        if (readyResult != ReadyResult::Accepted) {
            SendLobbyError(
                session,
                LobbyWire::BadPhase,
                type,
                sequence);
            return 1;
        }

        PublishLobby();
        BeginLoading();
        return 1;
    }

    if (type == LobbyWire::CharacterSelection) {
        if (size != 17) return -1;

        if (room.phase != ServicePhase::READY_LOBBY ||
            !IsLobbyMember(session.id) ||
            !IsValidCharacterId(packet[16]))
        {
            SendLobbyError(session, LobbyWire::BadPhase, type, sequence);
            return 1;
        }
        if (Wire::Read32(packet + 12) != room.lobbyEpoch) {
            SendLobbyError(session, LobbyWire::Stale, type, sequence);
            return 1;
        }
        const auto it = std::find(room.members.begin(),
                                  room.members.end(), session.id);
        if (room.ready[static_cast<std::size_t>(it - room.members.begin())]) {
            SendLobbyError(session, LobbyWire::BadPhase, type, sequence);
            return 1;
        }
        session.characterId = static_cast<CharacterId>(packet[16]);
        PublishLobby();
        return 1;
    }

    if (size != 13) {
        return -1;
    }

    if (IsLobbyMember(session.id)) {
        CloseLobby(session.id);
    }
    else {
        auto close = LobbyWire::MakePacket(
            LobbyWire::RoomClose, 1);

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

        const std::size_t size =
            (std::size_t(p[0]) << 8) | p[1];

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

        const int lobbyResult =
            HandleLobbyPacket(s, p, size);

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
                !s.snapshotting &&
                !s.itemSnapshotPending)
            {
                RoundController controller(*lobbyRoom);
                controller.MarkLoaded(s.id, round);

                if (lobbyRoom->phase == ServicePhase::PLAYING) {
                    SendStart(
                        static_cast<std::uint32_t>(tick));
                }
            }

            offset += size;
            continue;
        }

        if (p[3] != Wire::Reset &&
            p[3] != Wire::WorldPosition &&
            p[3] != Wire::PickupRequest &&
            p[3] != Wire::ItemSpawnRequest &&
            p[3] != Wire::ItemSwitch &&
            p[3] != Wire::ItemUse &&
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
            SendLobbyError(
                s,
                LobbyWire::BadPhase,
                p[3],
                seq);

            offset += size;
            continue;
        }

        if (p[3] == Wire::Reset) {
            if (size != Wire::HeaderSize) {
                return false;
            }

            s.resetVote = true;
        }
        else if (p[3] == Wire::PickupRequest ||
            p[3] == Wire::ItemSpawnRequest ||
            p[3] == Wire::ItemSwitch ||
            p[3] == Wire::ItemUse)
        {
            if (!HandleItemRequest(s, p, size)) {
                return false;
            }
        }
        else if (p[3] == Wire::WorldPosition) {
            if (size != Wire::PositionSize ||
                p[20] > 1)
            {
                return false;
            }

            const float x = Wire::ReadFloat(p + 12);
            const float z = Wire::ReadFloat(p + 16);

            if (std::isfinite(x) &&
                std::isfinite(z) &&
                std::abs(x) < 100.0f &&
                std::abs(z) < 1000000.0f)
            {
                if (!s.awaitingBananaPosition ||
                    (std::abs(x - s.x) < 0.01f &&
                        std::abs(z - s.z) < 0.01f))
                {
                    s.checkBanana =
                        s.checkBanana ||
                        x != s.x ||
                        z != s.z;

                    s.x = x;
                    s.z = z;
                    s.awaitingBananaPosition = false;
                }

                if (s.alive && p[20] == 0) {
                    s.alive = false;
                    s.deathReason = Wire::DeathReason::Other;
                }
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

                    auto direction =
                        static_cast<Direction>(p[16]);

                    const auto* effects =
                        items.GetPlayerEffects(s.id);

                    if (effects &&
                        effects->reverseControlsRemainingMs > 0)
                    {
                        switch (direction) {
                        case Direction::UP:
                            direction = Direction::DOWN;
                            break;
                        case Direction::DOWN:
                            direction = Direction::UP;
                            break;
                        case Direction::LEFT:
                            direction = Direction::RIGHT;
                            break;
                        case Direction::RIGHT:
                            direction = Direction::LEFT;
                            break;
                        }
                    }

                    switch (direction) {
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

                    const auto* lane = world.FindLane(
                        static_cast<std::int32_t>(z));

                    const int gridX = static_cast<int>(x);

                    bool occupied = false;
                    for (std::size_t i = 0; i < sessionCount; ++i) {
                        const auto& other = sessions[i];
                        if (other.id != s.id && IsRoundMember(other.id) &&
                            other.alive &&
                            std::abs(other.x - x) < 0.5f &&
                            std::abs(other.z - z) < 0.5f) {
                            occupied = true;
                            break;
                        }
                    }

                    const bool blocked =
                        gridX < -15 ||
                        gridX > 15 ||
                        !lane ||
                        occupied ||
                        (lane->treeMask &
                            (std::uint32_t{ 1 } <<
                                (gridX + 15))) != 0;

                    if (blocked) {
                        s.rotation = rotation;
                        s.status =
                            Wire::MoveStatus::Invalid;
                    }
                    else {
                        s.x = x;
                        s.z = z;
                        s.rotation = rotation;

                        s.nextMoveTick =
                            tick + Wire::MoveCooldownTicks;

                        s.status =
                            Wire::MoveStatus::Accepted;

                        s.checkBanana = true;
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

    if (lobbyRoom &&
        lobbyRoom->phase == ServicePhase::PLAYING)
    {
        items.AdvanceEffects(
            tick % 3 == 0 ? 34 : 33);
    }

    for (std::size_t i = sessionCount; i > 0; --i) {
        auto& s = sessions[i - 1];

        if (s.drop ||
            !ProcessPackets(s, tick) ||
            s.drop)
        {
            RemoveSession(i - 1);
        }
    }

    TryBeginNextRound();

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
        const auto elapsedTicks = tick - roundStartTick;
        for (std::size_t i = 0; i < sessionCount; ++i) {
            auto& session = sessions[i];
            if (IsRoundMember(session.id) && session.alive &&
                ScreenChase::Overtaken(session.z, elapsedTicks)) {
                session.alive = false;
                session.deathReason = Wire::DeathReason::Chase;
            }
        }

        auto minZ = world.MinZ();
        auto maxZ = world.MaxZ();

        for (std::size_t i = 0; i < sessionCount; ++i) {
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

    for (std::size_t i = 0; i < sessionCount; ++i) {
        auto& s = sessions[i];

        if (lobbyRoom &&
            IsRoundMember(s.id) &&
            (lobbyRoom->phase == ServicePhase::LOADING ||
                lobbyRoom->phase == ServicePhase::PLAYING))
        {
            PumpWorldPackets(
                s,
                static_cast<std::uint32_t>(tick));

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

        if (lobbyRoom &&
            lobbyRoom->phase == ServicePhase::PLAYING &&
            IsRoundMember(s.id) &&
            s.checkBanana &&
            s.alive)
        {
            bool ready = true;

            for (std::size_t j = 0;
                j < sessionCount;
                ++j)
            {
                if (IsRoundMember(sessions[j].id) &&
                    (sessions[j].itemSnapshotPending ||
                        sessions[j].sendBuffer.size() +
                        Wire::BananaHitSize +
                        Wire::StateSize > 16384))
                {
                    ready = false;
                }
            }

            if (ready) {
                s.checkBanana = false;

                BananaHitResult hit{};

                const auto x =
                    static_cast<std::int32_t>(
                        std::round(s.x));

                const auto z =
                    static_cast<std::int32_t>(
                        std::round(s.z));

                items.ResolveBananaHit(s.id, x, z, &hit);

                if (hit.trapId != 0) {
                    s.x = static_cast<float>(x);

                    s.z = static_cast<float>(
                        z + (hit.pushed
                            ? GetItemRule(
                                ItemKind::Banana).hitPushbackCells
                            : 0));

                    s.awaitingBananaPosition = hit.pushed;

                    for (std::size_t j = 0;
                        j < sessionCount;
                        ++j)
                    {
                        if (IsRoundMember(sessions[j].id)) {
                            QueueBananaHit(
                                sessions[j],
                                hit,
                                s.id,
                                x,
                                static_cast<std::int32_t>(s.z));
                        }
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

        Wire::Write32(packet.data() + 12, s.inputSeq);

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

void ServerNetwork::RunAcceptLoop()
{
    using Clock = std::chrono::steady_clock;

    const auto interval =
        std::chrono::nanoseconds(
            1000000000 / Wire::TickRate);

    auto nextTick = Clock::now() + interval;
    std::uint64_t tick = 0;

    while (!stopping.load()) {
        fd_set readable;
        fd_set writable;

        FD_ZERO(&readable);
        FD_ZERO(&writable);
        FD_SET(listenSocket, &readable);

        for (std::size_t i = 0; i < sessionCount; ++i) {
            FD_SET(sessions[i].socket, &readable);

            if (!sessions[i].sendBuffer.empty()) {
                FD_SET(sessions[i].socket, &writable);
            }
        }

        const auto remaining =
            std::chrono::duration_cast<std::chrono::microseconds>(
                nextTick - Clock::now()).count();

        timeval timeout{
            0,
            static_cast<long>(remaining > 0 ? remaining : 0)
        };

        const int ready = select(
            0,
            &readable,
            &writable,
            nullptr,
            &timeout);

        if (ready == SOCKET_ERROR) {
            std::cerr << "select failed: "
                << WSAGetLastError() << '\n';
            return;
        }

        if (stopping.load()) {
            break;
        }

        for (std::size_t i = sessionCount; i > 0; --i) {
            auto& s = sessions[i - 1];

            if (FD_ISSET(s.socket, &writable) &&
                !Flush(s))
            {
                RemoveSession(i - 1);
                continue;
            }

            if (!FD_ISSET(s.socket, &readable)) {
                continue;
            }

            std::uint8_t buffer[1024];

            const int n = recv(
                s.socket,
                reinterpret_cast<char*>(buffer),
                sizeof(buffer),
                0);

            if (n == SOCKET_ERROR &&
                WSAGetLastError() == WSAEWOULDBLOCK)
            {
                continue;
            }

            if (n <= 0 ||
                s.receiveBuffer.size() + n > 16384)
            {
                RemoveSession(i - 1);
                continue;
            }

            s.receiveBuffer.insert(
                s.receiveBuffer.end(),
                buffer,
                buffer + n);
        }

        if (FD_ISSET(listenSocket, &readable)) {
            const SOCKET client =
                accept(listenSocket, nullptr, nullptr);

            if (client != INVALID_SOCKET) {
                u_long nonBlocking = 1;

                if (sessionCount == sessions.size() ||
                    ioctlsocket(
                        client,
                        FIONBIO,
                        &nonBlocking) == SOCKET_ERROR)
                {
                    closesocket(client);
                }
                else {
                    auto& s = sessions[sessionCount++];
                    s = TransportSession{};

                    s.id = nextSessionId++;
                    s.socket = client;

                    std::array<
                        std::uint8_t,
                        Wire::AssignedIdSize
                    > packet{};

                    Wire::Header(
                        packet.data(),
                        packet.size(),
                        Wire::AssignedId,
                        ++s.sendSeq,
                        s.roundId);

                    Wire::Write32(
                        packet.data() + Wire::HeaderSize,
                        s.id);

                    s.sendBuffer.insert(
                        s.sendBuffer.end(),
                        packet.begin(),
                        packet.end());

                    std::cout
                        << "TCP session connected: "
                        << s.id << std::endl;
                }
            }
        }

        while (!stopping.load() &&
            Clock::now() >= nextTick)
        {
            Tick(++tick);
            nextTick += interval;
        }
    }
}

void ServerNetwork::RemoveSession(std::size_t index)
{
    CloseLobby(sessions[index].id);

    const auto id = sessions[index].id;
    closesocket(sessions[index].socket);

    --sessionCount;

    if (index != sessionCount) {
        sessions[index] = sessions[sessionCount];
    }

    sessions[sessionCount] = TransportSession{};

    std::cout << "TCP session disconnected: "
        << id << std::endl;
}

void ServerNetwork::StopAccepting()
{
    stopping.store(true);
}

void ServerNetwork::Shutdown()
{
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

void ServerNetwork::BeginLoading()
{
    if (!lobbyRoom) {
        return;
    }

    RoundController controller(*lobbyRoom);

    if (controller.BeginRound(false)) {
        StartRoundSnapshots();
    }
}

void ServerNetwork::StartRoundSnapshots()
{
    ResetWorld(lobbyRoom->roundId);
    loadingStartTick = currentTick;
    roundStartTick = 0;

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

        session->x = static_cast<float>(
            static_cast<int>(slot) * 2 - 2);

        session->z = 0;
        session->rotation = 0;
        session->alive = true;
        session->deathReason = Wire::DeathReason::None;

        session->inputSeq = 0;
        session->nextMoveTick = 0;
        session->status = Wire::MoveStatus::Idle;

        session->resetVote = false;
        session->checkBanana = false;
        session->awaitingBananaPosition = false;

        std::array<
            std::uint8_t,
            Wire::ItemIdentitySize
        > identity{};

        Wire::Header(
            identity.data(),
            identity.size(),
            Wire::ItemIdentity,
            ++session->sendSeq,
            session->roundId);

        Wire::Write32(
            identity.data() + Wire::HeaderSize,
            session->id);

        session->sendBuffer.insert(
            session->sendBuffer.end(),
            identity.begin(),
            identity.end());

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

    roundStartTick = currentTick;

    std::vector<std::uint8_t> packet(19 + 14 * 3, 0);

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
            packet.data() + 19 + slot * 14;

        Wire::Write32(record, session->id);
        record[4] = static_cast<std::uint8_t>(slot);

        Wire::WriteFloat(record + 5, session->x);
        Wire::WriteFloat(record + 9, session->z);
        record[13] = static_cast<std::uint8_t>(session->characterId);
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

        if (!session ||
            !IsRoundMember(session->id))
        {
            continue;
        }

        lobbyRoom->players[slot].alive =
            session->alive;

        const auto offset = packet.size();
        packet.resize(offset + Wire::PlayerStateRecordSize);

        auto* record = packet.data() + offset;

        Wire::Write32(record, session->id);
        Wire::WriteFloat(record + 4, session->x);
        Wire::WriteFloat(record + 8, session->z);
        Wire::WriteFloat(record + 12, session->rotation);

        record[16] = session->alive ? 1 : 0;
        const auto* effects = items.GetPlayerEffects(session->id);
        Wire::Write32(record + 17,
            effects ? effects->shieldRemainingMs : 0);
        record[21] = static_cast<std::uint8_t>(session->deathReason);
        record[22] = static_cast<std::uint8_t>(session->characterId);
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

    std::array<
        std::uint8_t,
        Wire::WorldBeginSize
    > packet{};

    Wire::Header(
        packet.data(),
        packet.size(),
        Wire::WorldBegin,
        ++s.sendSeq,
        world.RoundId());

    Wire::Write32(
        packet.data() + 12,
        world.Generation());

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

bool ServerNetwork::QueueWorldLane(
    TransportSession& s,
    const WorldLaneData& lane)
{
    if (s.sendBuffer.size() +
        Wire::WorldLaneSize +
        Wire::StateSize > 16384)
    {
        return false;
    }

    std::array<
        std::uint8_t,
        Wire::WorldLaneSize
    > packet{};

    Wire::Header(
        packet.data(),
        packet.size(),
        Wire::WorldLane,
        ++s.sendSeq,
        world.RoundId());

    Wire::Write32(
        packet.data() + 12,
        world.Generation());

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

bool ServerNetwork::QueueWorldActor(
    TransportSession& s,
    const WorldActorData& actor,
    std::uint32_t tick)
{
    if (s.sendBuffer.size() +
        Wire::WorldActorSize +
        Wire::StateSize > 16384)
    {
        return false;
    }

    std::array<
        std::uint8_t,
        Wire::WorldActorSize
    > packet{};

    Wire::Header(
        packet.data(),
        packet.size(),
        Wire::WorldActor,
        ++s.sendSeq,
        world.RoundId());

    Wire::Write32(
        packet.data() + 12,
        world.Generation());

    Wire::Write32(packet.data() + 16, tick);
    Wire::Write32(packet.data() + 20, actor.id);

    packet[24] =
        static_cast<std::uint8_t>(actor.kind);

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

    std::array<
        std::uint8_t,
        Wire::WorldEndSize
    > packet{};

    Wire::Header(
        packet.data(),
        packet.size(),
        Wire::WorldEnd,
        ++s.sendSeq,
        world.RoundId());

    Wire::Write32(
        packet.data() + 12,
        world.Generation());

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
    s.itemSnapshotPending = true;

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

        while (s.snapshotNextZ <= s.snapshotMaxZ &&
            sent < kMaxLanesPerTick)
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

    if (s.itemSnapshotPending) {
        if (!QueueItemSnapshot(s)) {
            return;
        }

        s.itemSnapshotPending = false;
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

void ServerNetwork::TryBeginNextRound()
{
    if (!lobbyRoom ||
        lobbyRoom->phase != ServicePhase::PLAYING)
    {
        return;
    }

    for (const auto id : lobbyRoom->members) {
        const auto* session = FindSession(id);

        if (!session || !session->resetVote) {
            return;
        }
    }

    RoundController controller(*lobbyRoom);

    if (controller.ReloadRound()) {
        StartRoundSnapshots();
    }
}

void ServerNetwork::ResetWorld(std::uint32_t roundId)
{
    world.Reset(roundId);
    items.ResetRound();

    std::mt19937 rng(std::random_device{}());

    std::vector<
        std::pair<std::int32_t, std::int32_t>
    > locations;

    for (const auto& entry : world.Lanes()) {
        const auto z = entry.first;
        const auto& lane = entry.second;

        if (z >= -2 || lane.terrain != 0) {
            continue;
        }

        std::vector<std::int32_t> freeX;

        for (int x = -6; x <= 6; ++x) {
            if ((lane.treeMask & (1u << (x + 15))) == 0) {
                freeX.push_back(x);
            }
        }

        if (!freeX.empty()) {
            const auto index =
                std::uniform_int_distribution<std::size_t>(
                    0, freeX.size() - 1)(rng);

            locations.emplace_back(freeX[index], z);
        }
    }

    locations.emplace_back(
        std::uniform_int_distribution<int>(-6, 6)(rng),
        -1);

    std::shuffle(
        locations.begin(),
        locations.end(),
        rng);

    const auto count = (std::min<std::size_t>)(
        locations.size(),
        std::uniform_int_distribution<int>(3, 7)(rng));

    for (std::size_t i = 0; i < count; ++i) {
        const auto kind = static_cast<ItemKind>(
            std::uniform_int_distribution<int>(1, 4)(rng));

        items.AddGroundItem({
            static_cast<ObjectId>(i + 1),
            kind,
            locations[i].first,
            locations[i].second
            });
    }

    nextSpawnItemId =
        static_cast<ObjectId>(count + 1);

    for (std::size_t i = 0; i < sessionCount; ++i) {
        if (IsRoundMember(sessions[i].id)) {
            items.AddPlayer(sessions[i].id);
        }
    }
}

bool ServerNetwork::QueueItemSnapshot(TransportSession& s)
{
    const auto& ground = items.GetGroundItems();

    const std::size_t bytes =
        Wire::ItemBeginSize +
        ground.size() * Wire::ItemEntrySize +
        Wire::ItemEndSize +
        items.GetBananaTraps().size() *
        Wire::BananaPlacedSize;

    if (s.sendBuffer.size() +
        bytes +
        Wire::StateSize > 16384)
    {
        return false;
    }

    const auto append = [&s](const auto& packet) {
        s.sendBuffer.insert(
            s.sendBuffer.end(),
            packet.begin(),
            packet.end());
        };

    std::array<
        std::uint8_t,
        Wire::ItemBeginSize
    > begin{};

    Wire::Header(
        begin.data(),
        begin.size(),
        Wire::ItemBegin,
        ++s.sendSeq,
        world.RoundId());

    Wire::Write32(
        begin.data() + 12,
        static_cast<std::uint32_t>(ground.size()));

    append(begin);

    for (const auto& entry : ground) {
        const auto& item = entry.second;

        std::array<
            std::uint8_t,
            Wire::ItemEntrySize
        > packet{};

        Wire::Header(
            packet.data(),
            packet.size(),
            Wire::ItemEntry,
            ++s.sendSeq,
            world.RoundId());

        Wire::Write32(packet.data() + 12, item.id);

        packet[16] =
            static_cast<std::uint8_t>(item.kind);

        Wire::Write32(
            packet.data() + 17,
            static_cast<std::uint32_t>(item.x));

        Wire::Write32(
            packet.data() + 21,
            static_cast<std::uint32_t>(item.z));

        append(packet);
    }

    std::array<
        std::uint8_t,
        Wire::ItemEndSize
    > end{};

    Wire::Header(
        end.data(),
        end.size(),
        Wire::ItemEnd,
        ++s.sendSeq,
        world.RoundId());

    append(end);

    for (const auto& entry : items.GetBananaTraps()) {
        if (!QueueBananaPlaced(s, entry.second)) {
            return false;
        }
    }

    return true;
}

bool ServerNetwork::QueueItemRemoved(
    TransportSession& s,
    const PickupResult& result)
{
    if (s.sendBuffer.size() +
        Wire::ItemRemovedSize +
        Wire::StateSize > 16384)
    {
        return false;
    }

    std::array<
        std::uint8_t,
        Wire::ItemRemovedSize
    > packet{};

    Wire::Header(
        packet.data(),
        packet.size(),
        Wire::ItemRemoved,
        ++s.sendSeq,
        world.RoundId());

    Wire::Write32(packet.data() + 12, result.objectId);
    Wire::Write32(packet.data() + 16, result.playerId);
    packet[20] = result.slot;

    s.sendBuffer.insert(
        s.sendBuffer.end(),
        packet.begin(),
        packet.end());

    return true;
}

bool ServerNetwork::QueueInventoryState(
    TransportSession& s,
    bool accepted)
{
    const auto* inventory = items.GetInventory(s.id);

    if (!inventory ||
        s.sendBuffer.size() +
        Wire::InventoryStateSize +
        Wire::StateSize > 16384)
    {
        return false;
    }

    std::array<
        std::uint8_t,
        Wire::InventoryStateSize
    > packet{};

    Wire::Header(
        packet.data(),
        packet.size(),
        Wire::InventoryState,
        ++s.sendSeq,
        world.RoundId());

    packet[12] = inventory->selectedSlot;
    packet[13] =
        static_cast<std::uint8_t>(inventory->slots[0]);
    packet[14] =
        static_cast<std::uint8_t>(inventory->slots[1]);
    packet[15] = accepted ? 1 : 0;
    packet[16] = 0;

    s.sendBuffer.insert(
        s.sendBuffer.end(),
        packet.begin(),
        packet.end());

    return true;
}

bool ServerNetwork::QueueItemEffect(
    TransportSession& s,
    ItemKind kind,
    std::uint32_t durationMs)
{
    if (s.sendBuffer.size() +
        Wire::ItemEffectSize +
        Wire::StateSize > 16384)
    {
        return false;
    }

    std::array<
        std::uint8_t,
        Wire::ItemEffectSize
    > packet{};

    Wire::Header(
        packet.data(),
        packet.size(),
        Wire::ItemEffect,
        ++s.sendSeq,
        world.RoundId());

    packet[12] = static_cast<std::uint8_t>(kind);
    Wire::Write32(packet.data() + 13, durationMs);

    s.sendBuffer.insert(
        s.sendBuffer.end(),
        packet.begin(),
        packet.end());

    return true;
}

bool ServerNetwork::QueueBananaPlaced(
    TransportSession& s,
    const BananaTrap& trap)
{
    if (s.sendBuffer.size() +
        Wire::BananaPlacedSize +
        Wire::StateSize > 16384)
    {
        return false;
    }

    std::array<
        std::uint8_t,
        Wire::BananaPlacedSize
    > packet{};

    Wire::Header(
        packet.data(),
        packet.size(),
        Wire::BananaPlaced,
        ++s.sendSeq,
        world.RoundId());

    Wire::Write32(packet.data() + 12, trap.id);
    Wire::Write32(packet.data() + 16, trap.ownerId);

    Wire::Write32(
        packet.data() + 20,
        static_cast<std::uint32_t>(trap.x));

    Wire::Write32(
        packet.data() + 24,
        static_cast<std::uint32_t>(trap.z));

    s.sendBuffer.insert(
        s.sendBuffer.end(),
        packet.begin(),
        packet.end());

    return true;
}

bool ServerNetwork::QueueBananaHit(
    TransportSession& s,
    const BananaHitResult& hit,
    PlayerId victimId,
    std::int32_t x,
    std::int32_t z)
{
    if (s.sendBuffer.size() +
        Wire::BananaHitSize +
        Wire::StateSize > 16384)
    {
        return false;
    }

    std::array<
        std::uint8_t,
        Wire::BananaHitSize
    > packet{};

    Wire::Header(
        packet.data(),
        packet.size(),
        Wire::BananaHit,
        ++s.sendSeq,
        world.RoundId());

    Wire::Write32(packet.data() + 12, hit.trapId);
    Wire::Write32(packet.data() + 16, victimId);

    Wire::Write32(
        packet.data() + 20,
        static_cast<std::uint32_t>(x));

    Wire::Write32(
        packet.data() + 24,
        static_cast<std::uint32_t>(z));

    packet[28] = hit.pushed ? 1 : 0;

    s.sendBuffer.insert(
        s.sendBuffer.end(),
        packet.begin(),
        packet.end());

    return true;
}

bool ServerNetwork::QueueItemSpawned(
    TransportSession& s,
    const GroundItem& item)
{
    if (s.sendBuffer.size() +
        Wire::ItemSpawnedSize +
        Wire::StateSize > 16384)
    {
        return false;
    }

    std::array<
        std::uint8_t,
        Wire::ItemSpawnedSize
    > packet{};

    Wire::Header(
        packet.data(),
        packet.size(),
        Wire::ItemSpawned,
        ++s.sendSeq,
        world.RoundId());

    Wire::Write32(packet.data() + 12, item.id);

    packet[16] =
        static_cast<std::uint8_t>(item.kind);

    Wire::Write32(
        packet.data() + 17,
        static_cast<std::uint32_t>(item.x));

    Wire::Write32(
        packet.data() + 21,
        static_cast<std::uint32_t>(item.z));

    s.sendBuffer.insert(
        s.sendBuffer.end(),
        packet.begin(),
        packet.end());

    return true;
}

bool ServerNetwork::HandleItemRequest(
    TransportSession& s,
    const std::uint8_t* p,
    std::size_t size)
{
    if (p[3] == Wire::PickupRequest &&
        size == Wire::PickupRequestSize)
    {
        if (Wire::ReadRoundId(p) == world.RoundId()) {
            bool snapshotsReady = true;

            for (std::size_t i = 0;
                i < sessionCount;
                ++i)
            {
                if (IsRoundMember(sessions[i].id) &&
                    (sessions[i].itemSnapshotPending ||
                        sessions[i].sendBuffer.size() +
                        Wire::ItemRemovedSize +
                        Wire::StateSize > 16384))
                {
                    snapshotsReady = false;
                }
            }

            if (snapshotsReady) {
                PickupResult result{};

                const PickupPlayer player{
                    s.id,
                    static_cast<std::int32_t>(
                        std::round(s.x)),
                    static_cast<std::int32_t>(
                        std::round(s.z)),
                    s.alive
                };

                if (items.TryPickup(
                    Wire::Read32(p + 12),
                    player,
                    result))
                {
                    for (std::size_t i = 0;
                        i < sessionCount;
                        ++i)
                    {
                        if (IsRoundMember(sessions[i].id)) {
                            QueueItemRemoved(
                                sessions[i],
                                result);
                        }
                    }
                }
            }
        }
    }
    else if (p[3] == Wire::ItemSpawnRequest &&
        size == Wire::ItemSpawnRequestSize)
    {
        if (Wire::ReadRoundId(p) == s.roundId &&
            s.alive &&
            p[12] >= 1 &&
            p[12] <= 4 &&
            nextSpawnItemId != 0 &&
            items.GetGroundItems().size() < 512)
        {
            bool ready = true;

            for (std::size_t i = 0;
                i < sessionCount;
                ++i)
            {
                if (IsRoundMember(sessions[i].id) &&
                    (sessions[i].itemSnapshotPending ||
                        sessions[i].sendBuffer.size() +
                        Wire::ItemSpawnedSize +
                        Wire::StateSize > 16384))
                {
                    ready = false;
                }
            }

            while (nextSpawnItemId != 0 &&
                (items.GetGroundItems().count(nextSpawnItemId) ||
                    items.GetBananaTraps().count(nextSpawnItemId)))
            {
                ++nextSpawnItemId;
            }

            if (nextSpawnItemId == 0) {
                return true;
            }

            const GroundItem item{
                nextSpawnItemId,
                static_cast<ItemKind>(p[12]),
                static_cast<std::int32_t>(
                    std::round(s.x)),
                static_cast<std::int32_t>(
                    std::round(s.z)) - 2
            };

            if (ready && items.AddGroundItem(item)) {
                ++nextSpawnItemId;

                for (std::size_t i = 0;
                    i < sessionCount;
                    ++i)
                {
                    if (IsRoundMember(sessions[i].id)) {
                        QueueItemSpawned(
                            sessions[i],
                            item);
                    }
                }
            }
        }
    }
    else if (p[3] == Wire::ItemSwitch &&
        size == Wire::ItemSwitchSize)
    {
        if (Wire::ReadRoundId(p) == s.roundId) {
            const bool accepted =
                s.alive &&
                items.SwitchSlot(s.id, p[12]);

            if (!QueueInventoryState(s, accepted)) {
                return false;
            }
        }
    }
    else if (p[3] == Wire::ItemUse &&
        size == Wire::ItemUseSize)
    {
        if (Wire::ReadRoundId(p) == s.roundId) {
            std::vector<TargetPlayer> players;

            for (std::size_t i = 0;
                i < sessionCount;
                ++i)
            {
                if (IsRoundMember(sessions[i].id)) {
                    players.push_back({
                        sessions[i].id,
                        sessions[i].alive
                        });
                }
            }

            const auto* inventory =
                items.GetInventory(s.id);

            const auto kind = inventory
                ? inventory->slots[inventory->selectedSlot]
                : ItemKind::Empty;

            const bool needsTarget =
                kind == ItemKind::ReverseControls ||
                kind == ItemKind::Blackout;

            const auto target = needsTarget
                ? items.SelectAttackTarget(s.id, players, attackTargetRng)
                : 0;

            TransportSession* targetSession = nullptr;

            if (needsTarget) {
                for (std::size_t i = 0;
                    i < sessionCount;
                    ++i)
                {
                    if (IsRoundMember(sessions[i].id) &&
                        sessions[i].id == target)
                    {
                        targetSession = &sessions[i];
                    }
                }
            }

            const bool targetReady =
                !needsTarget ||
                (targetSession &&
                    !targetSession->itemSnapshotPending &&
                    targetSession->sendBuffer.size() +
                    Wire::ItemEffectSize +
                    Wire::StateSize <= 16384);

            const bool shieldReady =
                kind != ItemKind::Shield ||
                (!s.itemSnapshotPending &&
                    s.sendBuffer.size() +
                    Wire::InventoryStateSize +
                    Wire::ItemEffectSize +
                    Wire::StateSize <= 16384);

            std::vector<ObjectId> previousTrapIds;

            if (kind == ItemKind::Banana) {
                for (const auto& trap : items.GetBananaTraps()) {
                    previousTrapIds.push_back(trap.first);
                }
            }

            const bool accepted =
                s.alive &&
                targetReady &&
                shieldReady &&
                items.UseSelectedItem(
                    s.id,
                    static_cast<std::int32_t>(
                        std::round(s.x)),
                    static_cast<std::int32_t>(
                        std::round(s.z)),
                    target);

            if (!QueueInventoryState(s, accepted)) {
                return false;
            }

            if (accepted &&
                needsTarget &&
                !QueueItemEffect(
                    *targetSession,
                    kind,
                    GetItemRule(kind).effectDurationMs))
            {
                return false;
            }

            if (accepted &&
                kind == ItemKind::Shield &&
                !QueueItemEffect(
                    s,
                    kind,
                    GetItemRule(kind).effectDurationMs))
            {
                return false;
            }

            if (accepted && kind == ItemKind::Banana) {
                const auto& traps =
                    items.GetBananaTraps();

                const auto trap = std::find_if(
                    traps.begin(),
                    traps.end(),
                    [&previousTrapIds](const auto& entry) {
                        return std::find(
                            previousTrapIds.begin(),
                            previousTrapIds.end(),
                            entry.first) == previousTrapIds.end();
                    });

                if (trap != traps.end()) {
                    for (std::size_t i = 0;
                        i < sessionCount;
                        ++i)
                    {
                        if (IsRoundMember(sessions[i].id) &&
                            !QueueBananaPlaced(
                                sessions[i],
                                trap->second))
                        {
                            return false;
                        }
                    }
                }
            }
        }
    }
    else {
        return false;
    }

    return true;
}
