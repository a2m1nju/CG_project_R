#pragma once

#include "WorldState.h"
#include "ResultTypes.h"

#include <vector>
#include <array>
#include <cstdint>
#include <algorithm>
#include <unordered_set>
#include <deque>

using SessionId = std::uint32_t;
using PlayerId = std::uint32_t;
using LobbyEpoch = std::uint32_t;
using RoundId = std::uint32_t;
using ErrorCode = std::uint16_t;

enum class ServicePhase {
    MATCHING,
    READY_LOBBY,
    LOADING,
    PLAYING,
    RESULT
};

enum class RematchVote {
    UNSET,
    AGREE,
    DISAGREE
};

enum class RematchDecision {
    WAIT,
    RESTART,
    CLOSE
};

struct RoundRecord {

    PlayerId playerId = 0;
    std::uint8_t slot = 0;
    float startZ = 0;
    std::uint32_t maxForwardLanes = 0;
    std::uint32_t survivalTicks = 0;
    std::uint32_t itemsUsed = 0;

    bool departed = false;
    bool frozen = false;

    std::uint64_t deathTick = 0;
};

struct RankConfig {
    bool jointTies = true;
    bool departedLast = true;
};

struct RoomState {

    ServicePhase phase = ServicePhase::MATCHING;
    std::vector<SessionId> queue;
    std::array<SessionId, 3> members{};

    std::array<bool, 3> ready{};
    std::array<bool, 3> loaded{};

    std::array<RematchVote, 3> rematchVotes{};

    std::array<RoundRecord, 3> records{};

    std::uint64_t roundStartTick = 0;
    std::uint64_t recordTick = 0;

    bool recordClockStarted = false;

    bool resultReady = false;
    ResultData result{};

    LobbyEpoch lobbyEpoch = 0;
    RoundId roundId = 0;
};

struct JoinResult {
    ErrorCode code;
    PlayerId playerId;
    std::uint16_t queuePosition;
};

enum class ReadyResult {
    Accepted,
    BadPhase,
    Stale,
    NotMember
};

class MatchCoordinator {
public:
    explicit MatchCoordinator(
        RoomState& room,
        WorldState& world
    )
        : room(room),
        world(world)
    {
    }

    JoinResult Enqueue(SessionId id);

    void TryPromote();

    const RoomState& GetRoomState() const
    {
        return room;
    }

    ReadyResult SetReady(
        SessionId id,
        LobbyEpoch epoch,
        bool ready
    );

    void DissolveReadyLobby(SessionId leaverId);
    void CancelLoading(SessionId leaverId);
    void OnDisconnect(SessionId id);

private:
    RoomState& room;
    WorldState& world;
};

class RoundController {
public:
    explicit RoundController(
        RoomState& room,
        WorldState& world
    )
        : room(room),
        world(world)
    {
    }

    bool BeginRound(bool rematch);

    bool MarkLoaded(
        PlayerId id,
        RoundId roundId
    );

    bool ShouldFinish() const;

    void StartRecordClock(std::uint64_t tick);

    void RecordPosition(
        PlayerId id,
        float z
    );

    void RecordDeath(
        PlayerId id,
        std::uint64_t tick,
        bool departed = false
    );

    void RecordItemUsed(PlayerId id);

    void UpdateSurvival(std::uint64_t tick);

    ResultData BuildResult(
        const RankConfig& config
    );

private:
    void PrepareRound();

    RoomState& room;
    WorldState& world;
};

class RematchController {
public:
    explicit RematchController(
        RoomState& room,
        WorldState& world
    )
        : room(room),
        world(world)
    {
    }

    void SetVote(
        PlayerId playerId,
        bool agree
    );

    RematchDecision Evaluate() const;

private:
    RoomState& room;
    WorldState& world;
};