#pragma once
#include <winsock2.h>
#include "Protocol.h"
#include <array>
#include <atomic>
#include <vector>

class ServerNetwork {
public:
    ServerNetwork() = default;
    ~ServerNetwork() { Shutdown(); }
    ServerNetwork(const ServerNetwork&) = delete;
    ServerNetwork& operator=(const ServerNetwork&) = delete;
    bool Initialize(std::uint16_t port);
    void RunAcceptLoop();
    void StopAccepting();
    void Shutdown();
private:
    struct TransportSession {
        std::uint32_t id = 0;
        SOCKET socket = INVALID_SOCKET;
        std::vector<std::uint8_t> receiveBuffer, sendBuffer;
        std::uint32_t sequence = 0, inputSeq = 0, roundId = 1, sendSeq = 0;
        std::uint64_t nextMoveTick = 0;
        float x = 0, z = 0;
        bool alive = true;
        Wire::MoveStatus status = Wire::MoveStatus::Idle;
    };
    void RemoveSession(std::size_t index);
    bool ProcessPackets(TransportSession& session, std::uint64_t tick);
    bool Flush(TransportSession& session);
    void Tick(std::uint64_t tick);
    std::array<TransportSession, 8> sessions{};
    std::size_t sessionCount = 0;
    std::uint32_t nextSessionId = 1;
    std::atomic<bool> stopping{false};
    SOCKET listenSocket = INVALID_SOCKET;
    bool winsockInitialized = false;
};
