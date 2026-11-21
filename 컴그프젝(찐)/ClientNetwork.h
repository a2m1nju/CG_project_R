#pragma once

#include <winsock2.h>
#include "Protocol.h"
#include "ScreenChase.h"

#include <vector>
#include <chrono>
#include <deque>
#include <set>
#include <map>
#include <string>

enum class LobbyStage {
    Title,
    Joining,
    Matching,
    Ready,
    Loading,
    Playing
};

struct LobbyMemberView {
    std::uint32_t id = 0;
    bool ready = false;
    std::string nickname;
};

struct LobbyView {
    LobbyStage stage = LobbyStage::Title;

    bool joined = false;
    bool readyPending = false;
    bool leaving = false;

    std::uint16_t position = 0;
    std::uint16_t count = 0;
    std::uint32_t epoch = 0;

    std::vector<LobbyMemberView> members;
    std::string message;
};

struct WorldEvent {
    enum class Kind { Begin, Lane, Actor, End } kind;

    std::uint32_t roundId = 0, generation = 0;
    std::int32_t z = 0, minZ = 0, maxZ = 0;
    std::uint8_t terrain = 0;
    std::uint32_t treeMask = 0, padMask = 0, spawnMask = 0;
    std::uint32_t tick = 0, actorId = 0;
    std::uint8_t actorKind = 0;
    std::uint8_t red = 0, green = 0, blue = 0, design = 0;
    std::uint8_t trainState = 0, trainLight = 0;
    std::uint16_t trainTimer = 0;
    float actorZ = 0, x = 0, speed = 0, width = 0;
};

struct RemotePlayerView {
    std::uint32_t id = 0;
    std::uint8_t slot = 0;
    float x = 0, z = 0, rotation = 0;
    bool alive = true;
    std::uint32_t shieldRemainingMs = 0;
    float moveStartX = 0, moveStartZ = 0;
    std::chrono::steady_clock::time_point moveStarted{};
};

struct ItemEvent {
    enum class Kind {
        Snapshot,
        Removed,
        Inventory,
        Effect,
        BananaPlaced,
        BananaHit
    } kind;

    std::uint32_t id = 0, playerId = 0;
    std::uint8_t itemKind = 0, slot = 0;
    std::uint8_t secondKind = 0;
    std::uint32_t durationMs = 0;
    bool accepted = false;
    std::int32_t x = 0, z = 0;
};

class ClientNetwork {
public:
    ClientNetwork() = default;
    ~ClientNetwork() { Disconnect(); }

    ClientNetwork(const ClientNetwork&) = delete;
    ClientNetwork& operator=(const ClientNetwork&) = delete;

    bool Connect(const char* serverIp, std::uint16_t port);
    bool PollDisconnected();
    void Disconnect();

    bool IsConnected() const
    {
        return serverSocket != INVALID_SOCKET;
    }

    bool Ready() const
    {
        return IsConnected() &&
            lobby.stage == LobbyStage::Playing &&
            roundId != 0 &&
            worldReady &&
            itemReady &&
            !resetRequested &&
            itemEvents.empty();
    }

    std::uint32_t GetMyPlayerId() const
    {
        return myPlayerId;
    }

    std::uint32_t PlayerId() const
    {
        return myPlayerId;
    }

    bool RequestMove(Direction direction, float x, float z);
    bool TakeMove(float& x, float& z);

    bool MovePending() const
    {
        return pendingInput != 0;
    }

    void SyncWorld(float x, float z, bool alive);
    void ResetRound();

    const LobbyView& GetLobby() const
    {
        return lobby;
    }

    bool RequestJoin(const std::string& nickname);
    bool RequestReady(bool ready);
    bool RequestLeave();

    bool IsMyLobbyReady() const
    {
        for (const auto& member : lobby.members) {
            if (member.id == myPlayerId) {
                return member.ready;
            }
        }
        return false;
    }

    bool TakeWorldEvent(WorldEvent& event);
    bool RequestLoaded();
    bool TakeStart(float& x, float& z);

    const std::vector<RemotePlayerView>& GetPlayers() const
    {
        return players;
    }

    bool ChaseActive() const
    {
        return roundStartTick != 0 && lastPlayersTick != 0 &&
            ScreenChase::Active(std::uint32_t(lastPlayersTick - roundStartTick));
    }

    float ChaseFrontZ() const
    {
        return ScreenChase::FrontZ(
            std::uint32_t(lastPlayersTick - roundStartTick));
    }

    bool TakeItemEvent(ItemEvent& event);
    bool RequestPickup(std::uint32_t itemId);
    bool RequestSwitch(std::uint8_t slot);
    bool RequestUse();
    bool RequestSpawn(std::uint8_t itemKind);

private:
    bool Queue(const std::uint8_t* p, std::size_t size);
    bool Flush();

    SOCKET serverSocket = INVALID_SOCKET;
    bool winsockInitialized = false;

    std::vector<std::uint8_t> receiveBuffer, sendBuffer;

    std::uint32_t sequence = 0;
    std::uint32_t inputSeq = 0;
    std::uint32_t pendingInput = 0;
    std::uint32_t roundId = 0;
    std::uint32_t roundStartTick = 0;
    std::uint32_t lastPlayersTick = 0;
    std::uint32_t myPlayerId = 0;

    std::chrono::steady_clock::time_point lastState{}, pendingSince{};

    bool pollingStarted = false;
    bool moveReady = false;
    float moveX = 0, moveZ = 0;

    LobbyView lobby;

    int HandleLobbyPacket(
        const std::uint8_t* packet,
        std::size_t size);

    std::uint32_t worldGeneration = 0;
    std::int32_t initialMinZ = 0, initialMaxZ = -1;

    std::set<std::int32_t> initialLanes;
    std::deque<WorldEvent> worldEvents;
    std::vector<RemotePlayerView> players;

    bool worldReady = false;
    bool loadedSent = false;
    bool startReady = false;

    std::deque<ItemEvent> itemEvents;
    std::vector<ItemEvent> initialItems;

    std::map<
        std::uint32_t,
        std::chrono::steady_clock::time_point
    > pendingPickups;

    std::uint32_t expectedItems = 0;
    bool itemSnapshotOpen = false;
    bool itemReady = false;
    bool resetRequested = false;

    void ClearWorld();
    void ClearItems();

    int HandleWorldPacket(
        const std::uint8_t* packet,
        std::size_t size);

    int HandleItemPacket(
        const std::uint8_t* packet,
        std::size_t size);
};

extern ClientNetwork* activeClientNetwork;
