#include "Protocol.h"

#include <cstddef>

std::array<std::uint8_t, 17> EncodeMovePacket(
    std::uint32_t sequence,
    std::uint32_t roundId,
    std::uint32_t inputSeq,
    Direction physicalDirection)
{
    std::array<std::uint8_t, 17> packet{};

    // 32비트 값을 상위 바이트부터 기록
    const auto writeU32 = [&packet](std::size_t offset, std::uint32_t value) {
        packet[offset]     = static_cast<std::uint8_t>(value >> 24);
        packet[offset + 1] = static_cast<std::uint8_t>(value >> 16);
        packet[offset + 2] = static_cast<std::uint8_t>(value >> 8);
        packet[offset + 3] = static_cast<std::uint8_t>(value);
    };

    packet[0] = 0;
    packet[1] = static_cast<std::uint8_t>(packet.size()); // 전체 크기 17바이트
    packet[2] = 1; // 프로토콜 버전
    packet[3] = 9; // MOVE

    writeU32(4, sequence);  // 전체 송신 패킷의 순번
    writeU32(8, roundId);   // 현재 라운드 번호
    writeU32(12, inputSeq);  // 이동 요청의 순번
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
