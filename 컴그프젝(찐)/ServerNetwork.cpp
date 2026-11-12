#include "ServerNetwork.h"
#include <cstring>
#include <chrono>
#include <cmath>
#include <iostream>
#include <algorithm>
#include <random>

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
    world.Reset(1);
    items.ResetRound();
    std::mt19937 rng(std::random_device{}());
    std::vector<std::pair<std::int32_t, std::int32_t>> locations;
    for (const auto& entry : world.Lanes()) {
        const auto z = entry.first;
        const auto& lane = entry.second;
        if (z >= -2 || lane.terrain != 0) continue;
        std::vector<std::int32_t> freeX;
        for (int x = -6; x <= 6; ++x) {
            if ((lane.treeMask & (1u << (x + 15))) == 0) freeX.push_back(x);
        }
        if (!freeX.empty()) {
            const auto index = std::uniform_int_distribution<std::size_t>(0, freeX.size() - 1)(rng);
            locations.emplace_back(freeX[index], z);
        }
    }
    // 잔디 줄이 생성되지 않아도 시작 지점 근처에 획득 가능한 아이템을 배치한다.
    locations.emplace_back(std::uniform_int_distribution<int>(-6, 6)(rng), -1);
    std::shuffle(locations.begin(), locations.end(), rng);
    const auto count = std::min<std::size_t>(locations.size(),
        std::uniform_int_distribution<int>(3, 7)(rng));
    for (std::size_t i = 0; i < count; ++i) {
        const auto x = locations[i].first;
        const auto z = locations[i].second;
        const auto kind = static_cast<ItemKind>(std::uniform_int_distribution<int>(1, 4)(rng));
        items.AddGroundItem({static_cast<ObjectId>(i + 1), kind, x, z});
    }
    return true;
}

bool ServerNetwork::QueueItemSnapshot(TransportSession& s) {
    const auto& ground = items.GetGroundItems();
    const std::size_t bytes = Wire::ItemBeginSize + ground.size() * Wire::ItemEntrySize +
                              Wire::ItemEndSize;
    if (s.sendBuffer.size() + bytes + Wire::StateSize > 16384) return false;
    const auto append = [&s](const auto& packet) {
        s.sendBuffer.insert(s.sendBuffer.end(), packet.begin(), packet.end());
    };
    std::array<std::uint8_t, Wire::ItemBeginSize> begin{};
    Wire::Header(begin.data(), begin.size(), Wire::ItemBegin, ++s.sendSeq);
    Wire::Write32(begin.data() + 8, world.RoundId());
    Wire::Write32(begin.data() + 12, static_cast<std::uint32_t>(ground.size()));
    append(begin);
    for (const auto& entry : ground) {
        const auto& item = entry.second;
        std::array<std::uint8_t, Wire::ItemEntrySize> packet{};
        Wire::Header(packet.data(), packet.size(), Wire::ItemEntry, ++s.sendSeq);
        Wire::Write32(packet.data() + 8, world.RoundId());
        Wire::Write32(packet.data() + 12, item.id);
        packet[16] = static_cast<std::uint8_t>(item.kind);
        Wire::Write32(packet.data() + 17, static_cast<std::uint32_t>(item.x));
        Wire::Write32(packet.data() + 21, static_cast<std::uint32_t>(item.z));
        append(packet);
    }
    std::array<std::uint8_t, Wire::ItemEndSize> end{};
    Wire::Header(end.data(), end.size(), Wire::ItemEnd, ++s.sendSeq);
    Wire::Write32(end.data() + 8, world.RoundId());
    append(end);
    return true;
}

