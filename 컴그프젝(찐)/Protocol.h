#pragma once

#include <array>
#include <cstdint>

enum class Direction : std::uint8_t {
    UP = 0,
    DOWN = 1,
    LEFT = 2,
    RIGHT = 3
};

std::array<std::uint8_t, 17> EncodeMovePacket(
    std::uint32_t sequence,
    std::uint32_t roundId,
    std::uint32_t inputSeq,
    Direction physicalDirection);


// All messages use the existing big-endian length/version/type/sequence header.
// MOVE (9) remains the original 17-byte packet and Direction numbering.
namespace Wire {
constexpr std::uint8_t State = 10, WorldPosition = 11, Reset = 12;
constexpr std::size_t StateSize = 29, PositionSize = 21;
constexpr std::uint32_t TickRate = 30, MoveCooldownTicks = 3;
enum class MoveStatus : std::uint8_t { Idle, Accepted, Cooldown, Invalid, Stale };
inline std::uint32_t Read32(const std::uint8_t* p) {
    return (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) |
           (std::uint32_t(p[2]) << 8) | p[3];
}
inline void Write32(std::uint8_t* p, std::uint32_t v) {
    p[0] = std::uint8_t(v >> 24); p[1] = std::uint8_t(v >> 16);
    p[2] = std::uint8_t(v >> 8); p[3] = std::uint8_t(v);
}
inline bool Newer(std::uint32_t a, std::uint32_t b) {
    return a != b && std::uint32_t(a - b) < 0x80000000u;
}
inline void Header(std::uint8_t* p, std::size_t size, std::uint8_t type, std::uint32_t seq) {
    p[0] = std::uint8_t(size >> 8); p[1] = std::uint8_t(size);
    p[2] = 1; p[3] = type; Write32(p + 4, seq);
}
// IEEE-754 bits are also written in network byte order (no packed structs).
float ReadFloat(const std::uint8_t* p);
void WriteFloat(std::uint8_t* p, float v);
}
