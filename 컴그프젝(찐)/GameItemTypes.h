#pragma once
#include <array>
#include <cstdint>

using ObjectId = std::uint32_t;
using PlayerId = std::uint32_t;

enum class ItemKind : std::uint8_t {
    Empty = 0,
    ReverseControls = 1,
    Blackout = 2,
    Banana = 3,
    Shield = 4
};

enum class EffectKind : std::uint8_t {
    None,
    ReverseControls,
    Blackout
};

enum class ItemUseMode : std::uint8_t {
    None,
    TargetOpponent, // 서버가 살아 있는 상대를 선정
    PlaceTrapAhead, // 시선 방향과 관계없이 전방(-Z)에 설치
    ActivateShield
};

struct ItemRule {
    ItemUseMode useMode;
    EffectKind effect;
    std::uint32_t effectDurationMs;
    std::uint8_t placementDistanceCells; // 설치형 아이템의 전방 격자 거리
    std::uint8_t hitPushbackCells;       // 밟았을 때 뒤쪽(+Z) 이동 거리
};

// 같은 효과를 다시 받으면 남은 시간을 다시 갱신한다.
// 서로 다른 효과는 동시에 유지하며, 라운드 종료 시 모두 초기화한다.
// 유효한 사용에만 인벤토리 슬롯을 비운다.
constexpr ItemRule GetItemRule(ItemKind kind) {
    switch (kind) {
    case ItemKind::ReverseControls:
        return {ItemUseMode::TargetOpponent, EffectKind::ReverseControls, 5000, 0, 0};
    case ItemKind::Blackout:
        return {ItemUseMode::TargetOpponent, EffectKind::Blackout, 1000, 0, 0};
    case ItemKind::Banana:
        // 현재 격자에서 시선 방향과 관계없이 z-3에 설치한다.
        // 설치자를 포함한 생존자 한 명이 밟을 때까지 유지한다.
        // 밟히면 뒤쪽(+Z)으로 1칸 밀고 바나나를 제거한다.
        return {ItemUseMode::PlaceTrapAhead, EffectKind::None, 0, 3, 1};
    case ItemKind::Shield:
        return {ItemUseMode::ActivateShield, EffectKind::None, 10000, 0, 0};
    default:
        return {ItemUseMode::None, EffectKind::None, 0, 0, 0};
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

// 남은 시간이 0이면 해당 효과가 비활성 상태다.
struct PlayerEffectState {
    std::uint32_t reverseControlsRemainingMs = 0;
    std::uint32_t blackoutRemainingMs = 0;
    std::uint32_t shieldRemainingMs = 0;
};

// 바나나는 시간 제한 없이 유지된다.
struct BananaTrap {
    ObjectId id;
    PlayerId ownerId;
    std::int32_t x;
    std::int32_t z;
};
