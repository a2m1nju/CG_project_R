#include "ServerNetwork.h"
#include "Room.h"
#include <algorithm>
#include <cstring>
#include <chrono>
#include <cmath>
#include <iostream>
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
    return true;
}

void ServerNetwork::ResetWorld(std::uint32_t roundId) {
    world.Reset(roundId);
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
    nextSpawnItemId = static_cast<ObjectId>(count + 1);
    for (std::size_t i = 0; i < sessionCount; ++i)
        if (sessions[i].joined) items.AddPlayer(sessions[i].id);
}

bool ServerNetwork::QueueItemSnapshot(TransportSession& s) {
    const auto& ground = items.GetGroundItems();
    const std::size_t bytes = Wire::ItemBeginSize + ground.size() * Wire::ItemEntrySize +
                              Wire::ItemEndSize + items.GetBananaTraps().size() * Wire::BananaPlacedSize;
    if (s.sendBuffer.size() + bytes + Wire::StateSize > 16384) return false;
    const auto append = [&s](const auto& packet) {
        s.sendBuffer.insert(s.sendBuffer.end(), packet.begin(), packet.end());
    };
    std::array<std::uint8_t, Wire::ItemBeginSize> begin{};
    Wire::Header(begin.data(), begin.size(), Wire::ItemBegin, ++s.sendSeq, world.RoundId());

    Wire::Write32(begin.data() + 12, static_cast<std::uint32_t>(ground.size()));
    append(begin);
    for (const auto& entry : ground) {
        const auto& item = entry.second;
        std::array<std::uint8_t, Wire::ItemEntrySize> packet{};
        Wire::Header(packet.data(), packet.size(), Wire::ItemEntry, ++s.sendSeq, world.RoundId());

        Wire::Write32(packet.data() + 12, item.id);
        packet[16] = static_cast<std::uint8_t>(item.kind);
        Wire::Write32(packet.data() + 17, static_cast<std::uint32_t>(item.x));
        Wire::Write32(packet.data() + 21, static_cast<std::uint32_t>(item.z));
        append(packet);
    }
    std::array<std::uint8_t, Wire::ItemEndSize> end{};
    Wire::Header(end.data(), end.size(), Wire::ItemEnd, ++s.sendSeq, world.RoundId());

    append(end);
    for (const auto& entry : items.GetBananaTraps())
        if (!QueueBananaPlaced(s, entry.second)) return false;
    return true;
}

bool ServerNetwork::QueueItemRemoved(TransportSession& s, const PickupResult& result) {
    if (s.sendBuffer.size() + Wire::ItemRemovedSize + Wire::StateSize > 16384) return false;
    std::array<std::uint8_t, Wire::ItemRemovedSize> packet{};
    Wire::Header(packet.data(), packet.size(), Wire::ItemRemoved, ++s.sendSeq, world.RoundId());

    Wire::Write32(packet.data() + 12, result.objectId);
    Wire::Write32(packet.data() + 16, result.playerId);
    packet[20] = result.slot;
    s.sendBuffer.insert(s.sendBuffer.end(), packet.begin(), packet.end());
    return true;
}

bool ServerNetwork::QueueInventoryState(TransportSession& s, bool accepted) {
    const auto* inventory = items.GetInventory(s.id);
    if (!inventory || s.sendBuffer.size() + Wire::InventoryStateSize + Wire::StateSize > 16384)
        return false;
    std::array<std::uint8_t, Wire::InventoryStateSize> packet{};
    Wire::Header(packet.data(), packet.size(), Wire::InventoryState, ++s.sendSeq, world.RoundId());

    packet[12] = inventory->selectedSlot;
    packet[13] = static_cast<std::uint8_t>(inventory->slots[0]);
    packet[14] = static_cast<std::uint8_t>(inventory->slots[1]);
    packet[15] = accepted ? 1 : 0;
    packet[16] = 0;
    s.sendBuffer.insert(s.sendBuffer.end(), packet.begin(), packet.end());
    return true;
}

