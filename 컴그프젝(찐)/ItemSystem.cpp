#include "ItemSystem.h"

#include <algorithm>

bool ItemSystem::AddGroundItem(const GroundItem& item) {
    if (item.id == 0 || item.kind == ItemKind::Empty ||
        item.kind > ItemKind::Shield) {
        return false;
    }
    return groundItems_.emplace(item.id, item).second;
}

bool ItemSystem::AddPlayer(PlayerId playerId) {
    if (playerId == 0) return false;
    const bool added = inventories_.emplace(playerId, PlayerInventory{}).second;
    if (added) {
        playerEffects_.emplace(playerId, PlayerEffectState{});
    }
    return added;
}

bool ItemSystem::RemovePlayer(PlayerId playerId) {
    const bool removed = inventories_.erase(playerId) != 0;
    playerEffects_.erase(playerId);
    return removed;
}

bool ItemSystem::SwitchSlot(PlayerId playerId, std::uint8_t slot) {
    auto it = inventories_.find(playerId);
    if (it == inventories_.end() || slot >= it->second.slots.size()) {
        return false;
    }

    it->second.selectedSlot = slot;
    return true;
}

bool ItemSystem::UseSelectedItem(PlayerId playerId, std::int32_t x, std::int32_t z,
                                 PlayerId targetId) {
    auto inventory = inventories_.find(playerId);
    if (inventory == inventories_.end()) return false;
    auto& kind = inventory->second.slots[inventory->second.selectedSlot];
    if (kind == ItemKind::Empty) return false;
    const ItemRule rule = GetItemRule(kind);
    if (rule.useMode == ItemUseMode::PlaceTrapAhead) {
        ObjectId id = 1;
        while (groundItems_.count(id) || bananaTraps_.count(id)) ++id;
        bananaTraps_.emplace(id, BananaTrap{id, playerId, x, z - rule.placementDistanceCells});
    } else if (rule.useMode == ItemUseMode::ActivateShield) {
        playerEffects_[playerId].shieldRemainingMs = rule.effectDurationMs;
    } else if (rule.useMode == ItemUseMode::TargetOpponent) {
        if (targetId == 0 || targetId == playerId || !GetInventory(targetId)) return false;
        auto& effects = playerEffects_[targetId];
        if (rule.effect == EffectKind::ReverseControls)
            effects.reverseControlsRemainingMs = rule.effectDurationMs;
        else if (rule.effect == EffectKind::Blackout)
            effects.blackoutRemainingMs = rule.effectDurationMs;
    } else return false;
    kind = ItemKind::Empty;
    return true;
}

bool ItemSystem::ResolveBananaHit(PlayerId playerId, std::int32_t x, std::int32_t z,
                                  BananaHitResult* result) {
    if (result) *result = {};
    if (!GetInventory(playerId)) return false;
    for (auto trap = bananaTraps_.begin(); trap != bananaTraps_.end(); ++trap) {
        if (trap->second.x == x && trap->second.z == z) {
            const ObjectId trapId = trap->first;
            bananaTraps_.erase(trap);
            const auto* effects = GetPlayerEffects(playerId);
            const bool pushed = !effects || effects->shieldRemainingMs == 0;
            if (result) *result = {trapId, pushed};
            return pushed;
        }
    }
    return false;
}

bool ItemSystem::RemoveBananaTrap(ObjectId trapId) {
    return bananaTraps_.erase(trapId) != 0;
}

bool ItemSystem::ApplyConfirmedBanana(const BananaTrap& trap) {
    if (trap.id == 0 || trap.ownerId == 0) return false;
    return bananaTraps_.emplace(trap.id, trap).second;
}

void ItemSystem::AdvanceEffects(std::uint32_t elapsedMs) {
    const auto advance = [elapsedMs](std::uint32_t& remaining) {
        remaining = remaining > elapsedMs ? remaining - elapsedMs : 0;
    };
    for (auto& entry : playerEffects_) {
        auto& effects = entry.second;
        advance(effects.reverseControlsRemainingMs);
        advance(effects.blackoutRemainingMs);
        advance(effects.shieldRemainingMs);
    }
}

