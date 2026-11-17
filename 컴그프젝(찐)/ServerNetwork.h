#pragma once
#include <winsock2.h>
#include "Protocol.h"
#include <array>
#include <atomic>
#include <vector>
#include <string>

class MatchCoordinator;

class ServerNetwork {
public:
    explicit ServerNetwork(MatchCoordinator& matchCoordinator)
        : matchCoordinator(matchCoordinator) {}
    ~ServerNetwork() { Shutdown(); }
    ServerNetwork(const ServerNetwork&) = delete;
    ServerNetwork& operator=(const ServerNetwork&) = delete;
    bool Initialize(std::uint16_t port);
    void RunAcceptLoop();
    void StopAccepting();
    void Shutdown();
private:
    // 서버 내부에서 TCP 연결 하나의 소켓과 송수신 상태를 관리한다.
    // struct 멤버는 기본 public이고, 타입 자체는 ServerNetwork 내부에 둔다.
    struct TransportSession {
        std::uint32_t id = 0;
        std::uint32_t playerId = 0;
        bool joined = false; //TCP 접속과 게임 참가 승인 구분
        std::string nickname;
        SOCKET socket = INVALID_SOCKET;
        std::vector<std::uint8_t> receiveBuffer, sendBuffer;
        std::uint32_t sequence = 0, inputSeq = 0, roundId = 1, sendSeq = 0;
        std::uint64_t nextMoveTick = 0;
        float x = 0, z = 0;
        bool alive = true;
        Wire::MoveStatus status = Wire::MoveStatus::Idle;
    };
    void RemoveSession(std::size_t index);
    void BroadcastMatchState();
    bool matchStateDirty = false;
    std::uint32_t lastBroadcastLobbyEpoch = 0;
    bool QueueRequestError(TransportSession& session, std::uint8_t type,
                           std::uint32_t sequence, std::uint16_t error);
    bool HandleReady(TransportSession& session, const std::uint8_t* packet,
                     std::size_t size, std::uint32_t sequence);
    bool HandleJoin(TransportSession& session, const std::uint8_t* packet,
                    std::size_t size, std::uint32_t sequence);
    bool ProcessPackets(TransportSession& session, std::uint64_t tick);
    bool Flush(TransportSession& session);
    void Tick(std::uint64_t tick);
    MatchCoordinator& matchCoordinator;
    std::array<TransportSession, 8> sessions{};
    std::size_t sessionCount = 0;
    std::uint32_t nextSessionId = 1;
    std::atomic<bool> stopping{false};
    SOCKET listenSocket = INVALID_SOCKET;
    bool winsockInitialized = false;
};
