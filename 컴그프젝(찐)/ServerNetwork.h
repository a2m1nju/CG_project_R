#pragma once
#include <winsock2.h>
#include "Protocol.h"
#include "ItemSystem.h"
#include "WorldState.h"
#include <array>
#include <atomic>
#include <vector>
#include <string>

class MatchCoordinator;
class RoundController;

class ServerNetwork {
public:
    explicit ServerNetwork(MatchCoordinator& matchCoordinator, RoundController& roundController)
        : matchCoordinator(matchCoordinator), roundController(roundController) {}
    ~ServerNetwork() { Shutdown(); }
    ServerNetwork(const ServerNetwork&) = delete;
    ServerNetwork& operator=(const ServerNetwork&) = delete;
    bool Initialize(std::uint16_t port);
    void RunAcceptLoop();
    void StopAccepting();
    void Shutdown();
private:
    // 서버 내부에서 TCP 연결 하나의 소켓과 송수신 상태를 관리한다.
    // struct 멤버는 기본 public이고, 타입 자체는 ServerNetwork 내부에 둔다.
    struct TransportSession {
        std::uint32_t id = 0;
        std::uint32_t playerId = 0;
        bool joined = false; //TCP 접속과 게임 참가 승인 구분
        std::string nickname;
        SOCKET socket = INVALID_SOCKET;
        std::vector<std::uint8_t> receiveBuffer, sendBuffer;
        std::uint32_t sequence = 0, inputSeq = 0, roundId = 1, sendSeq = 0;
        std::uint64_t nextMoveTick = 0;
        float x = 0, z = 0;
        bool alive = true;
        bool resetVote = false;
        bool snapshotting = false;
        bool snapshotBeginPending = false;
        bool itemSnapshotPending = false;
        bool checkBanana = false, awaitingBananaPosition = false;
        std::int32_t snapshotMinZ = 0, snapshotMaxZ = 0, snapshotNextZ = 0;
        std::int32_t announcedMinZ = 0, announcedMaxZ = 0;
        Wire::MoveStatus status = Wire::MoveStatus::Idle;
    };
    void RemoveSession(std::size_t index);
    void BroadcastMatchState();
    bool matchStateDirty = false;
    std::uint32_t lastBroadcastLobbyEpoch = 0;
    bool QueueRequestError(TransportSession& session, std::uint8_t type,
                           std::uint32_t sequence, std::uint16_t error);
    bool HandleReady(TransportSession& session, const std::uint8_t* packet,
                     std::size_t size, std::uint32_t sequence);
    bool HandleJoin(TransportSession& session, const std::uint8_t* packet,
                    std::size_t size, std::uint32_t sequence);
    bool ProcessPackets(TransportSession& session, std::uint64_t tick);
    bool Flush(TransportSession& session);
    bool QueueWorldBegin(TransportSession& session);
    bool QueueWorldLane(TransportSession& session, const WorldLaneData& lane);
    bool QueueWorldActor(TransportSession& session, const WorldActorData& actor,
                         std::uint32_t tick);
    bool QueueWorldEnd(TransportSession& session);
    void StartWorldSnapshot(TransportSession& session);
    void PumpWorldPackets(TransportSession& session, std::uint32_t tick);
    bool QueueItemSnapshot(TransportSession& session);
    bool QueueItemRemoved(TransportSession& session, const PickupResult& result);
    bool QueueInventoryState(TransportSession& session, bool accepted);
    bool QueueItemEffect(TransportSession& session, ItemKind kind,
                         std::uint32_t durationMs);
    bool QueueBananaPlaced(TransportSession& session, const BananaTrap& trap);
    bool QueueBananaHit(TransportSession& session, const BananaHitResult& hit,
                        PlayerId victimId, std::int32_t x, std::int32_t z);
    bool QueueItemSpawned(TransportSession& session, const GroundItem& item);
    void StartRoundSnapshots();
    void HandleLoaded(TransportSession& session, const std::uint8_t* packet, std::size_t size);
    void ResetWorld(std::uint32_t roundId);
    void TryBeginNextRound();
    void Tick(std::uint64_t tick);
    MatchCoordinator& matchCoordinator;
    RoundController& roundController;
    WorldState world;
    ItemSystem items;
    std::array<TransportSession, 8> sessions{};
    std::size_t sessionCount = 0;
    std::uint32_t nextSessionId = 1;
    ObjectId nextSpawnItemId = 1;
    std::atomic<bool> stopping{false};
    SOCKET listenSocket = INVALID_SOCKET;
    bool winsockInitialized = false;
};
