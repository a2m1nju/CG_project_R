#include "ClientNetwork.h"
#include <cmath>
#include <iostream>
#include <algorithm>

void ClientNetwork::ClearWorld()
{
    ClearItems();
    roundId = 0;
    inputSeq = 0;
    pendingInput = 0;
    worldGeneration = 0;

    worldReady = false;
    loadedSent = false;
    startReady = false;
    moveReady = false;

    initialMinZ = 0;
    initialMaxZ = -1;

    initialLanes.clear();
    worldEvents.clear();
    players.clear();
    lastVisualSent = {};
}

bool ClientNetwork::TakeWorldEvent(WorldEvent& event)
{
    if (worldEvents.empty()) {
        return false;
    }

    event = worldEvents.front();
    worldEvents.pop_front();

    if (event.kind == WorldEvent::Kind::End) {
        worldReady = true;
    }

    return true;
}

bool ClientNetwork::RequestLoaded()
{
    if (!IsConnected() ||
        lobby.stage != LobbyStage::Loading ||
        !worldReady ||
        !itemReady ||
        !worldEvents.empty() ||
        !itemEvents.empty() ||
        loadedSent)
    {
        return false;
    }
    std::array<std::uint8_t, Wire::HeaderSize> packet{};
    Wire::Header(
        packet.data(),
        packet.size(),
        Wire::Loaded,
        ++sequence,
        roundId);

    loadedSent = true;

    return Queue(packet.data(), packet.size());
}

bool ClientNetwork::TakeStart(float& x, float& z)
{
    if (!startReady) {
        return false;
    }

    for (const auto& player : players) {
        if (player.id == myPlayerId) {
            x = player.x;
            z = player.z;
            startReady = false;
            return true;
        }
    }

    return false;
}

