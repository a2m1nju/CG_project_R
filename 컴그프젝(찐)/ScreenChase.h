#pragma once

#include "Protocol.h"

#include <cstdint>

namespace ScreenChase {
    constexpr std::uint32_t StartDelayTicks = 15 * Wire::TickRate;
    constexpr float LanesPerSecond = 1.0f;
    // 카메라가 플레이어 뒤에서 내려다볼 때 화면의 뒤쪽 끝은
    // 카메라가 따라가는 지점에서 약 여섯 칸 뒤에 있다.
    constexpr float RearEdgeDistance = 6.0f;
    // 플레이어가 화면에 보일 때 붙잡히는 연출을 시작한다.
    constexpr float CatchDistance = RearEdgeDistance - 1.5f;

    inline bool Active(std::uint64_t elapsedTicks) {
        return elapsedTicks >= StartDelayTicks;
    }

    inline float FrontZ(std::uint64_t elapsedTicks) {
        if (!Active(elapsedTicks)) return 0.0f;
        return -static_cast<float>(elapsedTicks - StartDelayTicks) *
            LanesPerSecond / static_cast<float>(Wire::TickRate);
    }

    inline bool Overtaken(float playerZ, std::uint64_t elapsedTicks) {
        return Active(elapsedTicks) &&
            playerZ > FrontZ(elapsedTicks) + CatchDistance;
    }
}
