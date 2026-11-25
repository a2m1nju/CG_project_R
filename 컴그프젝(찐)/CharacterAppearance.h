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
    Kuromi = 9,
    Sans = 10
};

constexpr int CharacterCount = static_cast<int>(CharacterId::Sans) + 1;

inline bool IsValidCharacterId(std::uint8_t id) {
    return id <= static_cast<std::uint8_t>(CharacterId::Sans);
}

inline CharacterId NormalizeCharacterId(std::uint8_t id) {
    return IsValidCharacterId(id) ? static_cast<CharacterId>(id) : CharacterId::Chicken;
}

enum class HeadAccessory : std::uint8_t {
    None = 0, Ribbon = 1, Crown = 2, Hat = 3, Halo = 4,
    Horns = 5, Flower = 6
};
enum class FaceAccessory : std::uint8_t { None = 0, Glasses = 1, Sunglasses = 2 };
enum class BackAccessory : std::uint8_t {
    None = 0, Backpack = 1, AngelWings = 2, DevilWings = 3
};

constexpr int HeadAccessoryCount = 7;
constexpr int FaceAccessoryCount = 3;
constexpr int BackAccessoryCount = 4;

struct AccessoryLoadout {
    HeadAccessory head = HeadAccessory::None;
    BackAccessory back = BackAccessory::None;
    FaceAccessory face = FaceAccessory::None;
};

inline bool CanEquipHead(CharacterId character, HeadAccessory item) {
    if (item == HeadAccessory::None) return true;
    if (item == HeadAccessory::Ribbon)
        return character == CharacterId::Rabbit || character == CharacterId::Cat;
    if (item == HeadAccessory::Crown)
        return character == CharacterId::Chicken || character == CharacterId::Dog ||
            character == CharacterId::Bear || character == CharacterId::Panda ||
            character == CharacterId::Korilakkuma;
    if (item == HeadAccessory::Hat)
        return character == CharacterId::Chicken || character == CharacterId::Dog ||
            character == CharacterId::Bear || character == CharacterId::Panda;
    if (item == HeadAccessory::Flower)
        return character == CharacterId::Rabbit || character == CharacterId::Cat ||
            character == CharacterId::Kuromi;
    if (item == HeadAccessory::Halo)
        return IsValidCharacterId(static_cast<std::uint8_t>(character));
    if (item == HeadAccessory::Horns)
        return character == CharacterId::Chicken || character == CharacterId::Dog ||
            character == CharacterId::Tino;
    return false;
}

inline bool CanEquipFace(CharacterId character, FaceAccessory item) {
    if (!IsValidCharacterId(static_cast<std::uint8_t>(character))) return false;
    if (item == FaceAccessory::None) return true;
    return character != CharacterId::Chicken &&
        (item == FaceAccessory::Glasses || item == FaceAccessory::Sunglasses);
}

inline bool CanEquipBack(CharacterId character, BackAccessory item) {
    return IsValidCharacterId(static_cast<std::uint8_t>(character)) &&
        (item == BackAccessory::None || item == BackAccessory::Backpack ||
         item == BackAccessory::AngelWings || item == BackAccessory::DevilWings);
}
