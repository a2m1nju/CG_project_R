#pragma once
#include <winsock2.h>
#include "Protocol.h"
#include <array>
#include <atomic>
#include <vector>
#include "Room.h"
#include <string>
#include "WorldState.h"

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
        std::uint32_t sequence = 0, inputSeq = 0, roundId = 1, sendSeq = 0;
        std::uint64_t nextMoveTick = 0;
        float x = 0, z = 0;
        float rotation = 0;
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
    void RemoveSession(std::size_t index);
    bool ProcessPackets(TransportSession& session, std::uint64_t tick);
    bool Flush(TransportSession& session);
    void Tick(std::uint64_t tick);
    std::array<TransportSession, 8> sessions{};
    std::size_t sessionCount = 0;
    std::uint32_t nextSessionId = 1;
    std::atomic<bool> stopping{false};
    SOCKET listenSocket = INVALID_SOCKET;
    bool winsockInitialized = false;
    RoomState* lobbyRoom = nullptr;

    TransportSession* FindSession(SessionId id);

    bool IsLobbyMember(SessionId id) const;

    void SendLobby(TransportSession& session, std::vector<std::uint8_t> packet);

    void SendLobbyError(TransportSession& session, std::uint16_t code, std::uint8_t rejectedType, std::uint32_t rejectedSequence);

    void PublishLobby();

    void CloseLobby(SessionId leaverId);

    int HandleLobbyPacket(TransportSession& session, const std::uint8_t* packet, std::size_t size);
    bool IsRoundMember(SessionId id) const;

    void SendGame(
        TransportSession& session,
        std::vector<std::uint8_t> packet);

    void BeginLoading();
    void SendStart(std::uint32_t tick);
    void PublishPlayers(std::uint32_t tick);

    bool QueueWorldBegin(TransportSession& session);

    bool QueueWorldLane(
        TransportSession& session,
        const WorldLaneData& lane);

    bool QueueWorldActor(
        TransportSession& session,
        const WorldActorData& actor,
        std::uint32_t tick);

    bool QueueWorldEnd(TransportSession& session);

    void StartWorldSnapshot(TransportSession& session);

    void PumpWorldPackets(
        TransportSession& session,
        std::uint32_t tick);

    WorldState world;

    std::uint64_t currentTick = 0;
    std::uint64_t loadingStartTick = 0;
};
