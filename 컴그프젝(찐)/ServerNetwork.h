#pragma once
#include <winsock2.h>
#include "Protocol.h"
#include "WorldState.h"
#include <array>
#include <atomic>
#include <vector>

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
private:
    struct TransportSession {
        std::uint32_t id = 0;
        SOCKET socket = INVALID_SOCKET;
        std::vector<std::uint8_t> receiveBuffer, sendBuffer;
        std::uint32_t sequence = 0, inputSeq = 0, roundId = 1, sendSeq = 0;
        std::uint64_t nextMoveTick = 0;
        float x = 0, z = 0;
        bool alive = true;
        bool resetVote = false;
        bool snapshotting = false;
        bool snapshotBeginPending = false;
        std::int32_t snapshotMinZ = 0, snapshotMaxZ = 0, snapshotNextZ = 0;
        std::int32_t announcedMinZ = 0, announcedMaxZ = 0;
        Wire::MoveStatus status = Wire::MoveStatus::Idle;
    };
    void RemoveSession(std::size_t index);
    bool ProcessPackets(TransportSession& session, std::uint64_t tick);
    bool Flush(TransportSession& session);
    bool QueueWorldBegin(TransportSession& session);
    bool QueueWorldLane(TransportSession& session, const WorldLaneData& lane);
    bool QueueWorldActor(TransportSession& session, const WorldActorData& actor,
                         std::uint32_t tick);
    bool QueueWorldEnd(TransportSession& session);
    void StartWorldSnapshot(TransportSession& session);
    void PumpWorldPackets(TransportSession& session, std::uint32_t tick);
    void TryBeginNextRound();
    void Tick(std::uint64_t tick);
    WorldState world;
    std::array<TransportSession, 8> sessions{};
    std::size_t sessionCount = 0;
    std::uint32_t nextSessionId = 1;
    std::atomic<bool> stopping{false};
    SOCKET listenSocket = INVALID_SOCKET;
    bool winsockInitialized = false;
};
