#pragma once
#include <winsock2.h>
#include "Protocol.h"
#include <vector>
#include <chrono>

enum class LobbyStage {
    Title,
    Joining,
    Matching,
    Ready,
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
            roundId != 0;
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
};

// Set only while main owns the connected client; offline input stays unchanged.
extern ClientNetwork* activeClientNetwork;
