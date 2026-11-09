#pragma once
#include <winsock2.h>
#include "Protocol.h"
#include <vector>
#include <chrono>

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
    bool Ready() const { return IsConnected() && roundId != 0; }
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
    std::chrono::steady_clock::time_point lastState{}, pendingSince{};
    bool pollingStarted = false, moveReady = false;
    float moveX = 0, moveZ = 0;
};

// Set only while main owns the connected client; offline input stays unchanged.
extern ClientNetwork* activeClientNetwork;
