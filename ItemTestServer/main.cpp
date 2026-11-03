#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>

#include "../컴그프젝(찐)/ItemSystem.h"

#include <cstdint>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#pragma comment(lib, "Ws2_32.lib")

namespace {
constexpr unsigned short kPort = 19001;
constexpr std::size_t kMaxLine = 1024;

bool SendResponse(SOCKET socket, const std::string& message) {
    std::size_t sent = 0;
    while (sent < message.size()) {
        const int count = send(socket, message.data() + sent,
            static_cast<int>(message.size() - sent), 0);
        if (count <= 0) return false;
        sent += static_cast<std::size_t>(count);
    }
    return true;
}

template <typename T>
bool ReadNumber(std::istringstream& input, T& output) {
    long long value = 0;
    if (!(input >> value) || value < 0 ||
        static_cast<unsigned long long>(value) >
            static_cast<unsigned long long>(std::numeric_limits<T>::max())) {
        return false;
    }
    output = static_cast<T>(value);
    return true;
}

bool AtEnd(std::istringstream& input) {
    std::string extra;
    return !(input >> extra);
}

std::string HandleCommand(ItemSystem& items, const std::string& line) {
    std::istringstream input(line);
    std::string command;
    input >> command;

    if (command == "ADD_PLAYER") {
        PlayerId id = 0;
        if (!ReadNumber(input, id) || !AtEnd(input)) return "ERR usage: ADD_PLAYER id\n";
        return items.AddPlayer(id) ? "OK player\n" : "ERR invalid_or_duplicate_player\n";
    }
    if (command == "SPAWN") {
        ObjectId id = 0;
        std::uint8_t kind = 0;
        std::int32_t x = 0, z = 0;
        if (!ReadNumber(input, id) || !ReadNumber(input, kind) ||
            !(input >> x >> z) || !AtEnd(input)) {
            return "ERR usage: SPAWN id kind(1..3) x z\n";
        }
        GroundItem item{id, static_cast<ItemKind>(kind), x, z};
        return items.AddGroundItem(item) ? "OK item\n" : "ERR invalid_or_duplicate_item\n";
    }
    if (command == "PICKUP") {
        PlayerId id = 0;
        std::int32_t x = 0, z = 0;
        if (!ReadNumber(input, id) || !(input >> x >> z) || !AtEnd(input)) {
            return "ERR usage: PICKUP playerId x z\n";
        }
        if (!items.GetInventory(id)) return "ERR unknown_player\n";
        const auto results = items.ResolvePickups({PickupPlayer{id, x, z, true}});
        std::ostringstream response;
        response << "OK pickups=" << results.size();
        for (const auto& result : results) {
            response << " item=" << result.objectId << ":slot="
                     << static_cast<unsigned>(result.slot);
        }
        return response.str() + "\n";
    }
    if (command == "TICK") {
        std::uint8_t count = 0;
        if (!ReadNumber(input, count) || count == 0 || count > 3) {
            return "ERR usage: TICK count(1..3) [playerId x z alive(0|1)]...\n";
        }
        std::vector<PickupPlayer> players;
        std::set<PlayerId> seen;
        for (unsigned i = 0; i < count; ++i) {
            PlayerId id = 0;
            std::int32_t x = 0, z = 0;
            std::uint8_t alive = 0;
            if (!ReadNumber(input, id) || !(input >> x >> z) ||
                !ReadNumber(input, alive) || alive > 1 ||
                !items.GetInventory(id) || !seen.insert(id).second) {
                return "ERR invalid_tick_player\n";
            }
            players.push_back({id, x, z, alive == 1});
        }
        if (!AtEnd(input)) return "ERR extra_tick_fields\n";
        const auto results = items.ResolvePickups(players);
        std::ostringstream response;
        response << "OK pickups=" << results.size();
        for (const auto& result : results) {
            response << " player=" << result.playerId << ":item="
                     << result.objectId << ":slot="
                     << static_cast<unsigned>(result.slot);
        }
        return response.str() + "\n";
    }
    if (command == "STATUS") {
        PlayerId id = 0;
        if (!ReadNumber(input, id) || !AtEnd(input)) return "ERR usage: STATUS playerId\n";
        const auto* inventory = items.GetInventory(id);
        if (!inventory) return "ERR unknown_player\n";
        std::ostringstream response;
        response << "OK slots=" << static_cast<unsigned>(inventory->slots[0])
                 << "," << static_cast<unsigned>(inventory->slots[1])
                 << " selected=" << static_cast<unsigned>(inventory->selectedSlot)
                 << " ground=" << items.GetGroundItems().size() << "\n";
        return response.str();
    }
    if (command == "RESET" && AtEnd(input)) {
        items.ResetRound();
        return "OK reset\n";
    }
    if (command == "QUIT" && AtEnd(input)) return "BYE\n";
    return "ERR commands: ADD_PLAYER, SPAWN, PICKUP, TICK, STATUS, RESET, QUIT\n";
}

void ServeClient(SOCKET client, ItemSystem& items) {
    if (!SendResponse(client, "OK item test server ready\n")) return;
    std::string pending;
    char buffer[512];
    while (true) {
        const int count = recv(client, buffer, sizeof(buffer), 0);
        if (count <= 0) break;
        pending.append(buffer, count);
        for (std::size_t newline; (newline = pending.find('\n')) != std::string::npos;) {
            std::string line = pending.substr(0, newline);
            pending.erase(0, newline + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.size() > kMaxLine) {
                SendResponse(client, "ERR line_too_long\n");
                return;
            }
            const std::string response = HandleCommand(items, line);
            if (!SendResponse(client, response) || response == "BYE\n") return;
        }
        if (pending.size() > kMaxLine) {
            SendResponse(client, "ERR line_too_long\n");
            return;
        }
    }
}
}

int main() {
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }
    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) {
        std::cerr << "socket failed\n";
        WSACleanup();
        return 1;
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(kPort);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR ||
        listen(listener, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "bind/listen failed: " << WSAGetLastError() << "\n";
        closesocket(listener);
        WSACleanup();
        return 1;
    }
    std::cout << "Item test server listening on 127.0.0.1:" << kPort << "\n";
    ItemSystem items;
    while (true) {
        SOCKET client = accept(listener, nullptr, nullptr);
        if (client == INVALID_SOCKET) break;
        ServeClient(client, items);
        closesocket(client);
    }
    closesocket(listener);
    WSACleanup();
    return 0;
}