int ClientNetwork::HandleWorldPacket(
    const std::uint8_t* p,
    std::size_t size)
{
    if (p[3] < Wire::WorldLane ||
        p[3] > Wire::PlayerStates)
    {
        return 0;
    }

    if (!lobby.joined || lobby.leaving) {
        return 1;
    }

    if (p[3] == Wire::WorldBegin &&
        size == Wire::WorldBeginSize)
    {
        if (lobby.stage != LobbyStage::Ready &&
            lobby.stage != LobbyStage::Loading &&
            !(lobby.stage == LobbyStage::Playing && resetRequested))
        {
            return -1;
        }
        const auto round = Wire::ReadRoundId(p);
        const auto generation = Wire::Read32(p + 12);

        const auto minZ =
            static_cast<std::int32_t>(Wire::Read32(p + 16));

        const auto maxZ =
            static_cast<std::int32_t>(Wire::Read32(p + 20));

        if (round == 0 ||
            generation == 0 ||
            minZ > maxZ ||
            static_cast<std::int64_t>(maxZ) - minZ > 4096)
        {
            return -1;
        }

        if (roundId != 0 &&
            !Wire::Newer(round, roundId) &&
            round != roundId)
        {
            return 1;
        }

        if (round == roundId &&
            generation < worldGeneration)
        {
            return 1;
        }

        if (round != roundId ||
            generation != worldGeneration)
        {
            ClearWorld();
            result = ResultData{};
            lobby.stage = LobbyStage::Loading;
            lobby.message = "LOADING SHARED WORLD...";

            roundId = round;
            worldGeneration = generation;
            initialMinZ = minZ;
            initialMaxZ = maxZ;

            WorldEvent event{};
            event.kind = WorldEvent::Kind::Begin;
            event.roundId = round;
            event.generation = generation;
            event.minZ = minZ;
            event.maxZ = maxZ;

            worldEvents.push_back(event);
        }
    }
    else if (p[3] == Wire::WorldLane &&
        size == Wire::WorldLaneSize)
    {
        const auto round = Wire::ReadRoundId(p);
        const auto generation = Wire::Read32(p + 12);

        if (round == roundId &&
            generation == worldGeneration)
        {
            WorldEvent event{};
            event.kind = WorldEvent::Kind::Lane;
            event.roundId = round;
            event.generation = generation;

            event.z =
                static_cast<std::int32_t>(Wire::Read32(p + 16));

            event.terrain = p[20];
            event.treeMask = Wire::Read32(p + 21);
            event.padMask = Wire::Read32(p + 25);
            event.spawnMask = Wire::Read32(p + 29);

            if (event.terrain > 3 ||
                ((event.treeMask |
                    event.padMask |
                    event.spawnMask) & 0x80000000u) != 0)
            {
                return -1;
            }

            if (!worldReady &&
                event.z >= initialMinZ &&
                event.z <= initialMaxZ)
            {
                initialLanes.insert(event.z);
            }

            worldEvents.push_back(event);
        }
    }
    else if (p[3] == Wire::WorldActor &&
        size == Wire::WorldActorSize)
    {
        const auto round = Wire::ReadRoundId(p);
        const auto generation = Wire::Read32(p + 12);

        if (round == roundId &&
            generation == worldGeneration)
        {
            WorldEvent event{};
            event.kind = WorldEvent::Kind::Actor;
            event.roundId = round;
            event.generation = generation;

            event.tick = Wire::Read32(p + 16);
            event.actorId = Wire::Read32(p + 20);
            event.actorKind = p[24];

            event.actorZ = Wire::ReadFloat(p + 25);
            event.x = Wire::ReadFloat(p + 29);
            event.speed = Wire::ReadFloat(p + 33);
            event.width = Wire::ReadFloat(p + 37);

            event.red = p[41];
            event.green = p[42];
            event.blue = p[43];
            event.design = p[44];

            event.trainState = p[45];
            event.trainTimer =
                (std::uint16_t(p[46]) << 8) | p[47];
            event.trainLight = p[48];

            if (event.actorId == 0 ||
                event.actorKind < 1 ||
                event.actorKind > 3 ||
                event.design > 3 ||
                event.trainState > 2 ||
                event.trainLight > 1 ||
                !std::isfinite(event.actorZ) ||
                !std::isfinite(event.x) ||
                !std::isfinite(event.speed) ||
                !std::isfinite(event.width))
            {
                return -1;
            }

            worldEvents.push_back(event);
        }
    }
    else if (p[3] == Wire::WorldEnd &&
        size == Wire::WorldEndSize)
    {
        const auto round = Wire::ReadRoundId(p);
        const auto generation = Wire::Read32(p + 12);

        if (round == roundId &&
            generation == worldGeneration)
        {
            const auto expected =
                static_cast<std::size_t>(
                    static_cast<std::int64_t>(initialMaxZ) -
                    initialMinZ + 1);

            if (initialLanes.size() != expected) {
                return -1;
            }

            WorldEvent event{};
            event.kind = WorldEvent::Kind::End;
            event.roundId = round;
            event.generation = generation;

            worldEvents.push_back(event);
        }
    }
    else if (p[3] == Wire::Start)
    {
        if (size < 19 ||
            lobby.stage != LobbyStage::Loading ||
            !worldReady ||
            !loadedSent ||
            Wire::ReadRoundId(p) != roundId ||
            LobbyWire::Read16(p + 16) != Wire::TickRate ||
            p[18] != 3 ||
            size != 19 + 13 * p[18])
        {
            return -1;
        }

        std::vector<RemotePlayerView> next;
        std::set<std::uint32_t> ids;
        std::set<std::uint8_t> slots;

        for (std::size_t offset = 19;
            offset < size;
            offset += 13)
        {
            RemotePlayerView player;
            player.id = Wire::Read32(p + offset);
            player.slot = p[offset + 4];
            player.x = Wire::ReadFloat(p + offset + 5);
            player.z = Wire::ReadFloat(p + offset + 9);
            player.visualX = player.previousVisualX = player.x;
            player.visualZ = player.previousVisualZ = player.z;

            if (player.id == 0 ||
                player.slot > 2 ||
                !ids.insert(player.id).second ||
                !slots.insert(player.slot).second ||
                !std::isfinite(player.x) ||
                !std::isfinite(player.z))
            {
                return -1;
            }

            next.push_back(player);
        }

        if (!ids.count(myPlayerId)) {
            return -1;
        }

        players = std::move(next);
        startReady = true;

        lobby.stage = LobbyStage::Playing;
        lobby.message = "PLAYING";
    }
    else if (p[3] == Wire::PlayerStates)
    {
        if (size < 17 ||
            p[16] > 3 ||
            size != 17 + Wire::PlayerStateRecordSize * p[16])
        {
            return -1;
        }

        if (Wire::ReadRoundId(p) != roundId ||
            lobby.stage != LobbyStage::Playing)
        {
            return 1;
        }

        std::vector<RemotePlayerView> next;
        std::set<std::uint32_t> ids;

        const auto receivedAt = std::chrono::steady_clock::now();

        for (std::size_t offset = 17;
            offset < size;
            offset += Wire::PlayerStateRecordSize)
        {
            RemotePlayerView player;
            player.id = Wire::Read32(p + offset);
            player.x = Wire::ReadFloat(p + offset + 4);
            player.z = Wire::ReadFloat(p + offset + 8);
            player.rotation = Wire::ReadFloat(p + offset + 12);
            player.alive = p[offset + 16] != 0;

            player.visualX = Wire::ReadFloat(p + offset + 17);
            player.visualY = Wire::ReadFloat(p + offset + 21);
            player.visualZ = Wire::ReadFloat(p + offset + 25);

            const auto previous = std::find_if(
                players.begin(),
                players.end(),
                [&player](const RemotePlayerView& old) {
                    return old.id == player.id;
                });

            if (!ids.insert(player.id).second ||
                previous == players.end() ||
                !std::isfinite(player.x) ||
                !std::isfinite(player.z) ||
                !std::isfinite(player.rotation) ||
                !std::isfinite(player.visualX) ||
                !std::isfinite(player.visualY) ||
                !std::isfinite(player.visualZ) ||
                player.visualY < 0.0f ||
                player.visualY > 4.0f ||
                p[offset + 16] > 1)
            {
                return -1;
            }
            const auto displayed = previous->GetRenderPosition();
            player.slot = previous->slot;
            player.previousVisualX = displayed[0];
            player.previousVisualY = displayed[1];
            player.previousVisualZ = displayed[2];

            player.visualReceivedAt = receivedAt;

            next.push_back(player);
        }
        players = std::move(next);
        }
    else {
        return -1;
    }

    return 1;
}