std::vector<PickupResult> ItemSystem::ResolvePickups(
    const std::vector<PickupPlayer>& players) {
    std::vector<PickupPlayer> ordered = players;
    std::sort(ordered.begin(), ordered.end(),
        [](const PickupPlayer& a, const PickupPlayer& b) { return a.id < b.id; });

    std::vector<PickupResult> results;
    for (auto item = groundItems_.begin(); item != groundItems_.end();) {
        bool picked = false;
        for (const auto& player : ordered) {
            if (!player.alive || player.x != item->second.x ||
                player.z != item->second.z) continue;

            auto owner = inventories_.find(player.id);
            if (owner == inventories_.end()) continue;

            auto& slots = owner->second.slots;
            for (std::uint8_t slot = 0; slot < slots.size(); ++slot) {
                if (slots[slot] != ItemKind::Empty) continue;
                slots[slot] = item->second.kind;
                results.push_back({player.id, item->first, slot, item->second.kind});
                item = groundItems_.erase(item);
                picked = true;
                break;
            }
            if (picked) break;
        }
        if (!picked) ++item;
    }
    return results;
}

bool ItemSystem::TryPickup(ObjectId objectId, const PickupPlayer& player,
                           PickupResult& result) {
    const auto item = groundItems_.find(objectId);
    const auto owner = inventories_.find(player.id);
    if (item == groundItems_.end() || owner == inventories_.end() || !player.alive ||
        player.x != item->second.x || player.z != item->second.z) return false;
    auto& slots = owner->second.slots;
    for (std::uint8_t slot = 0; slot < slots.size(); ++slot) {
        if (slots[slot] != ItemKind::Empty) continue;
        slots[slot] = item->second.kind;
        result = {player.id, objectId, slot, item->second.kind};
        groundItems_.erase(item);
        return true;
    }
    return false;
}

bool ItemSystem::ApplyConfirmedPickup(PlayerId playerId, std::uint8_t slot,
                                      ItemKind kind) {
    const auto owner = inventories_.find(playerId);
    if (owner == inventories_.end() || slot >= owner->second.slots.size() ||
        kind == ItemKind::Empty || kind > ItemKind::Shield ||
        owner->second.slots[slot] != ItemKind::Empty) return false;
    owner->second.slots[slot] = kind;
    return true;
}

bool ItemSystem::ApplyInventoryState(PlayerId playerId, std::uint8_t selected,
                                      ItemKind first, ItemKind second) {
    auto found = inventories_.find(playerId);
    if (found == inventories_.end() || selected >= found->second.slots.size() ||
        first > ItemKind::Shield || second > ItemKind::Shield) return false;
    found->second.selectedSlot = selected;
    found->second.slots = {first, second};
    return true;
}

bool ItemSystem::ApplyConfirmedEffect(PlayerId playerId, EffectKind effect,
                                       std::uint32_t durationMs) {
    auto found = playerEffects_.find(playerId);
    if (found == playerEffects_.end() || durationMs == 0) return false;
    if (effect == EffectKind::ReverseControls)
        found->second.reverseControlsRemainingMs = durationMs;
    else if (effect == EffectKind::Blackout)
        found->second.blackoutRemainingMs = durationMs;
    else return false;
    return true;
}

bool ItemSystem::ApplyConfirmedShield(PlayerId playerId,
                                      std::uint32_t durationMs) {
    auto found = playerEffects_.find(playerId);
    if (found == playerEffects_.end() || durationMs == 0) return false;
    found->second.shieldRemainingMs = durationMs;
    return true;
}

PlayerId ItemSystem::SelectAttackTarget(PlayerId userId,
    const std::vector<TargetPlayer>& players, std::mt19937& rng) const {
    if (inventories_.find(userId) == inventories_.end()) return 0;

    const auto user = std::find_if(players.begin(), players.end(),
        [userId](const TargetPlayer& player) { return player.id == userId; });
    if (user == players.end() || !user->alive) return 0;

    std::vector<PlayerId> candidates;
    for (const auto& player : players) {
        if (player.id == 0 || player.id == userId || !player.alive ||
            inventories_.find(player.id) == inventories_.end()) continue;
        candidates.push_back(player.id);
    }
    if (candidates.empty()) return 0;
    std::uniform_int_distribution<std::size_t> pick(0, candidates.size() - 1);
    return candidates[pick(rng)];
}

const PlayerInventory* ItemSystem::GetInventory(PlayerId playerId) const {
    auto it = inventories_.find(playerId);
    return it == inventories_.end() ? nullptr : &it->second;
}

const PlayerEffectState* ItemSystem::GetPlayerEffects(PlayerId playerId) const {
    auto it = playerEffects_.find(playerId);
    return it == playerEffects_.end() ? nullptr : &it->second;
}

const std::map<ObjectId, GroundItem>& ItemSystem::GetGroundItems() const {
    return groundItems_;
}

const std::map<ObjectId, BananaTrap>& ItemSystem::GetBananaTraps() const {
    return bananaTraps_;
}

void ItemSystem::ResetRound() {
    groundItems_.clear();
    inventories_.clear();
    playerEffects_.clear();
    bananaTraps_.clear();
}
