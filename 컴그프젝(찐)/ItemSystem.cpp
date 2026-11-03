#include "ItemSystem.h"

#include <algorithm>

bool ItemSystem::AddGroundItem(const GroundItem& item) {
    if (item.id == 0 || item.kind == ItemKind::Empty ||
        item.kind > ItemKind::Banana) {
        return false;
    }
    return groundItems_.emplace(item.id, item).second;
}

bool ItemSystem::AddPlayer(PlayerId playerId) {
    if (playerId == 0) return false;
    return inventories_.emplace(playerId, PlayerInventory{}).second;
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

const PlayerInventory* ItemSystem::GetInventory(PlayerId playerId) const {
    auto it = inventories_.find(playerId);
    return it == inventories_.end() ? nullptr : &it->second;
}

const std::map<ObjectId, GroundItem>& ItemSystem::GetGroundItems() const {
    return groundItems_;
}

void ItemSystem::ResetRound() {
    groundItems_.clear();
    inventories_.clear();
}