void ClientNetwork::ClearItems()
{
    itemEvents.clear();
    initialItems.clear();
    pendingPickups.clear();

    expectedItems = 0;
    itemSnapshotOpen = false;
    itemReady = false;
    resetRequested = false;
}

int ClientNetwork::HandleItemPacket(
    const std::uint8_t* p,
    std::size_t size)
{
    if (p[3] < Wire::ItemBegin || p[3] > Wire::ItemSpawned) {
        return 0;
    }

    if (!lobby.joined || lobby.leaving) {
        return 1;
    }

    if (p[3] == Wire::ItemIdentity &&
        size == Wire::ItemIdentitySize)
    {
        if (Wire::Read32(p + 12) != myPlayerId) {
            return -1;
        }
    }
    else if (p[3] == Wire::ItemBegin &&
        size == Wire::ItemBeginSize)
    {
        if (Wire::ReadRoundId(p) == roundId &&
            !itemReady &&
            !itemSnapshotOpen)
        {
            expectedItems = Wire::Read32(p + 12);

            if (expectedItems > 512) {
                return -1;
            }

            initialItems.clear();
            itemSnapshotOpen = true;
        }
    }
    else if (p[3] == Wire::ItemEntry &&
        size == Wire::ItemEntrySize)
    {
        if (Wire::ReadRoundId(p) == roundId && itemSnapshotOpen) {
            ItemEvent event{};
            event.kind = ItemEvent::Kind::Snapshot;
            event.id = Wire::Read32(p + 12);
            event.itemKind = p[16];
            event.x = static_cast<std::int32_t>(
                Wire::Read32(p + 17));
            event.z = static_cast<std::int32_t>(
                Wire::Read32(p + 21));

            const bool duplicate = std::any_of(
                initialItems.begin(),
                initialItems.end(),
                [&event](const ItemEvent& item) {
                    return item.id == event.id;
                });

            if (event.id == 0 ||
                event.itemKind < 1 ||
                event.itemKind > 4 ||
                initialItems.size() >= expectedItems ||
                duplicate)
            {
                return -1;
            }

            initialItems.push_back(event);
        }
    }
    else if (p[3] == Wire::ItemEnd &&
        size == Wire::ItemEndSize)
    {
        if (Wire::ReadRoundId(p) == roundId && itemSnapshotOpen) {
            if (initialItems.size() != expectedItems) {
                return -1;
            }

            for (const auto& item : initialItems) {
                itemEvents.push_back(item);
            }

            initialItems.clear();
            itemSnapshotOpen = false;
            itemReady = true;
        }
    }
    else if (p[3] == Wire::ItemSpawned &&
        size == Wire::ItemSpawnedSize)
    {
        if (Wire::ReadRoundId(p) == roundId && itemReady) {
            ItemEvent event{};
            event.kind = ItemEvent::Kind::Snapshot;
            event.id = Wire::Read32(p + 12);
            event.itemKind = p[16];
            event.x = static_cast<std::int32_t>(
                Wire::Read32(p + 17));
            event.z = static_cast<std::int32_t>(
                Wire::Read32(p + 21));

            if (event.id == 0 ||
                event.itemKind < 1 ||
                event.itemKind > 4)
            {
                return -1;
            }

            itemEvents.push_back(event);
        }
    }
    else if (p[3] == Wire::ItemRemoved &&
        size == Wire::ItemRemovedSize)
    {
        if (Wire::ReadRoundId(p) == roundId && itemReady) {
            ItemEvent event{};
            event.kind = ItemEvent::Kind::Removed;
            event.id = Wire::Read32(p + 12);
            event.playerId = Wire::Read32(p + 16);
            event.slot = p[20];

            if (event.id == 0 ||
                event.playerId == 0 ||
                event.slot >= 2)
            {
                return -1;
            }

            pendingPickups.erase(event.id);
            itemEvents.push_back(event);
        }
    }
    else if (p[3] == Wire::InventoryState &&
        size == Wire::InventoryStateSize)
    {
        if (Wire::ReadRoundId(p) == roundId && itemReady) {
            if (p[12] >= 2 ||
                p[13] > 4 ||
                p[14] > 4 ||
                p[15] > 1)
            {
                return -1;
            }

            ItemEvent event{};
            event.kind = ItemEvent::Kind::Inventory;
            event.slot = p[12];
            event.itemKind = p[13];
            event.secondKind = p[14];
            event.accepted = p[15] != 0;

            itemEvents.push_back(event);
        }
    }
    else if (p[3] == Wire::ItemEffect &&
        size == Wire::ItemEffectSize)
    {
        if (Wire::ReadRoundId(p) == roundId && itemReady) {
            const auto duration = Wire::Read32(p + 13);

            if ((p[12] != 1 && p[12] != 2 && p[12] != 4) ||
                duration == 0 ||
                duration > 60000)
            {
                return -1;
            }

            ItemEvent event{};
            event.kind = ItemEvent::Kind::Effect;
            event.itemKind = p[12];
            event.durationMs = duration;

            itemEvents.push_back(event);
        }
    }
    else if (p[3] == Wire::BananaPlaced &&
        size == Wire::BananaPlacedSize)
    {
        if (Wire::ReadRoundId(p) == roundId && itemReady) {
            ItemEvent event{};
            event.kind = ItemEvent::Kind::BananaPlaced;
            event.id = Wire::Read32(p + 12);
            event.playerId = Wire::Read32(p + 16);
            event.x = static_cast<std::int32_t>(
                Wire::Read32(p + 20));
            event.z = static_cast<std::int32_t>(
                Wire::Read32(p + 24));

            if (event.id == 0 || event.playerId == 0) {
                return -1;
            }

            itemEvents.push_back(event);
        }
    }
    else if (p[3] == Wire::BananaHit &&
        size == Wire::BananaHitSize)
    {
        if (Wire::ReadRoundId(p) == roundId && itemReady) {
            ItemEvent event{};
            event.kind = ItemEvent::Kind::BananaHit;
            event.id = Wire::Read32(p + 12);
            event.playerId = Wire::Read32(p + 16);
            event.x = static_cast<std::int32_t>(
                Wire::Read32(p + 20));
            event.z = static_cast<std::int32_t>(
                Wire::Read32(p + 24));
            event.accepted = p[28] == 1;

            if (event.id == 0 ||
                event.playerId == 0 ||
                p[28] > 1)
            {
                return -1;
            }

            itemEvents.push_back(event);
        }
    }
    else {
        return -1;
    }

    return 1;
}

