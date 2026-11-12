#pragma once

#include "ServerNetwork.h"

class ServerApp {
public:
    ServerApp() = default;
    ServerApp(const ServerApp&) = delete;
    ServerApp& operator=(const ServerApp&) = delete;

    bool Initialize(std::uint16_t port);
    void Run();
    void Shutdown();

private:
    ServerNetwork network;

    std::uint16_t port = 0;
    bool initialized = false;
};
