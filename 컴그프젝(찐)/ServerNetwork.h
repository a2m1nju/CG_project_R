#pragma once

#include <winsock2.h>
#include "Protocol.h"
#include "Room.h"
#include "WorldState.h"
#include "ItemSystem.h"

#include <array>
#include <atomic>
#include <vector>
#include <string>

class ServerNetwork {
public:
    ServerNetwork() = default;
    ~ServerNetwork() { Shutdown(); }

    ServerNetwork(const ServerNetwork&) = delete;
    ServerNetwork& operator=(const ServerNetwork&) = delete;

    bool Initialize(std::uint16_t port);
    void RunAcceptLoop();
    void StopAccepting();
    void Shutdown();

    void AttachLobby(RoomState& state)
    {
        lobbyRoom = &state;
    }

private:
    struct TransportSession {
        std::uint32_t id = 0;
        SOCKET socket = INVALID_SOCKET;

        std::vector<std::uint8_t> receiveBuffer, sendBuffer;

        std::uint32_t sequence = 0;
        std::uint32_t inputSeq = 0;
        std::uint32_t roundId = 0;
        std::uint32_t sendSeq = 0;

        std::uint64_t nextMoveTick = 0;
        float x = 0, z = 0, rotation = 0;

        bool resetVote = false;
        bool itemSnapshotPending = false;
        bool checkBanana = false;
        bool awaitingBananaPosition = false;

        bool snapshotting = false;
        bool snapshotBeginPending = false;

        std::int32_t snapshotMinZ = 0;
        std::int32_t snapshotMaxZ = 0;
        std::int32_t snapshotNextZ = 0;
        std::int32_t announcedMinZ = 0;
        std::int32_t announcedMaxZ = 0;

        bool alive = true;
        Wire::MoveStatus status = Wire::MoveStatus::Idle;

        std::string nickname;
        bool drop = false;
    };

    std::array<TransportSession, 8> sessions{};
    std::size_t sessionCount = 0;
    std::uint32_t nextSessionId = 1;

    std::atomic<bool> stopping{ false };
    SOCKET listenSocket = INVALID_SOCKET;
    bool winsockInitialized = false;

    RoomState* lobbyRoom = nullptr;

    WorldState world;
    ItemSystem items;
    ObjectId nextSpawnItemId = 1;

    std::uint64_t currentTick = 0;
    std::uint64_t loadingStartTick = 0;

    void RemoveSession(std::size_t index);
    bool ProcessPackets(TransportSession& session, std::uint64_t tick);
    bool Flush(TransportSession& session);
    void Tick(std::uint64_t tick);

    TransportSession* FindSession(SessionId id);
    bool IsLobbyMember(SessionId id) const;
    bool IsRoundMember(SessionId id) const;

    void SendLobby(TransportSession& session, std::vector<std::uint8_t> packet);

    void SendLobbyError(
        TransportSession& session,
        std::uint16_t code,
        std::uint8_t rejectedType,
        std::uint32_t rejectedSequence);

    void PublishLobby();
    void CloseLobby(SessionId leaverId);

    int HandleLobbyPacket(
        TransportSession& session,
        const std::uint8_t* packet,
        std::size_t size);

    void SendGame(TransportSession& session, std::vector<std::uint8_t> packet);

    void BeginLoading();
    void StartRoundSnapshots();
    void TryBeginNextRound();
    void SendStart(std::uint32_t tick);
    void PublishPlayers(std::uint32_t tick);

    bool QueueWorldBegin(TransportSession& session);
    bool QueueWorldLane(TransportSession& session, const WorldLaneData& lane);
    bool QueueWorldActor(TransportSession& session, const WorldActorData& actor, std::uint32_t tick);
    bool QueueWorldEnd(TransportSession& session);

    void StartWorldSnapshot(TransportSession& session);
    void PumpWorldPackets(TransportSession& session, std::uint32_t tick);

    void ResetWorld(std::uint32_t roundId);

    bool HandleItemRequest(
        TransportSession& session,
        const std::uint8_t* packet,
        std::size_t size);

    bool QueueItemSnapshot(TransportSession& session);
    bool QueueItemRemoved(TransportSession& session, const PickupResult& result);
    bool QueueInventoryState(TransportSession& session, bool accepted);
    bool QueueItemEffect(TransportSession& session, ItemKind kind, std::uint32_t durationMs);
    bool QueueBananaPlaced(TransportSession& session, const BananaTrap& trap);
    bool QueueBananaHit(TransportSession& session, const BananaHitResult& hit,
        PlayerId victimId,
        std::int32_t x,
        std::int32_t z);
    bool QueueItemSpawned(TransportSession& session, const GroundItem& item);
};