bool ServerNetwork::QueueItemEffect(TransportSession& s, ItemKind kind,
                                    std::uint32_t durationMs) {
    if (s.sendBuffer.size() + Wire::ItemEffectSize + Wire::StateSize > 16384)
        return false;
    std::array<std::uint8_t, Wire::ItemEffectSize> packet{};
    Wire::Header(packet.data(), packet.size(), Wire::ItemEffect, ++s.sendSeq, world.RoundId());

    packet[12] = static_cast<std::uint8_t>(kind);
    Wire::Write32(packet.data() + 13, durationMs);
    s.sendBuffer.insert(s.sendBuffer.end(), packet.begin(), packet.end());
    return true;
}

bool ServerNetwork::QueueBananaPlaced(TransportSession& s, const BananaTrap& trap) {
    if (s.sendBuffer.size() + Wire::BananaPlacedSize + Wire::StateSize > 16384)
        return false;
    std::array<std::uint8_t, Wire::BananaPlacedSize> packet{};
    Wire::Header(packet.data(), packet.size(), Wire::BananaPlaced, ++s.sendSeq, world.RoundId());

    Wire::Write32(packet.data() + 12, trap.id);
    Wire::Write32(packet.data() + 16, trap.ownerId);
    Wire::Write32(packet.data() + 20, static_cast<std::uint32_t>(trap.x));
    Wire::Write32(packet.data() + 24, static_cast<std::uint32_t>(trap.z));
    s.sendBuffer.insert(s.sendBuffer.end(), packet.begin(), packet.end());
    return true;
}

bool ServerNetwork::QueueBananaHit(TransportSession& s, const BananaHitResult& hit,
                                   PlayerId victimId, std::int32_t x, std::int32_t z) {
    if (s.sendBuffer.size() + Wire::BananaHitSize + Wire::StateSize > 16384)
        return false;
    std::array<std::uint8_t, Wire::BananaHitSize> packet{};
    Wire::Header(packet.data(), packet.size(), Wire::BananaHit, ++s.sendSeq, world.RoundId());

    Wire::Write32(packet.data() + 12, hit.trapId);
    Wire::Write32(packet.data() + 16, victimId);
    Wire::Write32(packet.data() + 20, static_cast<std::uint32_t>(x));
    Wire::Write32(packet.data() + 24, static_cast<std::uint32_t>(z));
    packet[28] = hit.pushed ? 1 : 0;
    s.sendBuffer.insert(s.sendBuffer.end(), packet.begin(), packet.end());
    return true;
}

bool ServerNetwork::QueueItemSpawned(TransportSession& s, const GroundItem& item) {
    if (s.sendBuffer.size() + Wire::ItemSpawnedSize + Wire::StateSize > 16384)
        return false;
    std::array<std::uint8_t, Wire::ItemSpawnedSize> packet{};
    Wire::Header(packet.data(), packet.size(), Wire::ItemSpawned, ++s.sendSeq, world.RoundId());

    Wire::Write32(packet.data() + 12, item.id);
    packet[16] = static_cast<std::uint8_t>(item.kind);
    Wire::Write32(packet.data() + 17, static_cast<std::uint32_t>(item.x));
    Wire::Write32(packet.data() + 21, static_cast<std::uint32_t>(item.z));
    s.sendBuffer.insert(s.sendBuffer.end(), packet.begin(), packet.end());
    return true;
}

bool ServerNetwork::QueueWorldBegin(TransportSession& s) {
    if (s.sendBuffer.size() + Wire::WorldBeginSize + Wire::StateSize > 16384) return false;
    std::array<std::uint8_t, Wire::WorldBeginSize> packet{};
    Wire::Header(packet.data(), packet.size(), Wire::WorldBegin, ++s.sendSeq, world.RoundId());

    Wire::Write32(packet.data() + 12, world.Generation());
    Wire::Write32(packet.data() + 16, static_cast<std::uint32_t>(s.snapshotMinZ));
    Wire::Write32(packet.data() + 20, static_cast<std::uint32_t>(s.snapshotMaxZ));
    s.sendBuffer.insert(s.sendBuffer.end(), packet.begin(), packet.end());
    return true;
}

