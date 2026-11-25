#pragma once

#include <array>
#include <cstdint>

struct ResultEntry {
    std::uint32_t playerId = 0;
    std::uint32_t distanceLanes = 0;
    std::uint32_t survivalTicks = 0;
    std::uint32_t itemsUsed = 0;

    // 통신에서는 0, 1, 2.
    // 화면에 표시할 때는 slot + 1.
    std::uint8_t slot = 0;

    std::uint8_t rank = 0;
    bool departed = false;
};

struct ResultData {
    std::uint32_t roundId = 0;

    std::array<ResultEntry, 3> entries{};

    bool rematchAllowed = false;
    bool roomClosing = false;
};