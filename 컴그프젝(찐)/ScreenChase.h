#pragma once

#include "Protocol.h"

#include <cstdint>

namespace ScreenChase {
    constexpr std::uint32_t StartDelayTicks = 15 * Wire::TickRate;
    constexpr float LanesPerSecond = 1.0f;
    // The camera looks down from behind the player; its ground-level rear edge
    // is about six lanes behind the point it follows.
    constexpr float RearEdgeDistance = 6.0f;
    // Start the catch animation while the player is still visible.
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
