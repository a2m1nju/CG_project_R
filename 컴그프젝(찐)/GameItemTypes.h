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

enum class EffectKind : std::uint8_t {
    None,
    ReverseControls,
    Blackout,
    Stun
};

enum class ItemUseMode : std::uint8_t {
    None,
    TargetOpponent, // 살아 있는 다른 플레이어를 지정
    PlaceTrap       // 사용자의 현재 격자에 설치
};

struct ItemRule {
    ItemUseMode useMode;
    EffectKind effect;
    std::uint32_t effectDurationMs;
};

// 같은 효과를 다시 받으면 남은 시간을 다시 갱신한다.
// 서로 다른 효과는 동시에 유지하며, 라운드 종료 시 모두 초기화한다.
// 유효한 사용에만 인벤토리 슬롯을 비운다.
constexpr ItemRule GetItemRule(ItemKind kind) {
    switch (kind) {
    case ItemKind::ReverseControls:
        return {ItemUseMode::TargetOpponent, EffectKind::ReverseControls, 5000};
    case ItemKind::Blackout:
        return {ItemUseMode::TargetOpponent, EffectKind::Blackout, 4000};
    case ItemKind::Banana:
        // 설치자가 아닌 다른 플레이어가 밟을 때까지 유지한다.
        // 밟히면 바나나를 제거하고 1.2초 동안 이동을 막는다.
        return {ItemUseMode::PlaceTrap, EffectKind::Stun, 1200};
    default:
        return {ItemUseMode::None, EffectKind::None, 0};
    }
}

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
