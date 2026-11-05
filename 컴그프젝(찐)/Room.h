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
enum class RematchVote {
    UNSET,     // 아직 응답하지 않음
    AGREE,     // 재경기 동의
    DISAGREE   // 재경기 거절
};
enum class RematchDecision {
    WAIT,      // 응답 대기
    RESTART,   // 재경기
    CLOSE      // 방 종료
};
struct PlayerState {
    PlayerId playerId = 0;
    bool alive = false;
    bool departed = false;
};
struct RoundRecord {
    PlayerId playerId = 0;
    std::uint32_t maxForwardLanes = 0;
    std::uint32_t survivalTicks = 0;
    bool departed = false;
    std::uint32_t deathTick = 0;
};
struct ResultEntry {
    PlayerId playerId = 0;
    std::uint32_t distanceLanes = 0;
    std::uint32_t survivalTicks = 0;
    std::uint8_t rank = 0; // 미산정
    bool departed = false;
};
struct ResultData {
    RoundId roundId = 0;
    std::array<ResultEntry, 3> entries{};
    bool rematchAllowed = false;
    bool roomClosing = false;
};
struct RankConfig; // 구체적인 순위 설정은 추후 정의
typedef struct RoomState {
    ServicePhase phase = ServicePhase::MATCHING;

    std::vector<SessionId> queue;
    std::array<SessionId, 3> members{};
    std::array<PlayerState, 3> players{};

    std::array<bool, 3> ready{};
    std::array<bool, 3> loaded{};
    std::array<RematchVote, 3> rematchVotes{};

    std::array<RoundRecord, 3> records{};
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
    ResultData BuildResult(const RankConfig& config);

private:
    RoomState& room;
};

class RematchController {
public:
    explicit RematchController(RoomState& room)
        : room(room) {
    }

    void SetVote(PlayerId p, bool agree);
    RematchDecision Evaluate() const;

private:
    RoomState& room;
};
