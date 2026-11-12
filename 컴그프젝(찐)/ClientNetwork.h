#pragma once
#include <winsock2.h>
#include "Protocol.h"
#include <vector>
#include <deque>
#include <set>
#include <map>
#include <chrono>

struct WorldEvent {
    enum class Kind { Begin, Lane, Actor, End } kind;
    std::uint32_t roundId = 0, generation = 0;
    std::int32_t z = 0, minZ = 0, maxZ = 0;
    std::uint8_t terrain = 0;
    std::uint32_t treeMask = 0, padMask = 0, spawnMask = 0;
    std::uint32_t tick = 0, actorId = 0;
    std::uint8_t actorKind = 0, red = 0, green = 0, blue = 0, design = 0;
    std::uint8_t trainState = 0, trainLight = 0;
    std::uint16_t trainTimer = 0;
    float actorZ = 0, x = 0, speed = 0, width = 0;
};

struct ItemEvent {
    enum class Kind { Snapshot, Removed } kind;
    std::uint32_t id = 0, playerId = 0;
    std::uint8_t itemKind = 0, slot = 0;
    std::int32_t x = 0, z = 0;
};

class ClientNetwork {
public:
    ClientNetwork() = default;
    ~ClientNetwork() { Disconnect(); }
    ClientNetwork(const ClientNetwork&) = delete;
    ClientNetwork& operator=(const ClientNetwork&) = delete;
    bool Connect(const char* serverIp, std::uint16_t port);
    bool PollDisconnected(); // 프레임 수신과 부분 전송 대기열을 처리한다.
    void Disconnect();
    bool IsConnected() const { return serverSocket != INVALID_SOCKET; }
    bool Ready() const { return IsConnected() && roundId != 0 && worldReady && itemReady; }
    bool TakeWorldEvent(WorldEvent& event);
    bool TakeItemEvent(ItemEvent& event);
    bool RequestPickup(std::uint32_t itemId);
    std::uint32_t PlayerId() const { return playerId; }
    bool RequestMove(Direction direction, float x, float z);
    bool TakeMove(float& x, float& z);
    bool MovePending() const { return pendingInput != 0; }
    void SyncWorld(float x, float z, bool alive);
    void ResetRound();
private:
    bool Queue(const std::uint8_t* p, std::size_t size);
    bool Flush();
    SOCKET serverSocket = INVALID_SOCKET;
    bool winsockInitialized = false;
    std::vector<std::uint8_t> receiveBuffer, sendBuffer;
    std::uint32_t sequence = 0, inputSeq = 0, pendingInput = 0, roundId = 0;
    std::uint32_t worldGeneration = 0;
    std::int32_t initialMinZ = 0, initialMaxZ = -1;
    std::set<std::int32_t> initialLanes;
    std::chrono::steady_clock::time_point lastState{}, pendingSince{};
    bool pollingStarted = false, moveReady = false, worldReady = false, resetRequested = false;
    float moveX = 0, moveZ = 0;
    std::deque<WorldEvent> worldEvents;
    std::deque<ItemEvent> itemEvents;
    std::vector<ItemEvent> initialItems;
    std::map<std::uint32_t, std::chrono::steady_clock::time_point> pendingPickups;
    std::uint32_t playerId = 0, expectedItems = 0;
    bool itemSnapshotOpen = false, itemReady = false;
};

// main이 연결된 클라이언트를 소유할 때만 설정하며, 오프라인 입력은 그대로 유지한다.
extern ClientNetwork* activeClientNetwork;
