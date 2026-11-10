#include "../컴그프젝(찐)/ClientNetwork.h"
#include <chrono>
#include <stdexcept>
#include <thread>
#include <iostream>

// Keep the test checks active in Release builds.
#define CHECK(condition) do { \
    if (!(condition)) throw std::runtime_error(#condition); \
} while (false)

template<class Predicate> void Wait(ClientNetwork& client, Predicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!predicate()) {
        CHECK(!client.PollDisconnected());
        CHECK(std::chrono::steady_clock::now() < deadline);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}

int main() try {
    ClientNetwork client;
    CHECK(client.Connect("127.0.0.1", 9000));
    CHECK(!client.Ready());
    Wait(client, [&] { return client.Ready(); });
    CHECK(client.RequestMove(Direction::UP, 0, 0));
    CHECK(!client.RequestMove(Direction::DOWN, 0, 0));
    Wait(client, [&] { return !client.MovePending(); });
    float x, z;
    CHECK(client.TakeMove(x, z) && x == 0 && z == -1);
    CHECK(!client.TakeMove(x, z));
    // A new input during the six-tick cooldown is acknowledged without animation.
    CHECK(client.RequestMove(Direction::RIGHT, 0, -1));
    Wait(client, [&] { return !client.MovePending(); });
    CHECK(!client.TakeMove(x, z));
    std::this_thread::sleep_for(std::chrono::milliseconds(220));
    CHECK(client.RequestMove(Direction::RIGHT, 2.25f, -1));
    Wait(client, [&] { return !client.MovePending(); });
    CHECK(client.TakeMove(x, z) && x == 3 && z == -1);
    // Old STATE frames already buffered must not acknowledge a new-round input.
    client.ResetRound();
    CHECK(client.RequestMove(Direction::LEFT, 0, 0));
    Wait(client, [&] { return !client.MovePending(); });
    CHECK(client.TakeMove(x, z) && x == -1 && z == 0);
    client.Disconnect();
    CHECK(!client.Ready() && !client.MovePending());
    std::cout << "PASS: native ClientNetwork handshake, pending guard, approval, cooldown, world position, reset\n";
} catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
}
