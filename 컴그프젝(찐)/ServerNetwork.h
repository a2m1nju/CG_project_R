#pragma once

#include <winsock2.h>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

class ServerNetwork {
public:
    ServerNetwork() = default;
    ~ServerNetwork() {
        Shutdown();
    }
    ServerNetwork(const ServerNetwork&) = delete;
    ServerNetwork& operator=(const ServerNetwork&) = delete;

    bool Initialize(std::uint16_t port);
    void RunAcceptLoop();
    void StopAccepting();
    void Shutdown();

private:
    void RemoveSession(std::size_t index);

    struct TransportSession {
        std::uint32_t id = 0;
        SOCKET socket = INVALID_SOCKET;
        // 패킷 처리 전까지 세션별로 최대 16 KiB 보관
        std::array<char, 16 * 1024> receiveBuffer{};
        std::size_t receivedSize = 0;
    };

    // 게임 참가 정원과 별도로 TCP 연결 수 제한
    std::array<TransportSession, 8> sessions{};
    std::size_t sessionCount = 0;
    std::uint32_t nextSessionId = 1;
    std::atomic<bool> stopping{false};
    SOCKET listenSocket = INVALID_SOCKET;
    bool winsockInitialized = false;
};
