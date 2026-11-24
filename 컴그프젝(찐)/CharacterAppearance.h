#pragma once

#include <cstdint>

enum class CharacterId : std::uint8_t {
    Chicken = 0,
    Rabbit = 1,
    Cat = 2,
    Dog = 3,
    Bear = 4,
    Panda = 5,
    Tino = 6,
    Monokuma = 7,
    Korilakkuma = 8,
    Kuromi = 9
};

inline bool IsValidCharacterId(std::uint8_t id) {
    return id <= static_cast<std::uint8_t>(CharacterId::Kuromi);
}

inline CharacterId NormalizeCharacterId(std::uint8_t id) {
    return IsValidCharacterId(id) ? static_cast<CharacterId>(id) : CharacterId::Chicken;
}
