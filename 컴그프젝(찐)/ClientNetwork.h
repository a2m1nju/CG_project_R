#pragma once
#include <winsock2.h>
#include "Protocol.h"
#include <vector>
#include <chrono>
#include <deque>
#include <set>

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
    enum class Kind {
        Begin,
        Lane,
        Actor,
        End
    } kind;

    std::uint32_t roundId = 0;
    std::uint32_t generation = 0;

    std::int32_t z = 0;
    std::int32_t minZ = 0;
    std::int32_t maxZ = 0;

    std::uint8_t terrain = 0;
    std::uint32_t treeMask = 0;
    std::uint32_t padMask = 0;
    std::uint32_t spawnMask = 0;

    std::uint32_t tick = 0;
    std::uint32_t actorId = 0;

    std::uint8_t actorKind = 0;
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;
    std::uint8_t design = 0;

    std::uint8_t trainState = 0;
    std::uint8_t trainLight = 0;
    std::uint16_t trainTimer = 0;

    float actorZ = 0;
    float x = 0;
    float speed = 0;
    float width = 0;
};

struct RemotePlayerView {
    std::uint32_t id = 0;
    std::uint8_t slot = 0;

    float x = 0;
    float z = 0;
    float rotation = 0;

    bool alive = true;
};

class ClientNetwork {
public:
    ClientNetwork() = default;
    ~ClientNetwork() { Disconnect(); }
    ClientNetwork(const ClientNetwork&) = delete;
    ClientNetwork& operator=(const ClientNetwork&) = delete;
    bool Connect(const char* serverIp, std::uint16_t port);
    bool PollDisconnected(); // Pumps framed receive and partial send queues.
    void Disconnect();
    bool IsConnected() const { return serverSocket != INVALID_SOCKET; }
    bool Ready() const
    {
        return IsConnected() &&
            lobby.stage == LobbyStage::Playing &&
            roundId != 0 && worldReady;
    }
    std::uint32_t GetMyPlayerId() const {
        return myPlayerId;
    }
    bool RequestMove(Direction direction, float x, float z);
    bool TakeMove(float& x, float& z);
    bool MovePending() const { return pendingInput != 0; }
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
private:
    bool Queue(const std::uint8_t* p, std::size_t size);
    bool Flush();
    SOCKET serverSocket = INVALID_SOCKET;
    bool winsockInitialized = false;
    std::vector<std::uint8_t> receiveBuffer, sendBuffer;
    std::uint32_t sequence = 0, inputSeq = 0, pendingInput = 0, roundId = 0;
    std::uint32_t myPlayerId = 0;
    std::chrono::steady_clock::time_point lastState{}, pendingSince{};
    bool pollingStarted = false, moveReady = false;
    float moveX = 0, moveZ = 0;
    LobbyView lobby;

    int HandleLobbyPacket(
        const std::uint8_t* packet,
        std::size_t size);
    std::uint32_t worldGeneration = 0;

    std::int32_t initialMinZ = 0;
    std::int32_t initialMaxZ = -1;

    std::set<std::int32_t> initialLanes;
    std::deque<WorldEvent> worldEvents;
    std::vector<RemotePlayerView> players;

    bool worldReady = false;
    bool loadedSent = false;
    bool startReady = false;
    void ClearWorld();
    int HandleWorldPacket(const std::uint8_t* packet, std::size_t size);
};

// Set only while main owns the connected client; offline input stays unchanged.
extern ClientNetwork* activeClientNetwork;