bool ServerNetwork::QueueItemRemoved(TransportSession& s, const PickupResult& result) {
    if (s.sendBuffer.size() + Wire::ItemRemovedSize + Wire::StateSize > 16384) return false;
    std::array<std::uint8_t, Wire::ItemRemovedSize> packet{};
    Wire::Header(packet.data(), packet.size(), Wire::ItemRemoved, ++s.sendSeq);
    Wire::Write32(packet.data() + 8, world.RoundId());
    Wire::Write32(packet.data() + 12, result.objectId);
    Wire::Write32(packet.data() + 16, result.playerId);
    packet[20] = result.slot;
    s.sendBuffer.insert(s.sendBuffer.end(), packet.begin(), packet.end());
    return true;
}

bool ServerNetwork::QueueWorldBegin(TransportSession& s) {
    if (s.sendBuffer.size() + Wire::WorldBeginSize + Wire::StateSize > 16384) return false;
    std::array<std::uint8_t, Wire::WorldBeginSize> packet{};
    Wire::Header(packet.data(), packet.size(), Wire::WorldBegin, ++s.sendSeq);
    Wire::Write32(packet.data() + 8, world.RoundId());
    Wire::Write32(packet.data() + 12, world.Generation());
    Wire::Write32(packet.data() + 16, static_cast<std::uint32_t>(s.snapshotMinZ));
    Wire::Write32(packet.data() + 20, static_cast<std::uint32_t>(s.snapshotMaxZ));
    s.sendBuffer.insert(s.sendBuffer.end(), packet.begin(), packet.end());
    return true;
}

bool ServerNetwork::QueueWorldLane(TransportSession& s, const WorldLaneData& lane) {
    if (s.sendBuffer.size() + Wire::WorldLaneSize + Wire::StateSize > 16384) return false;
    std::array<std::uint8_t, Wire::WorldLaneSize> packet{};
    Wire::Header(packet.data(), packet.size(), Wire::WorldLane, ++s.sendSeq);
    Wire::Write32(packet.data() + 8, world.RoundId());
    Wire::Write32(packet.data() + 12, world.Generation());
    Wire::Write32(packet.data() + 16, static_cast<std::uint32_t>(lane.z));
    packet[20] = lane.terrain;
    Wire::Write32(packet.data() + 21, lane.treeMask);
    Wire::Write32(packet.data() + 25, lane.padMask);
    Wire::Write32(packet.data() + 29, lane.spawnMask);
    s.sendBuffer.insert(s.sendBuffer.end(), packet.begin(), packet.end());
    return true;
}

bool ServerNetwork::QueueWorldActor(TransportSession& s, const WorldActorData& actor,
                                     std::uint32_t tick) {
    if (s.sendBuffer.size() + Wire::WorldActorSize + Wire::StateSize > 16384) return false;
    std::array<std::uint8_t, Wire::WorldActorSize> packet{};
    Wire::Header(packet.data(), packet.size(), Wire::WorldActor, ++s.sendSeq);
    Wire::Write32(packet.data() + 8, world.RoundId());
    Wire::Write32(packet.data() + 12, world.Generation());
    Wire::Write32(packet.data() + 16, tick);
    Wire::Write32(packet.data() + 20, actor.id);
    packet[24] = static_cast<std::uint8_t>(actor.kind);
    Wire::WriteFloat(packet.data() + 25, actor.z);
    Wire::WriteFloat(packet.data() + 29, actor.x);
    Wire::WriteFloat(packet.data() + 33, actor.speed);
    Wire::WriteFloat(packet.data() + 37, actor.width);
    packet[41] = actor.red; packet[42] = actor.green; packet[43] = actor.blue;
    packet[44] = actor.design; packet[45] = actor.trainState;
    packet[46] = static_cast<std::uint8_t>(actor.trainTimer >> 8);
    packet[47] = static_cast<std::uint8_t>(actor.trainTimer);
    packet[48] = actor.trainLight ? 1 : 0;
    s.sendBuffer.insert(s.sendBuffer.end(), packet.begin(), packet.end());
    return true;
}

