#pragma once

#include "GameItemTypes.h"

#include <cstdint>
#include <map>
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

class ItemSystem {
public:
    bool AddGroundItem(const GroundItem& item);
    bool AddPlayer(PlayerId playerId);
    std::vector<PickupResult> ResolvePickups(const std::vector<PickupPlayer>& players);
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
