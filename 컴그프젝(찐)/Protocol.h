#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include <string>

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

namespace Wire {
    constexpr std::size_t HeaderSize = 12;
    constexpr std::size_t PlayerStateRecordSize = 26;

    enum class DeathReason : std::uint8_t {
        None = 0,
        Chase = 1,
        Other = 2
    };

    // 기존 이동 메시지
    constexpr std::uint8_t State = 10;
    constexpr std::uint8_t WorldPosition = 11;
    constexpr std::uint8_t Reset = 12;
    constexpr std::uint8_t AssignedId = 13;

    constexpr std::size_t StateSize = 29;
    constexpr std::size_t PositionSize = 21;
    constexpr std::size_t AssignedIdSize = 16;

    // 네 공유 월드/시작/플레이어 메시지
    constexpr std::uint8_t WorldLane = 30;
    constexpr std::uint8_t WorldActor = 31;
    constexpr std::uint8_t WorldBegin = 32;
    constexpr std::uint8_t WorldEnd = 33;
    constexpr std::uint8_t Loaded = 34;
    constexpr std::uint8_t Start = 35;
    constexpr std::uint8_t PlayerStates = 36;

    constexpr std::size_t WorldLaneSize = 33;
    constexpr std::size_t WorldActorSize = 49;
    constexpr std::size_t WorldBeginSize = 24;
    constexpr std::size_t WorldEndSize = 16;

    // 친구 아이템 메시지: 겹치지 않는 번호 사용
    constexpr std::uint8_t ItemBegin = 50;
    constexpr std::uint8_t ItemEntry = 51;
    constexpr std::uint8_t ItemEnd = 52;
    constexpr std::uint8_t PickupRequest = 53;
    constexpr std::uint8_t ItemRemoved = 54;
    constexpr std::uint8_t ItemIdentity = 55;
    constexpr std::uint8_t ItemSwitch = 56;
    constexpr std::uint8_t ItemUse = 57;
    constexpr std::uint8_t InventoryState = 58;
    constexpr std::uint8_t ItemEffect = 59;
    constexpr std::uint8_t BananaPlaced = 60;
    constexpr std::uint8_t BananaHit = 61;
    constexpr std::uint8_t ItemSpawnRequest = 62;
    constexpr std::uint8_t ItemSpawned = 63;

    constexpr std::size_t ItemBeginSize = 16;
    constexpr std::size_t ItemEntrySize = 25;
    constexpr std::size_t ItemEndSize = 12;
    constexpr std::size_t PickupRequestSize = 16;
    constexpr std::size_t ItemRemovedSize = 21;
    constexpr std::size_t ItemIdentitySize = 16;
    constexpr std::size_t ItemSwitchSize = 13;
    constexpr std::size_t ItemUseSize = 12;
    constexpr std::size_t InventoryStateSize = 17;
    constexpr std::size_t ItemEffectSize = 17;
    constexpr std::size_t BananaPlacedSize = 28;
    constexpr std::size_t BananaHitSize = 29;
    constexpr std::size_t ItemSpawnRequestSize = 13;
    constexpr std::size_t ItemSpawnedSize = 25;

    constexpr std::uint32_t TickRate = 30;
    constexpr std::uint32_t MoveCooldownTicks = 3;

    enum class MoveStatus : std::uint8_t {
        Idle,
        Accepted,
        Cooldown,
        Invalid,
        Stale
    };

    inline std::uint32_t Read32(const std::uint8_t* p)
    {
        return (std::uint32_t(p[0]) << 24) |
            (std::uint32_t(p[1]) << 16) |
            (std::uint32_t(p[2]) << 8) |
            p[3];
    }

    inline void Write32(std::uint8_t* p, std::uint32_t value)
    {
        p[0] = std::uint8_t(value >> 24);
        p[1] = std::uint8_t(value >> 16);
        p[2] = std::uint8_t(value >> 8);
        p[3] = std::uint8_t(value);
    }

    inline bool Newer(std::uint32_t a, std::uint32_t b)
    {
        return a != b &&
            std::uint32_t(a - b) < 0x80000000u;
    }

    inline std::uint32_t ReadRoundId(const std::uint8_t* p)
    {
        return Read32(p + 8);
    }

    inline void Header(
        std::uint8_t* p,
        std::size_t size,
        std::uint8_t type,
        std::uint32_t sequence,
        std::uint32_t roundId)
    {
        p[0] = std::uint8_t(size >> 8);
        p[1] = std::uint8_t(size);
        p[2] = 1;
        p[3] = type;

        Write32(p + 4, sequence);
        Write32(p + 8, roundId);
    }

    float ReadFloat(const std::uint8_t* p);
    void WriteFloat(std::uint8_t* p, float value);
}

namespace LobbyWire {
    constexpr std::uint8_t Join = 1;
    constexpr std::uint8_t Welcome = 2;
    constexpr std::uint8_t QueueState = 3;
    constexpr std::uint8_t ReadyLobbyState = 4;
    constexpr std::uint8_t Ready = 5;
    constexpr std::uint8_t CharacterSelection = 6;
    constexpr std::uint8_t Leave = 17;
    constexpr std::uint8_t RoomClose = 18;
    constexpr std::uint8_t Error = 21;

    constexpr std::uint16_t Busy = 1;
    constexpr std::uint16_t BadPhase = 2;
    constexpr std::uint16_t Duplicate = 3;
    constexpr std::uint16_t Stale = 4;

    inline std::uint16_t Read16(const std::uint8_t* p)
    {
        return static_cast<std::uint16_t>(
            (std::uint16_t(p[0]) << 8) | p[1]);
    }

    inline void Write16(std::uint8_t* p, std::uint16_t value)
    {
        p[0] = static_cast<std::uint8_t>(value >> 8);
        p[1] = static_cast<std::uint8_t>(value);
    }

    inline std::vector<std::uint8_t> MakePacket(
        std::uint8_t type,
        std::size_t bodySize,
        std::uint32_t sequence = 0)
    {
        std::vector<std::uint8_t> packet(
            Wire::HeaderSize + bodySize, 0);

        Wire::Header(
            packet.data(), packet.size(),
            type, sequence, 0);

        return packet;
    }

    inline void Append32(
        std::vector<std::uint8_t>& packet,
        std::uint32_t value)
    {
        const auto offset = packet.size();
        packet.resize(offset + 4);
        Wire::Write32(packet.data() + offset, value);
    }
}