bool ServerNetwork::QueueWorldEnd(TransportSession& s) {
    if (s.sendBuffer.size() + Wire::WorldEndSize + Wire::StateSize > 16384) return false;
    std::array<std::uint8_t, Wire::WorldEndSize> packet{};
    Wire::Header(packet.data(), packet.size(), Wire::WorldEnd, ++s.sendSeq);
    Wire::Write32(packet.data() + 8, world.RoundId());
    Wire::Write32(packet.data() + 12, world.Generation());
    s.sendBuffer.insert(s.sendBuffer.end(), packet.begin(), packet.end());
    return true;
}

void ServerNetwork::StartWorldSnapshot(TransportSession& s) {
    s.snapshotting = true;
    s.snapshotBeginPending = true;
    s.snapshotMinZ = world.MinZ();
    s.snapshotMaxZ = world.MaxZ();
    s.snapshotNextZ = s.snapshotMinZ;
    s.itemSnapshotPending = true;
}

void ServerNetwork::PumpWorldPackets(TransportSession& s, std::uint32_t tick) {
    constexpr int kMaxLanesPerTick = 12;
    if (s.snapshotting) {
        if (s.snapshotBeginPending) {
            if (!QueueWorldBegin(s)) return;
            s.snapshotBeginPending = false;
        }
        int sent = 0;
        while (s.snapshotNextZ <= s.snapshotMaxZ && sent < kMaxLanesPerTick) {
            const auto* lane = world.FindLane(s.snapshotNextZ);
            if (!lane || s.sendBuffer.size() + Wire::WorldLaneSize +
                lane->actorIds.size() * Wire::WorldActorSize + Wire::StateSize > 16384 ||
                !QueueWorldLane(s, *lane)) return;
            for (const auto id : lane->actorIds) {
                const auto* actor = world.FindActor(id);
                if (!actor || !QueueWorldActor(s, *actor, tick)) return;
            }
            ++s.snapshotNextZ;
            ++sent;
        }
        if (s.snapshotNextZ <= s.snapshotMaxZ || !QueueWorldEnd(s)) return;
        s.snapshotting = false;
        s.announcedMinZ = s.snapshotMinZ;
        s.announcedMaxZ = s.snapshotMaxZ;
    }

    if (s.itemSnapshotPending) {
        if (!QueueItemSnapshot(s)) return;
        s.itemSnapshotPending = false;
    }

    // 초기 스냅샷 구간 밖에 추가된 줄은 스냅샷 이후에 전송한다.
    // 클라이언트가 WorldBegin부터 WorldEnd까지 한 묶음으로 처리하도록 한다.
    int sent = 0;
    while (s.announcedMinZ > world.MinZ() && sent < kMaxLanesPerTick) {
        const auto* lane = world.FindLane(s.announcedMinZ - 1);
        if (!lane || s.sendBuffer.size() + Wire::WorldLaneSize +
            lane->actorIds.size() * Wire::WorldActorSize + Wire::StateSize > 16384 ||
            !QueueWorldLane(s, *lane)) return;
        for (const auto id : lane->actorIds) {
            const auto* actor = world.FindActor(id);
            if (!actor || !QueueWorldActor(s, *actor, tick)) return;
        }
        --s.announcedMinZ;
        ++sent;
    }
    while (s.announcedMaxZ < world.MaxZ() && sent < kMaxLanesPerTick) {
        const auto* lane = world.FindLane(s.announcedMaxZ + 1);
        if (!lane || s.sendBuffer.size() + Wire::WorldLaneSize +
            lane->actorIds.size() * Wire::WorldActorSize + Wire::StateSize > 16384 ||
            !QueueWorldLane(s, *lane)) return;
        for (const auto id : lane->actorIds) {
            const auto* actor = world.FindActor(id);
            if (!actor || !QueueWorldActor(s, *actor, tick)) return;
        }
        ++s.announcedMaxZ;
        ++sent;
    }
}

