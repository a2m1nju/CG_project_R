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


// 모든 메시지는 기존 빅 엔디언 길이·버전·종류·순서 번호 헤더를 사용한다.
// MOVE(9)는 기존 17바이트 형식과 방향 번호를 유지한다.
namespace Wire {
constexpr std::uint8_t State = 10, WorldPosition = 11, Reset = 12;
// 공유 월드 메시지의 모든 정수 필드는 네트워크 바이트 순서를 사용한다.
constexpr std::uint8_t WorldLane = 20, WorldActor = 21, WorldBegin = 22, WorldEnd = 23;
constexpr std::uint8_t ItemBegin = 30, ItemEntry = 31, ItemEnd = 32,
    PickupRequest = 33, ItemRemoved = 34, ItemIdentity = 35;
constexpr std::size_t ItemBeginSize = 16, ItemEntrySize = 25, ItemEndSize = 12,
    PickupRequestSize = 16, ItemRemovedSize = 21, ItemIdentitySize = 12;
constexpr std::size_t WorldLaneSize = 33, WorldBeginSize = 24, WorldEndSize = 16;
constexpr std::size_t WorldActorSize = 49;
// WorldLane: 헤더, 라운드 ID, 월드 세대, 부호 있는 z, 지형, 나무·연잎·생성 위치 마스크.
// 마스크의 0번 비트는 x=-15, 30번 비트는 x=15를 뜻한다.
// WorldBegin: 헤더, 라운드 ID, 월드 세대, 초기 최소·최대 z.
// WorldEnd: 헤더, 라운드 ID, 월드 세대.
// WorldActor: 헤더, 라운드 ID, 월드 세대, 틱, 객체 ID, 종류, z/x/속도/너비,
// RGB 색상, 디자인, 기차 상태·타이머·경고등. 접속 시와 이후 주기적으로 전송한다.
constexpr std::size_t StateSize = 29, PositionSize = 21;
constexpr std::uint32_t TickRate = 30, MoveCooldownTicks = 6;
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
// IEEE-754 비트도 구조체 패딩 없이 네트워크 바이트 순서로 기록한다.
float ReadFloat(const std::uint8_t* p);
void WriteFloat(std::uint8_t* p, float v);
}
