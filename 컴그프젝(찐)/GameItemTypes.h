#pragma once
#include <array>
#include <cstdint>

using ObjectId = std::uint32_t;
using PlayerId = std::uint32_t;

enum class ItemKind : std::uint8_t {
    Empty = 0,
    ReverseControls = 1,
    Blackout = 2,
    Banana = 3
};

struct GroundItem {
    ObjectId id;       // 한 라운드 안에서 중복되지 않는 ID
    ItemKind kind;
    std::int32_t x;    // 월드 격자 좌표
    std::int32_t z;
};

struct PlayerInventory {
    std::array<ItemKind, 2> slots{
        ItemKind::Empty, ItemKind::Empty
    };
    std::uint8_t selectedSlot = 0; // 0 또는 1
};
