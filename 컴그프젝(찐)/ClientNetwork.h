#pragma once

#include <winsock2.h>
#include <cstdint>

class ClientNetwork {
public:
    ClientNetwork() = default;
    ~ClientNetwork() {
        Disconnect();
    }

    ClientNetwork(const ClientNetwork&) = delete;
    ClientNetwork& operator=(const ClientNetwork&) = delete;

    bool Connect(const char* serverIp, std::uint16_t port);
    bool PollDisconnected();
    void Disconnect();

private:
    SOCKET serverSocket = INVALID_SOCKET;
    bool winsockInitialized = false;
};
