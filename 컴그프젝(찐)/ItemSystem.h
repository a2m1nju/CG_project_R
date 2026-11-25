#pragma once

#include "GameItemTypes.h"

#include <cstdint>
#include <map>
#include <random>
#include <vector>

struct PickupPlayer {
    PlayerId id;
    std::int32_t x;
    std::int32_t z;
    bool alive;
};

struct PickupResult {
    PlayerId playerId;
    ObjectId objectId;
    std::uint8_t slot;
    ItemKind kind;
};

struct BananaHitResult {
    ObjectId trapId = 0;
    bool pushed = false;
};

struct TargetPlayer {
    PlayerId id;
    bool alive;
};

class ItemSystem {
public:
    bool AddGroundItem(const GroundItem& item);
    bool AddPlayer(PlayerId playerId);
    bool RemovePlayer(PlayerId playerId);
    bool SwitchSlot(PlayerId playerId, std::uint8_t slot);
    bool UseSelectedItem(PlayerId playerId, std::int32_t x, std::int32_t z,
                         PlayerId targetId = 0);
    bool ResolveBananaHit(PlayerId playerId, std::int32_t x, std::int32_t z,
                          BananaHitResult* result = nullptr);
    bool RemoveBananaTrap(ObjectId trapId);
    void AdvanceEffects(std::uint32_t elapsedMs);
    void ClearPlayerEffects(PlayerId playerId);
    std::vector<PickupResult> ResolvePickups(const std::vector<PickupPlayer>& players);
    bool TryPickup(ObjectId objectId, const PickupPlayer& player, PickupResult& result);
    bool ApplyConfirmedPickup(PlayerId playerId, std::uint8_t slot, ItemKind kind);
    bool ApplyInventoryState(PlayerId playerId, std::uint8_t selected,
                             ItemKind first, ItemKind second);
    bool ApplyConfirmedEffect(PlayerId playerId, EffectKind effect,
                              std::uint32_t durationMs);
    bool ApplyConfirmedShield(PlayerId playerId, std::uint32_t durationMs);
    bool ApplyConfirmedBanana(const BananaTrap& trap);
    PlayerId SelectAttackTarget(PlayerId userId,
        const std::vector<TargetPlayer>& players, std::mt19937& rng) const;
    const PlayerInventory* GetInventory(PlayerId playerId) const;
    const PlayerEffectState* GetPlayerEffects(PlayerId playerId) const;
    const std::map<ObjectId, GroundItem>& GetGroundItems() const;
    const std::map<ObjectId, BananaTrap>& GetBananaTraps() const;
    void ResetRound();

private:
    std::map<ObjectId, GroundItem> groundItems_;
    std::map<PlayerId, PlayerInventory> inventories_;
    std::map<PlayerId, PlayerEffectState> playerEffects_;
    std::map<ObjectId, BananaTrap> bananaTraps_;
};
