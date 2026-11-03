#pragma once

#include <vector>
#include <array>

using SessionId = std::uint32_t;
using PlayerId = std::uint32_t;
using LobbyEpoch = std::uint32_t;
using RoundId = std::uint32_t;

enum class ServicePhase {
    MATCHING,     // 참가자 모집 중
    READY_LOBBY,  // 3명이 모여 준비 중
    LOADING,      // 게임 데이터 로딩 중
    PLAYING,      // 경기 진행 중
    RESULT        // 결과 확인 및 재경기 선택 중
};