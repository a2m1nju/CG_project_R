#include "Protocol.h"

std::array<std::uint8_t, 17> EncodeMovePacket(
    std::uint32_t sequence,
    std::uint32_t roundId,
    std::uint32_t inputSeq,
    Direction physicalDirection)
{
    std::array<std::uint8_t, 17> packet{};

    Wire::Header(packet.data(), packet.size(), 9, sequence);
    Wire::Write32(packet.data() + 8, roundId);
    Wire::Write32(packet.data() + 12, inputSeq);
    packet[16] = static_cast<std::uint8_t>(physicalDirection);

    return packet;
}


#include <cstring>
float Wire::ReadFloat(const std::uint8_t* p) {
    const auto bits = Read32(p); float value;
    static_assert(sizeof(value) == sizeof(bits), "32-bit float required");
    std::memcpy(&value, &bits, sizeof(value)); return value;
}
void Wire::WriteFloat(std::uint8_t* p, float value) {
    std::uint32_t bits; std::memcpy(&bits, &value, sizeof(bits)); Write32(p, bits);
}