bool ServerNetwork::QueueWorldLane(TransportSession& s, const WorldLaneData& lane) {
    if (s.sendBuffer.size() + Wire::WorldLaneSize + Wire::StateSize > 16384) return false;
    std::array<std::uint8_t, Wire::WorldLaneSize> packet{};
    Wire::Header(packet.data(), packet.size(), Wire::WorldLane, ++s.sendSeq, world.RoundId());

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
    Wire::Header(packet.data(), packet.size(), Wire::WorldActor, ++s.sendSeq, world.RoundId());

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
    Wire::Header(packet.data(), packet.size(), Wire::WorldEnd, ++s.sendSeq, world.RoundId());

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

// minju의 공유 월드·아이템 전송을 같은 경기 번호로 시작한다.
void ServerNetwork::StartRoundSnapshots() {
    const auto& room = matchCoordinator.GetRoomState();
    ResetWorld(room.roundId);
    for (std::size_t i = 0; i < sessionCount; ++i) {
        auto& s = sessions[i];
        if (!s.joined || std::find(room.members.begin(), room.members.end(), s.id) == room.members.end()) continue;
        s.roundId = room.roundId;
        s.x = s.z = 0;
        s.alive = true;
        s.inputSeq = 0; s.nextMoveTick = 0;
        s.status = Wire::MoveStatus::Idle;
        s.resetVote = s.checkBanana = s.awaitingBananaPosition = false;
        std::array<std::uint8_t, Wire::ItemIdentitySize> identity{};
        Wire::Header(identity.data(), identity.size(), Wire::ItemIdentity, ++s.sendSeq, s.roundId);
        Wire::Write32(identity.data() + Wire::HeaderSize, s.playerId);
        s.sendBuffer.insert(s.sendBuffer.end(), identity.begin(), identity.end());
        StartWorldSnapshot(s);
    }
}

// 클라이언트가 맵과 아이템 적용을 마쳤는지 기록하고 전원 완료 후 시작한다.
void ServerNetwork::HandleLoaded(TransportSession& s, const std::uint8_t* p, std::size_t size) {
    if (!s.joined || size != Wire::HeaderSize || s.roundId == 0 ||
        Wire::ReadRoundId(p) != s.roundId || s.snapshotting || s.itemSnapshotPending) return;
    if (!roundController.MarkLoaded(s.playerId, s.roundId) ||
        matchCoordinator.GetRoomState().phase != ServicePhase::PLAYING) return;
    for (std::size_t i = 0; i < sessionCount; ++i) {
        auto& member = sessions[i];
        if (!member.joined || member.roundId != s.roundId) continue;
        std::array<std::uint8_t, Wire::HeaderSize> packet{};
        Wire::Header(packet.data(), packet.size(), Wire::Start, ++member.sendSeq, member.roundId);
        member.sendBuffer.insert(member.sendBuffer.end(), packet.begin(), packet.end());
    }
    std::cout << "START round=" << s.roundId << std::endl;
}

void ServerNetwork::TryBeginNextRound() {
    bool hasPlayer = false;
    for (std::size_t i = 0; i < sessionCount; ++i) {
        if (!sessions[i].joined) continue;
        hasPlayer = true;
        if (!sessions[i].resetVote) return;
    }
    if (!hasPlayer) return;
    if (roundController.ReloadRound()) {
        StartRoundSnapshots();
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

bool ServerNetwork::HandleJoin(TransportSession& s, const std::uint8_t* p,
                               std::size_t size, std::uint32_t sequence) {
    std::uint16_t error = 0;
    if (Wire::ReadRoundId(p) != 0 || size < Wire::HeaderSize + 1) {
        error = Wire::JoinBadPayload;
    }
    else {
        const auto nicknameSize = p[Wire::HeaderSize];
        if (nicknameSize == 0 || nicknameSize > Wire::MaxNicknameBytes ||
            size != Wire::HeaderSize + 1 + nicknameSize) { //닉네임 길이와 본문 크기 확인
            error = Wire::JoinBadPayload;
        }
    }

    if (error == 0 && s.joined) {
        error = Wire::JoinDuplicate; //같은 연결의 중복 참가 방지
    }
    if (error == 0) {
        const auto result = matchCoordinator.Enqueue(s.id);
        if (result.code == 0) {
            s.joined = true; //송신 실패 시에도 RemoveSession에서 참가 상태 정리
            s.playerId = result.playerId;
            s.nickname.assign(reinterpret_cast<const char*>(p + Wire::HeaderSize + 1),
                              p[Wire::HeaderSize]); //길이를 확인한 닉네임 보관

            std::array<std::uint8_t, Wire::WelcomeSize> packet{};
            if (s.sendBuffer.size() + packet.size() > 16384) return false;
            Wire::Header(packet.data(), packet.size(), Wire::Welcome, ++s.sendSeq, 0);
            Wire::Write32(packet.data() + Wire::HeaderSize, s.id);
            Wire::Write32(packet.data() + Wire::HeaderSize + 4, s.playerId);
            s.sendBuffer.insert(s.sendBuffer.end(), packet.begin(), packet.end());
            matchCoordinator.TryPromote(); //승인 응답을 큐에 넣은 뒤 대기실 구성
            matchStateDirty = true; //이번 틱의 최종 방 상태 전송 예약
            return true;
        }
        error = result.code == 2 ? Wire::JoinDuplicate : Wire::JoinBusy;
    }

    return QueueRequestError(s, Wire::Join, sequence, error);
}

bool ServerNetwork::QueueRequestError(TransportSession& s, std::uint8_t type,
                                      std::uint32_t sequence, std::uint16_t error) {
    std::array<std::uint8_t, Wire::ErrorSize> packet{};
    if (s.sendBuffer.size() + packet.size() > 16384) return false;
    Wire::Header(packet.data(), packet.size(), Wire::Error, ++s.sendSeq, 0);
    packet[12] = static_cast<std::uint8_t>(error >> 8);
    packet[13] = static_cast<std::uint8_t>(error);
    packet[14] = type;
    Wire::Write32(packet.data() + 15, sequence); //거절된 요청의 순번
    s.sendBuffer.insert(s.sendBuffer.end(), packet.begin(), packet.end());
    return true;
}

bool ServerNetwork::HandleReady(TransportSession& s, const std::uint8_t* p,
                                std::size_t size, std::uint32_t sequence) {
    if (size != Wire::ReadySize || Wire::ReadRoundId(p) != 0 ||
        p[Wire::HeaderSize + 4] > 1) {
        return QueueRequestError(s, Wire::Ready, sequence, Wire::JoinBadPayload);
    }
    if (!s.joined) return QueueRequestError(s, Wire::Ready, sequence, Wire::ReadyNotMember);

    const auto epoch = Wire::Read32(p + Wire::HeaderSize);
    const bool ready = p[Wire::HeaderSize + 4] != 0;
    switch (matchCoordinator.SetReady(s.id, epoch, ready)) {
    case ReadyResult::Accepted:
        matchStateDirty = true; //서버에서 확정한 준비값을 전체 멤버에게 통지
        return true;
    case ReadyResult::BadPhase:
        return QueueRequestError(s, Wire::Ready, sequence, Wire::ReadyBadPhase);
    case ReadyResult::Stale:
        return QueueRequestError(s, Wire::Ready, sequence, Wire::ReadyStale);
    case ReadyResult::NotMember:
        return QueueRequestError(s, Wire::Ready, sequence, Wire::ReadyNotMember);
    }
    return false;
}

bool ServerNetwork::ProcessPackets(TransportSession& s, std::uint64_t tick) {
    auto& bytes = s.receiveBuffer;
    std::size_t offset = 0;
    while (bytes.size() - offset >= 2) {
        const auto* p = bytes.data() + offset;
        const std::size_t size = (std::size_t(p[0]) << 8) | p[1];
        if (size < Wire::HeaderSize || size > 1024) return false;
        if (bytes.size() - offset < size) break; // TCP fragment
        if (p[2] != 1) return false;
        const auto seq = Wire::Read32(p + 4);
        const auto roundId = Wire::ReadRoundId(p);
        if (Wire::Newer(seq, s.sequence)) {
            s.sequence = seq;
            if (p[3] == Wire::Join) {
                if (!HandleJoin(s, p, size, seq)) return false;
            } else if (p[3] == Wire::Ready) {
                if (!HandleReady(s, p, size, seq)) return false;
            } else if (p[3] == Wire::Loaded) {
                HandleLoaded(s, p, size);
            } else if (!s.joined) {
                return false; //참가 승인 전 경기 패킷 차단
            } else if (p[3] == Wire::Reset && size == 12) {
                if (matchCoordinator.GetRoomState().phase == ServicePhase::PLAYING &&
                    Wire::Read32(p + 8) == world.RoundId() + 1) s.resetVote = true;
            } else if (matchCoordinator.GetRoomState().phase != ServicePhase::PLAYING) {
                // 로딩 중에는 완료 요청만 처리하고 경기 입력은 기다린다.
            } else if (p[3] == Wire::WorldPosition && size == Wire::PositionSize) {
                // 환경에 따른 이동과 사망 판정은 아직 클라이언트에서 처리한다.
                const float x = Wire::ReadFloat(p + 12), z = Wire::ReadFloat(p + 16);
                if (roundId == s.roundId && std::isfinite(x) && std::isfinite(z) &&
                    std::abs(x) < 1000000 && std::abs(z) < 1000000 && p[20] <= 1) {
                    if (!s.awaitingBananaPosition ||
                        (std::abs(x - s.x) < 0.01f && std::abs(z - s.z) < 0.01f)) {
                        s.x = x; s.z = z;
                        s.awaitingBananaPosition = false;
                    }
                    s.alive = p[20] != 0;
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
            } else if (p[3] == Wire::ItemSpawnRequest && size == Wire::ItemSpawnRequestSize) {
                if (Wire::Read32(p + 8) == s.roundId && s.alive &&
                    p[12] >= 1 && p[12] <= 4 && nextSpawnItemId != 0 &&
                    items.GetGroundItems().size() < 512) {
                    bool ready = true;
                    for (std::size_t i = 0; i < sessionCount; ++i)
                        if (sessions[i].itemSnapshotPending ||
                            sessions[i].sendBuffer.size() + Wire::ItemSpawnedSize +
                                Wire::StateSize > 16384) ready = false;
                    const GroundItem item{nextSpawnItemId,
                        static_cast<ItemKind>(p[12]),
                        static_cast<std::int32_t>(std::round(s.x)),
                        static_cast<std::int32_t>(std::round(s.z)) - 2};
                    if (ready && items.AddGroundItem(item)) {
                        ++nextSpawnItemId;
                        for (std::size_t i = 0; i < sessionCount; ++i)
                            QueueItemSpawned(sessions[i], item);
                    }
                }
            } else if (p[3] == Wire::ItemSwitch && size == Wire::ItemSwitchSize) {
                if (Wire::Read32(p + 8) == s.roundId) {
                    const bool accepted = s.alive && items.SwitchSlot(s.id, p[12]);
                    if (!QueueInventoryState(s, accepted)) return false;
                }
            } else if (p[3] == Wire::ItemUse && size == Wire::ItemUseSize) {
                if (Wire::Read32(p + 8) == s.roundId) {
                    std::vector<TargetPlayer> players;
                    for (std::size_t i = 0; i < sessionCount; ++i)
                        players.push_back({sessions[i].id,
                            static_cast<std::int32_t>(std::round(sessions[i].z * 1000)),
                            sessions[i].alive});
                    const auto* inventory = items.GetInventory(s.id);
                    const auto kind = inventory ? inventory->slots[inventory->selectedSlot]
                        : ItemKind::Empty;
                    const auto target = items.SelectAttackTarget(s.id, players);
                    const bool needsTarget = kind == ItemKind::ReverseControls ||
                        kind == ItemKind::Blackout;
                    TransportSession* targetSession = nullptr;
                    if (needsTarget) {
                        for (std::size_t i = 0; i < sessionCount; ++i)
                            if (sessions[i].id == target) targetSession = &sessions[i];
                    }
                    const bool targetReady = !needsTarget || (targetSession &&
                        !targetSession->itemSnapshotPending &&
                        targetSession->sendBuffer.size() + Wire::ItemEffectSize +
                            Wire::StateSize <= 16384);
                    const bool shieldReady = kind != ItemKind::Shield ||
                        (!s.itemSnapshotPending &&
                         s.sendBuffer.size() + Wire::InventoryStateSize +
                             Wire::ItemEffectSize + Wire::StateSize <= 16384);
                    std::vector<ObjectId> previousTrapIds;
                    if (kind == ItemKind::Banana)
                        for (const auto& trap : items.GetBananaTraps())
                            previousTrapIds.push_back(trap.first);
                    const bool accepted = s.alive && targetReady && shieldReady &&
                        items.UseSelectedItem(s.id,
                        static_cast<std::int32_t>(std::round(s.x)),
                        static_cast<std::int32_t>(std::round(s.z)), target);
                    if (!QueueInventoryState(s, accepted)) return false;
                    if (accepted && needsTarget && !QueueItemEffect(*targetSession, kind,
                        GetItemRule(kind).effectDurationMs)) return false;
                    if (accepted && kind == ItemKind::Shield &&
                        !QueueItemEffect(s, kind, GetItemRule(kind).effectDurationMs))
                        return false;
                    if (accepted && kind == ItemKind::Banana) {
                        const auto& traps = items.GetBananaTraps();
                        const auto trap = std::find_if(traps.begin(), traps.end(),
                            [&previousTrapIds](const auto& entry) {
                                return std::find(previousTrapIds.begin(), previousTrapIds.end(),
                                    entry.first) == previousTrapIds.end();
                            });
                        if (trap != traps.end())
                            for (std::size_t i = 0; i < sessionCount; ++i)
                                if (!QueueBananaPlaced(sessions[i], trap->second)) return false;
                    }
                }
            } else if (p[3] == 9 && size == 17) {
                const auto input = Wire::Read32(p + 12);
                if (matchCoordinator.GetRoomState().phase == ServicePhase::PLAYING &&
                    roundId == s.roundId && Wire::Newer(input, s.inputSeq)) {
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
                        s.checkBanana = true;
                    }
                }
            } else return false;
        }
        offset += size;
    }
    bytes.erase(bytes.begin(), bytes.begin() + offset);
    return true;
}

void ServerNetwork::BroadcastMatchState() {
    matchStateDirty = false;
    const auto& room = matchCoordinator.GetRoomState();
    std::vector<std::uint8_t> lobby;
    if (room.phase == ServicePhase::READY_LOBBY) {
        lobby.resize(Wire::LobbyPrefixSize);
        Wire::Write32(lobby.data() + Wire::HeaderSize, room.lobbyEpoch);
        lobby[16] = room.lobbyEpoch == lastBroadcastLobbyEpoch
            ? Wire::LobbyUpdated : Wire::LobbyEntered;
        lobby[17] = static_cast<std::uint8_t>(room.members.size());
        for (std::size_t i = 0; i < room.members.size(); ++i) {
            const auto end = sessions.begin() + sessionCount;
            const auto member = std::find_if(sessions.begin(), end, [&](const auto& s) {
                return s.joined && s.id == room.members[i];
            });
            if (member == end) return;

            const auto offset = lobby.size();
            lobby.resize(offset + Wire::LobbyMemberPrefixSize);
            Wire::Write32(lobby.data() + offset, member->playerId);
            lobby[offset + 4] = 0; //캐릭터 선택 전 기본값
            lobby[offset + 5] = room.ready[i] ? 1 : 0;
            lobby[offset + 6] = static_cast<std::uint8_t>(member->nickname.size());
            lobby.insert(lobby.end(), member->nickname.begin(), member->nickname.end());
        }
    }

    if (!lobby.empty()) lastBroadcastLobbyEpoch = room.lobbyEpoch;
    std::array<bool, 8> overflow{};
    for (std::size_t i = 0; i < sessionCount; ++i) {
        auto& s = sessions[i];
        if (!s.joined) continue;
        const auto queueMember = std::find(room.queue.begin(), room.queue.end(), s.id);
        std::array<std::uint8_t, Wire::QueueStateSize> queuePacket{};
        std::uint8_t* packet = nullptr;
        std::size_t size = 0;
        std::uint8_t type = 0;
        if (room.phase == ServicePhase::MATCHING && queueMember != room.queue.end()) {
            queuePacket[13] = static_cast<std::uint8_t>(queueMember - room.queue.begin() + 1);
            queuePacket[15] = static_cast<std::uint8_t>(room.queue.size());
            queuePacket[16] = Wire::MatchingPhase;
            packet = queuePacket.data();
            size = queuePacket.size();
            type = Wire::QueueState;
        } else if (!lobby.empty() &&
                   std::find(room.members.begin(), room.members.end(), s.id) != room.members.end()) {
            packet = lobby.data();
            size = lobby.size();
            type = Wire::ReadyLobbyState;
        }
        if (!packet) continue;
        if (s.sendBuffer.size() + size > 16384) {
            overflow[i] = true;
            continue;
        }
        Wire::Header(packet, size, type, ++s.sendSeq, 0); //경기 전 상태 통지
        s.sendBuffer.insert(s.sendBuffer.end(), packet, packet + size);
    }
    // 통지를 모두 큐에 넣은 뒤 슬롯을 회수
    for (std::size_t i = sessionCount; i > 0; --i) {
        if (overflow[i - 1]) RemoveSession(i - 1);
    }
}

void ServerNetwork::Tick(std::uint64_t tick) {
    items.AdvanceEffects(tick % 3 == 0 ? 34 : 33);
    for (std::size_t i = sessionCount; i > 0; --i) {
        auto& s = sessions[i - 1];
        if (!ProcessPackets(s, tick)) {
            RemoveSession(i - 1); continue;
        }
    }
    if (matchStateDirty) BroadcastMatchState();
    if (roundController.BeginRound(false)) { //전원 준비 판단은 방 전환 함수에서 처리
        StartRoundSnapshots();
    }
    TryBeginNextRound();

    if (matchCoordinator.GetRoomState().phase == ServicePhase::PLAYING) {
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
    }

    for (std::size_t i = sessionCount; i > 0; --i) {
        auto& s = sessions[i - 1];
        if (!s.joined) { if (!Flush(s)) RemoveSession(i - 1); continue; }
        if (s.roundId != 0) PumpWorldPackets(s, static_cast<std::uint32_t>(tick));
        if (s.roundId != 0 && !s.snapshotting && tick % 3 == 0) {
            for (const auto& entry : world.Actors()) {
                const auto& actor = entry.second;
                if (actor.z < s.z - 30 || actor.z > s.z + 10) continue;
                if (!QueueWorldActor(s, actor, static_cast<std::uint32_t>(tick))) break;
            }
        }
        if (s.checkBanana && s.alive) {
            bool ready = true;
            for (std::size_t j = 0; j < sessionCount; ++j)
                if (sessions[j].itemSnapshotPending ||
                    sessions[j].sendBuffer.size() + Wire::BananaHitSize + Wire::StateSize > 16384)
                    ready = false;
            if (ready) {
                s.checkBanana = false;
                BananaHitResult hit{};
                const auto x = static_cast<std::int32_t>(std::round(s.x));
                const auto z = static_cast<std::int32_t>(std::round(s.z));
                items.ResolveBananaHit(s.id, x, z, &hit);
                if (hit.trapId != 0) {
                    s.x = static_cast<float>(x);
                    s.z = static_cast<float>(z + (hit.pushed ?
                        GetItemRule(ItemKind::Banana).hitPushbackCells : 0));
                    s.awaitingBananaPosition = hit.pushed;
                    for (std::size_t j = 0; j < sessionCount; ++j)
                        QueueBananaHit(sessions[j], hit, s.id, x,
                            static_cast<std::int32_t>(s.z));
                }
            }
        }
        if (s.sendBuffer.size() + Wire::StateSize > 16384) {
            RemoveSession(i - 1); continue;
        }
        std::array<std::uint8_t, Wire::StateSize> packet{}; // 29바이트 = 헤더 12 + 본문 17
        Wire::Header(packet.data(), packet.size(), Wire::State, ++s.sendSeq, s.roundId); // 0~11: 서버 송신 순번과 경기 번호를 포함한 공통 헤더
        Wire::Write32(packet.data() + 12, s.inputSeq); // 12~15: 마지막으로 처리한 이동 요청의 순번
        Wire::Write32(packet.data() + 16, static_cast<std::uint32_t>(tick)); // 16~19: 서버 틱의 하위 32비트
        Wire::WriteFloat(packet.data() + 20, s.x); Wire::WriteFloat(packet.data() + 24, s.z); // 20~23: X, 24~27: Z
        packet[28] = static_cast<std::uint8_t>(s.status); // 28: 이동 처리 결과 (승인, 이동 간격 제한 등)
        s.sendBuffer.insert(s.sendBuffer.end(), packet.begin(), packet.end());
        if (tick % Wire::TickRate == 0) {
            std::cout << "[TICK " << tick << "] player=" << s.id << " round=" << s.roundId
                << " x=" << s.x << " z=" << s.z << " alive=" << s.alive << '\n';
        }
        if (!Flush(s)) RemoveSession(i - 1);
    }
    if (matchStateDirty) BroadcastMatchState();
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
                    s.roundId = 0;
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
    if (sessions[index].joined) {
        matchCoordinator.OnDisconnect(id); //슬롯 이동 전에 방 참가 상태 정리
        matchStateDirty = true; //이탈로 달라진 대기 순번 통지
    }
    closesocket(sessions[index].socket);
    items.RemovePlayer(id);
    --sessionCount;
    if (index != sessionCount) {
        sessions[index] = sessions[sessionCount];
    }
    sessions[sessionCount] = TransportSession{};
    if (matchCoordinator.GetRoomState().phase == ServicePhase::MATCHING) {
        for (std::size_t i = 0; i < sessionCount; ++i) {
            auto& member = sessions[i];
            member.roundId = 0;
            member.snapshotting = member.snapshotBeginPending = member.itemSnapshotPending = false;
            member.resetVote = false;
        }
    }
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
    lastBroadcastLobbyEpoch = 0;
    matchStateDirty = false;

    if (listenSocket != INVALID_SOCKET) {
        closesocket(listenSocket);
        listenSocket = INVALID_SOCKET;
    }

    if (winsockInitialized) {
        WSACleanup();
        winsockInitialized = false;
    }
}
