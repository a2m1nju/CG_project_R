#pragma once

#include "Room.h"
#include "ServerNetwork.h"

class ServerApp {
public:
    ServerApp()
        : matchCoordinator(room, world),
        roundController(room, world),
        rematchController(room, world),
        network(world)
    {
    }

    ServerApp(const ServerApp&) = delete;
    ServerApp& operator=(const ServerApp&) = delete;

    bool Initialize(std::uint16_t port);
    void Run();
    void Shutdown();

private:
    WorldState world;
    RoomState room;

    MatchCoordinator matchCoordinator;
    RoundController roundController;
    RematchController rematchController;

    ServerNetwork network;

    std::uint16_t port = 0;
    bool initialized = false;
};