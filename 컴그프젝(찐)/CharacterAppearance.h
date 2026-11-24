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

enum class HeadAccessory : std::uint8_t { None = 0, Ribbon = 1, Crown = 2 };
enum class BackAccessory : std::uint8_t { None = 0, Backpack = 1 };

struct AccessoryLoadout {
    HeadAccessory head = HeadAccessory::None;
    BackAccessory back = BackAccessory::None;
};

inline bool CanEquipHead(CharacterId character, HeadAccessory item) {
    if (item == HeadAccessory::None) return true;
    if (item == HeadAccessory::Ribbon)
        return character == CharacterId::Rabbit || character == CharacterId::Cat;
    if (item == HeadAccessory::Crown)
        return character == CharacterId::Chicken || character == CharacterId::Dog ||
            character == CharacterId::Bear || character == CharacterId::Panda ||
            character == CharacterId::Korilakkuma;
    return false;
}

inline bool CanEquipBack(CharacterId character, BackAccessory item) {
    return IsValidCharacterId(static_cast<std::uint8_t>(character)) &&
        (item == BackAccessory::None || item == BackAccessory::Backpack);
}
