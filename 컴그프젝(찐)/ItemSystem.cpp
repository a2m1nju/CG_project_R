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

bool ItemSystem::SwitchSlot(PlayerId playerId, std::uint8_t slot) {
    auto it = inventories_.find(playerId);
    if (it == inventories_.end() || slot >= it->second.slots.size()) {
        return false;
    }

    it->second.selectedSlot = slot;
    return true;
}

bool ItemSystem::UseSelectedItem(PlayerId playerId, std::int32_t x, std::int32_t z) {
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
        // Local preview until multiplayer integration is available.
        auto& effects = playerEffects_[playerId];
        if (rule.effect == EffectKind::ReverseControls)
            effects.reverseControlsRemainingMs = rule.effectDurationMs;
        else if (rule.effect == EffectKind::Blackout)
            effects.blackoutRemainingMs = rule.effectDurationMs;
    } else return false;
    kind = ItemKind::Empty;
    return true;
}

bool ItemSystem::ResolveBananaHit(PlayerId playerId, std::int32_t x, std::int32_t z) {
    if (!GetInventory(playerId)) return false;
    for (auto trap = bananaTraps_.begin(); trap != bananaTraps_.end(); ++trap) {
        if (trap->second.x == x && trap->second.z == z) {
            bananaTraps_.erase(trap);
            const auto* effects = GetPlayerEffects(playerId);
            return !effects || effects->shieldRemainingMs == 0;
        }
    }
    return false;
}

void ItemSystem::AdvanceEffects(std::uint32_t elapsedMs) {
    for (auto& entry : playerEffects_) {
        auto& effects = entry.second;
        effects.reverseControlsRemainingMs = effects.reverseControlsRemainingMs > elapsedMs ? effects.reverseControlsRemainingMs - elapsedMs : 0;
        effects.blackoutRemainingMs = effects.blackoutRemainingMs > elapsedMs ? effects.blackoutRemainingMs - elapsedMs : 0;
        effects.shieldRemainingMs = effects.shieldRemainingMs > elapsedMs ? effects.shieldRemainingMs - elapsedMs : 0;
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

PlayerId ItemSystem::SelectAttackTarget(PlayerId userId,
    const std::vector<TargetPlayer>& players) const {
    if (inventories_.find(userId) == inventories_.end()) return 0;

    const auto user = std::find_if(players.begin(), players.end(),
        [userId](const TargetPlayer& player) { return player.id == userId; });
    if (user == players.end() || !user->alive) return 0;

    PlayerId bestId = 0;
    int bestGroup = 3;
    std::int64_t bestDistance = 0;
    for (const auto& player : players) {
        if (player.id == 0 || player.id == userId || !player.alive ||
            inventories_.find(player.id) == inventories_.end()) continue;

        const auto delta = static_cast<std::int64_t>(player.zMilli) - user->zMilli;
        const int group = delta < 0 ? 0 : (delta == 0 ? 1 : 2);
        const auto distance = delta < 0 ? -delta : delta;
        if (bestId == 0 || group < bestGroup ||
            (group == bestGroup && (distance < bestDistance ||
                (distance == bestDistance && player.id < bestId)))) {
            bestId = player.id;
            bestGroup = group;
            bestDistance = distance;
        }
    }
    return bestId;
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
