#pragma once

#include <winsock2.h>
#include <cstdint>
#include <cstddef>
#include <array>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")

namespace Protocol
{
    constexpr uint8_t VERSION = 1;

    constexpr uint8_t TYPE_MOVE = 9;
    constexpr uint8_t TYPE_TEST_MOVE_RESULT = 240;

    constexpr std::size_t MOVE_PACKET_SIZE = 17;
    constexpr std::size_t RESULT_PACKET_SIZE = 25;

    constexpr uint32_t TEST_ROUND_ID = 1;

    enum class Direction : uint8_t
    {
        Up = 1,
        Down = 2,
        Left = 3,
        Right = 4
    };

    enum class MoveStatus : uint8_t
    {
        Accepted = 0,
        OutOfBounds = 1
    };

    using MovePacket =
        std::array<uint8_t, MOVE_PACKET_SIZE>;

    using ResultPacket =
        std::array<uint8_t, RESULT_PACKET_SIZE>;

    struct MoveResult
    {
        uint32_t inputSequence = 0;
        int32_t x = 0;
        int32_t z = 0;
        MoveStatus status = MoveStatus::Accepted;
    };

    inline void Put16(uint8_t* destination, uint16_t value)
    {
        value = htons(value);

        std::memcpy(
            destination,
            &value,
            sizeof(value)
        );
    }

    inline void Put32(uint8_t* destination, uint32_t value)
    {
        value = htonl(value);

        std::memcpy(
            destination,
            &value,
            sizeof(value)
        );
    }

    inline uint16_t Get16(const uint8_t* source)
    {
        uint16_t value;

        std::memcpy(
            &value,
            source,
            sizeof(value)
        );

        return ntohs(value);
    }

    inline uint32_t Get32(const uint8_t* source)
    {
        uint32_t value;

        std::memcpy(
            &value,
            source,
            sizeof(value)
        );

        return ntohl(value);
    }

    // 음수 좌표도 비트 형태를 유지해서 전송한다.
    inline void PutInt32(uint8_t* destination, int32_t value)
    {
        uint32_t bits;

        std::memcpy(
            &bits,
            &value,
            sizeof(bits)
        );

        Put32(destination, bits);
    }

    inline int32_t GetInt32(const uint8_t* source)
    {
        uint32_t bits = Get32(source);
        int32_t value;

        std::memcpy(
            &value,
            &bits,
            sizeof(value)
        );

        return value;
    }

    inline void PutHeader(
        uint8_t* destination,
        uint16_t size,
        uint8_t type,
        uint32_t packetSequence,
        uint32_t roundId)
    {
        Put16(destination, size);

        destination[2] = VERSION;
        destination[3] = type;

        Put32(destination + 4, packetSequence);
        Put32(destination + 8, roundId);
    }

    inline MovePacket MakeMovePacket(
        uint32_t packetSequence,
        uint32_t roundId,
        uint32_t inputSequence,
        Direction direction)
    {
        MovePacket packet{};

        PutHeader(
            packet.data(),
            static_cast<uint16_t>(packet.size()),
            TYPE_MOVE,
            packetSequence,
            roundId
        );

        Put32(packet.data() + 12, inputSequence);

        packet[16] =
            static_cast<uint8_t>(direction);

        return packet;
    }

    inline ResultPacket MakeMoveResult(
        uint32_t packetSequence,
        uint32_t roundId,
        const MoveResult& result)
    {
        ResultPacket packet{};

        PutHeader(
            packet.data(),
            static_cast<uint16_t>(packet.size()),
            TYPE_TEST_MOVE_RESULT,
            packetSequence,
            roundId
        );

        Put32(
            packet.data() + 12,
            result.inputSequence
        );

        PutInt32(packet.data() + 16, result.x);
        PutInt32(packet.data() + 20, result.z);

        packet[24] =
            static_cast<uint8_t>(result.status);

        return packet;
    }

    inline bool ReadMoveResult(
        const ResultPacket& packet,
        MoveResult& result)
    {
        if (Get16(packet.data()) != RESULT_PACKET_SIZE)
            return false;

        if (packet[2] != VERSION)
            return false;

        if (packet[3] != TYPE_TEST_MOVE_RESULT)
            return false;

        if (Get32(packet.data() + 8) != TEST_ROUND_ID)
            return false;

        if (packet[24] >
            static_cast<uint8_t>(MoveStatus::OutOfBounds))
            return false;

        result.inputSequence =
            Get32(packet.data() + 12);

        result.x = GetInt32(packet.data() + 16);
        result.z = GetInt32(packet.data() + 20);

        result.status =
            static_cast<MoveStatus>(packet[24]);

        return true;
    }
}