bool ClientNetwork::TakeItemEvent(ItemEvent& event)
{
    if (itemEvents.empty()) {
        return false;
    }

    event = itemEvents.front();
    itemEvents.pop_front();
    return true;
}

bool ClientNetwork::RequestPickup(std::uint32_t itemId)
{
    if (!Ready() || itemId == 0) {
        return false;
    }

    const auto now = std::chrono::steady_clock::now();
    const auto pending = pendingPickups.find(itemId);

    if (pending != pendingPickups.end() &&
        now - pending->second < std::chrono::milliseconds(500))
    {
        return false;
    }

    std::array<std::uint8_t, Wire::PickupRequestSize> packet{};

    Wire::Header(
        packet.data(), packet.size(),
        Wire::PickupRequest, ++sequence, roundId);

    Wire::Write32(packet.data() + 12, itemId);

    if (!Queue(packet.data(), packet.size())) {
        return false;
    }

    pendingPickups[itemId] = now;
    return true;
}

bool ClientNetwork::RequestSwitch(std::uint8_t slot)
{
    if (!Ready() || slot >= 2) {
        return false;
    }

    std::array<std::uint8_t, Wire::ItemSwitchSize> packet{};

    Wire::Header(
        packet.data(), packet.size(),
        Wire::ItemSwitch, ++sequence, roundId);

    packet[12] = slot;
    return Queue(packet.data(), packet.size());
}

bool ClientNetwork::RequestUse()
{
    if (!Ready()) {
        return false;
    }

    std::array<std::uint8_t, Wire::ItemUseSize> packet{};

    Wire::Header(
        packet.data(), packet.size(),
        Wire::ItemUse, ++sequence, roundId);

    return Queue(packet.data(), packet.size());
}

bool ClientNetwork::RequestSpawn(std::uint8_t itemKind)
{
    if (!Ready() || itemKind < 1 || itemKind > 4) {
        return false;
    }

    std::array<std::uint8_t, Wire::ItemSpawnRequestSize> packet{};

    Wire::Header(
        packet.data(), packet.size(),
        Wire::ItemSpawnRequest, ++sequence, roundId);

    packet[12] = itemKind;
    return Queue(packet.data(), packet.size());
}