void ServerNetwork::TryBeginNextRound() {
    if (sessionCount == 0) return;
    for (std::size_t i = 0; i < sessionCount; ++i) {
        if (!sessions[i].resetVote) return;
    }
    world.Reset(world.RoundId() + 1);
    for (std::size_t i = 0; i < sessionCount; ++i) {
        auto& s = sessions[i];
        s.roundId = world.RoundId();
        s.x = s.z = 0;
        s.alive = true;
        s.inputSeq = 0;
        s.nextMoveTick = 0;
        s.status = Wire::MoveStatus::Idle;
        s.resetVote = false;
        StartWorldSnapshot(s);
    }
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

bool ServerNetwork::ProcessPackets(TransportSession& s, std::uint64_t tick) {
    auto& bytes = s.receiveBuffer;
    std::size_t offset = 0;
    while (bytes.size() - offset >= 2) {
        const auto* p = bytes.data() + offset;
        const std::size_t size = (std::size_t(p[0]) << 8) | p[1];
        if (size < 8 || size > 1024) return false;
        if (bytes.size() - offset < size) break; // TCP 조각의 나머지 바이트를 기다린다.
        if (p[2] != 1) return false;
        const auto seq = Wire::Read32(p + 4);
        if (Wire::Newer(seq, s.sequence)) {
            s.sequence = seq;
            if (p[3] == Wire::Reset && size == 12) {
                if (Wire::Read32(p + 8) == world.RoundId() + 1) s.resetVote = true;
            } else if (p[3] == Wire::WorldPosition && size == Wire::PositionSize) {
                // 환경에 따른 이동과 사망 판정은 아직 클라이언트에서 처리한다.
                const float x = Wire::ReadFloat(p + 12), z = Wire::ReadFloat(p + 16);
                if (Wire::Read32(p + 8) == s.roundId && std::isfinite(x) && std::isfinite(z) &&
                    std::abs(x) < 1000000 && std::abs(z) < 1000000 && p[20] <= 1) {
                    s.x = x; s.z = z; s.alive = p[20] != 0;
                }
            } else if (p[3] == Wire::PickupRequest && size == Wire::PickupRequestSize) {
                if (Wire::Read32(p + 8) == world.RoundId()) {
                    bool snapshotsReady = true;
                    for (std::size_t i = 0; i < sessionCount; ++i) {
                        if (sessions[i].itemSnapshotPending ||
                            sessions[i].sendBuffer.size() + Wire::ItemRemovedSize +
                            Wire::StateSize > 16384) snapshotsReady = false;
                    }
                    if (snapshotsReady) {
                        PickupResult result{};
                        const PickupPlayer player{s.id, static_cast<std::int32_t>(std::round(s.x)),
                                                  static_cast<std::int32_t>(std::round(s.z)), s.alive};
                        if (items.TryPickup(Wire::Read32(p + 12), player, result)) {
                            for (std::size_t i = 0; i < sessionCount; ++i)
                                QueueItemRemoved(sessions[i], result);
                        }
                    }
                }
            } else if (p[3] == 9 && size == 17) {
                const auto input = Wire::Read32(p + 12);
                if (Wire::Read32(p + 8) == s.roundId && Wire::Newer(input, s.inputSeq)) {
                    s.inputSeq = input;
                    if (!s.alive || p[16] > 3) s.status = Wire::MoveStatus::Invalid;
                    else if (tick < s.nextMoveTick) s.status = Wire::MoveStatus::Cooldown;
                    else {
                        s.x = std::round(s.x); s.z = std::round(s.z);
                        switch (static_cast<Direction>(p[16])) {
                        case Direction::UP: --s.z; break;
                        case Direction::DOWN: ++s.z; break;
                        case Direction::LEFT: --s.x; break;
                        case Direction::RIGHT: ++s.x; break;
                        }
                        s.nextMoveTick = tick + Wire::MoveCooldownTicks;
                        s.status = Wire::MoveStatus::Accepted;
                    }
                }
            } else return false;
        }
        offset += size;
    }
    bytes.erase(bytes.begin(), bytes.begin() + offset);
    return true;
}

void ServerNetwork::Tick(std::uint64_t tick) {
    for (std::size_t i = sessionCount; i > 0; --i) {
        auto& s = sessions[i - 1];
        if (!ProcessPackets(s, tick)) {
            RemoveSession(i - 1); continue;
        }
    }
    TryBeginNextRound();

    // 틱마다 정해진 수의 새 줄만 생성한다.
    // 어느 플레이어가 전진했든 모든 클라이언트에 같은 저장된 줄을 전달한다.
    std::int32_t desiredMinZ = world.MinZ(), desiredMaxZ = world.MaxZ();
    for (std::size_t i = 0; i < sessionCount; ++i) {
        const auto z = static_cast<std::int32_t>(std::floor(sessions[i].z));
        desiredMinZ = (std::min)(desiredMinZ, z - 30);
        desiredMaxZ = (std::max)(desiredMaxZ, z + 10);
    }
    world.EnsureRange(desiredMinZ, desiredMaxZ, 8);
    world.Step(tick);

    for (std::size_t i = sessionCount; i > 0; --i) {
        auto& s = sessions[i - 1];
        PumpWorldPackets(s, static_cast<std::uint32_t>(tick));
        if (!s.snapshotting && tick % 3 == 0) {
            for (const auto& entry : world.Actors()) {
                const auto& actor = entry.second;
                if (actor.z < s.z - 30 || actor.z > s.z + 10) continue;
                if (!QueueWorldActor(s, actor, static_cast<std::uint32_t>(tick))) break;
            }
        }
        if (s.sendBuffer.size() + Wire::StateSize > 16384) {
            RemoveSession(i - 1); continue;
        }
        std::array<std::uint8_t, Wire::StateSize> packet{};
        Wire::Header(packet.data(), packet.size(), Wire::State, ++s.sendSeq);
        Wire::Write32(packet.data() + 8, s.roundId);
        Wire::Write32(packet.data() + 12, s.inputSeq);
        Wire::Write32(packet.data() + 16, static_cast<std::uint32_t>(tick));
        Wire::WriteFloat(packet.data() + 20, s.x); Wire::WriteFloat(packet.data() + 24, s.z);
        packet[28] = static_cast<std::uint8_t>(s.status);
        s.sendBuffer.insert(s.sendBuffer.end(), packet.begin(), packet.end());
        if (tick % Wire::TickRate == 0) {
            std::cout << "[TICK " << tick << "] player=" << s.id << " round=" << s.roundId
                << " x=" << s.x << " z=" << s.z << " alive=" << s.alive << '\n';
        }
        if (!Flush(s)) RemoveSession(i - 1);
    }
    std::cout.flush();
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
                    auto& s = sessions[sessionCount++]; s = TransportSession{};
                    s.id = nextSessionId++; s.socket = client;
                    s.roundId = world.RoundId();
                    items.AddPlayer(s.id);
                    std::array<std::uint8_t, Wire::ItemIdentitySize> identity{};
                    Wire::Header(identity.data(), identity.size(), Wire::ItemIdentity, ++s.sendSeq);
                    Wire::Write32(identity.data() + 8, s.id);
                    s.sendBuffer.insert(s.sendBuffer.end(), identity.begin(), identity.end());
                    StartWorldSnapshot(s);
                    std::cout << "TCP session connected: " << s.id << std::endl;
                }
            }
        }
        // 밀린 고정 간격 틱을 처리한다. 입출력과 게임 상태는 같은 스레드가 소유한다.
        while (!stopping.load() && Clock::now() >= nextTick) {
            Tick(++tick); nextTick += interval;
        }
    }
}

void ServerNetwork::RemoveSession(std::size_t index) {
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
