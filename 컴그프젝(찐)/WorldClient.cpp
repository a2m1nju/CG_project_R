#include "ClientNetwork.h"
#include <cmath>
#include <algorithm>

void ClientNetwork::ClearWorld()
{
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
            lobby.stage != LobbyStage::Loading)
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
            size != 17 + 17 * p[16])
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

        for (std::size_t offset = 17;
            offset < size;
            offset += 17)
        {
            RemotePlayerView player;

            player.id = Wire::Read32(p + offset);
            player.x = Wire::ReadFloat(p + offset + 4);
            player.z = Wire::ReadFloat(p + offset + 8);
            player.rotation =
                Wire::ReadFloat(p + offset + 12);
            player.alive = p[offset + 16] != 0;

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
                p[offset + 16] > 1)
            {
                return -1;
            }

            player.slot = previous->slot;
            next.push_back(player);
        }

        players = std::move(next);
    }
    else {
        return -1;
    }

    return 1;
}