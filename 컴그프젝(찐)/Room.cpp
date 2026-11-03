#include "Room.h"

struct RoomState { // 3명이 모이면 준비 대기실로 전환
    ServicePhase phase = ServicePhase::MATCHING;

    std::vector<SessionId> queue;
    std::array<SessionId, 3> members{};

    std::array<bool, 3> ready{};
    std::array<bool, 3> loaded{};

    LobbyEpoch lobbyEpoch = 0;
    RoundId roundId = 0;
};
