#include "../컴그프젝(찐)/ClientNetwork.h"
#include <cassert>
#include <chrono>
#include <thread>
#include <iostream>

template<class Predicate> void Wait(ClientNetwork& client, Predicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!predicate()) {
        assert(!client.PollDisconnected());
        assert(std::chrono::steady_clock::now() < deadline);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}

int main() {
    ClientNetwork client;
    assert(client.Connect("127.0.0.1", 9000));
    assert(!client.Ready());
    Wait(client, [&] { return client.Ready(); });
    assert(client.RequestMove(Direction::UP, 0, 0));
    assert(!client.RequestMove(Direction::DOWN, 0, 0));
    Wait(client, [&] { return !client.MovePending(); });
    float x, z;
    assert(client.TakeMove(x, z) && x == 0 && z == -1);
    assert(!client.TakeMove(x, z));
    // A new input during the six-tick cooldown is acknowledged without animation.
    assert(client.RequestMove(Direction::RIGHT, 0, -1));
    Wait(client, [&] { return !client.MovePending(); });
    assert(!client.TakeMove(x, z));
    std::this_thread::sleep_for(std::chrono::milliseconds(220));
    assert(client.RequestMove(Direction::RIGHT, 2.25f, -1));
    Wait(client, [&] { return !client.MovePending(); });
    assert(client.TakeMove(x, z) && x == 3 && z == -1);
    // Old STATE frames already buffered must not acknowledge a new-round input.
    client.ResetRound();
    assert(client.RequestMove(Direction::LEFT, 0, 0));
    Wait(client, [&] { return !client.MovePending(); });
    assert(client.TakeMove(x, z) && x == -1 && z == 0);
    client.Disconnect();
    assert(!client.Ready() && !client.MovePending());
    std::cout << "PASS: native ClientNetwork handshake, pending guard, approval, cooldown, world position, reset\n";
}
