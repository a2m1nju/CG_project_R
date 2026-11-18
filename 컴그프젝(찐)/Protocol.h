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
// 아래 번호는 현재 구현 기준이다. Reset은 클라이언트 재시작 시 해당 세션의 라운드 상태를 초기화한다.
// 연결 ID는 서버가 연결에 부여하는 번호다. 별도 ID 메시지를 쓸 경우 서버가 클라이언트에 전달하며, SOCKET 핸들과 구분한다.
// 공유 월드의 WorldLane은 z 위치 하나의 지형·배치를 나타낸다. 여러 줄의 메시지를 한 번에 송신할 수 있다.
constexpr std::uint8_t State = 10, WorldPosition = 11, Reset = 12;
constexpr std::size_t StateSize = 29, PositionSize = 21;

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

// 서버가 클라이언트에게 자기 접속 ID를 알려주는 메시지
constexpr std::uint8_t AssignedId = 13;

// 공통 헤더 12바이트 + 플레이어 ID 4바이트
constexpr std::size_t AssignedIdSize = Wire::HeaderSize + 4;

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
inline std::uint32_t ReadRoundId(const std::uint8_t* p) {
    return Read32(p + 8);
}
inline void Header(std::uint8_t* p, std::size_t size, std::uint8_t type,
                   std::uint32_t seq, std::uint32_t roundId) {
    p[0] = std::uint8_t(size >> 8); p[1] = std::uint8_t(size); // 0~1: 전체 길이, 상위 바이트부터
    p[2] = 1; p[3] = type; Write32(p + 4, seq); // 2: 현재 버전, 3: 종류, 4~7: 이 송신 방향의 패킷 순번
    Write32(p + 8, roundId); // 8~11: 경기 회차. 수신 측이 이전 라운드의 요청을 구별할 때 사용
}
float ReadFloat(const std::uint8_t* p);
void WriteFloat(std::uint8_t* p, float v);
}

namespace LobbyWire {
    constexpr std::uint8_t Join = 1;
    constexpr std::uint8_t Welcome = 2;
    constexpr std::uint8_t QueueState = 3;
    constexpr std::uint8_t ReadyLobbyState = 4;
    constexpr std::uint8_t Ready = 5;
    constexpr std::uint8_t Leave = 17;
    constexpr std::uint8_t RoomClose = 18;
    constexpr std::uint8_t Error = 21;

    // 오류 코드라서 내일 머지할 때 바꿔야됨
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

        // 참가·준비 메시지는 경기 시작 전이므로 roundId = 0.
        Wire::Header(
            packet.data(), packet.size(),
            type, sequence, 0);

        return packet;
    }

    inline void Append32(
        std::vector<std::uint8_t>& packet,
        std::uint32_t value)
    {
        const std::size_t offset = packet.size();
        packet.resize(offset + 4);

        Wire::Write32(packet.data() + offset, value);
    }
}