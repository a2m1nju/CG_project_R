#pragma once

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
    MATCHING,     // 참가자 모집 중
    READY_LOBBY,  // 3명이 모여 준비 중
    LOADING,      // 게임 데이터 로딩 중
    PLAYING,      // 경기 진행 중
    RESULT        // 결과 확인 및 재경기 선택 중
};
struct PlayerState {
    PlayerId playerId = 0;
    bool alive = false;
    bool departed = false;
};
typedef struct RoomState {
    ServicePhase phase = ServicePhase::MATCHING;

    std::vector<SessionId> queue;
    std::array<SessionId, 3> members{};
    std::array<PlayerState, 3> players{};

    std::array<bool, 3> ready{};
    std::array<bool, 3> loaded{};

    LobbyEpoch lobbyEpoch = 0;
    RoundId roundId = 0;
} RoomState;

typedef struct JoinResult {
    ErrorCode code; // 0:정상, 1:방상태 오류, 2:중복등록 3:인원초과 
    PlayerId playerId;
    uint16_t queuePosition;
};

class MatchCoordinator {
public:
    explicit MatchCoordinator(RoomState& room)
        : room(room) {};

    JoinResult Enqueue(SessionId id);
    void TryPromote();
    bool SetReady(SessionId id, LobbyEpoch epoch, bool ready);

    void DissolveReadyLobby(SessionId leaverId);
    void CancelLoading(SessionId leaverId);

private:
    RoomState& room;
};

class RoundController {
public:
    explicit RoundController(RoomState& room)
        : room(room) {}
    void BeginRound(bool rematch);
    bool MarkLoaded(PlayerId id, RoundId roundId);
    bool ShouldFinish() const;

private:
    RoomState& room;
};
