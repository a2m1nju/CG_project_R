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

// 서버가 확정한 현재 Z 좌표를 밀리 단위 정수로 전달한다.
struct TargetPlayer {
    PlayerId id;
    std::int32_t zMilli;
    bool alive;
};

class ItemSystem {
public:
    bool AddGroundItem(const GroundItem& item);
    bool AddPlayer(PlayerId playerId);
    bool SwitchSlot(PlayerId playerId, std::uint8_t slot);
    bool UseSelectedItem(PlayerId playerId, std::int32_t x, std::int32_t z);
    bool ResolveBananaHit(PlayerId playerId, std::int32_t x, std::int32_t z);
    void AdvanceEffects(std::uint32_t elapsedMs);
    std::vector<PickupResult> ResolvePickups(const std::vector<PickupPlayer>& players);
    PlayerId SelectAttackTarget(PlayerId userId,
        const std::vector<TargetPlayer>& players) const;
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