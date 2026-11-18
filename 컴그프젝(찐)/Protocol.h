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


// All messages use the big-endian length/version/type/sequence/roundId header.
// MOVE (9) remains the original 17-byte packet and Direction numbering.
namespace Wire {
// 12바이트 = 전체 길이 2 + 버전 1 + 종류 1 + 패킷 순번 4 + 경기 번호 4.
// 전체 길이 값에는 헤더와 본문이 모두 포함된다. 1바이트로는 255까지만 표현한다.
// 버전과 종류는 각각 0~255면 충분하다. 버전은 패킷 형식의 호환 여부를 검사하는 데 쓴다.
// 패킷 순번은 자주 증가하므로 32비트로 순환 주기를 늘리고, 경기 번호는 RoundId의 32비트 폭을 따른다.
// 현재 송수신 코드의 패킷 상한은 1024바이트다. 4096바이트는 LLD 상한이며, 큰 INIT/STATE는 조각 전송을 전제로 한다.
constexpr std::size_t HeaderSize = 12;
constexpr std::uint8_t QueueState = 3, ReadyLobbyState = 4;
constexpr std::uint8_t Ready = 5, LobbyUpdated = 1;
// 맵·아이템 본문은 minju 규약을 사용하고, 완료와 시작은 공통 헤더만 보낸다.
constexpr std::uint8_t Loaded = 7, Start = 8;
constexpr std::size_t ReadySize = HeaderSize + 5;
constexpr std::uint16_t ReadyBadPhase = 2, ReadyNotMember = 5, ReadyStale = 10;
constexpr std::uint8_t MatchingPhase = 0, LobbyEntered = 0;
constexpr std::size_t QueueStateSize = HeaderSize + 5;
constexpr std::size_t LobbyPrefixSize = HeaderSize + 6;
constexpr std::size_t LobbyMemberPrefixSize = 7;
constexpr std::uint8_t Join = 1, Welcome = 2, Error = 44; //WORLD_ACTOR(21)와 충돌하지 않도록 오류 응답 분리
constexpr std::size_t MaxNicknameBytes = 32;
constexpr std::size_t WelcomeSize = HeaderSize + 8, ErrorSize = HeaderSize + 7;
constexpr std::uint16_t JoinBusy = 1, JoinBadPayload = 4, JoinDuplicate = 11;
// 아래 번호는 현재 구현 기준이다. Reset은 전원 요청이 모이면 공유 라운드를 새로 로딩한다.
// 연결 ID는 서버가 연결에 부여하는 번호다. 별도 ID 메시지를 쓸 경우 서버가 클라이언트에 전달하며, SOCKET 핸들과 구분한다.
// 공유 월드의 WorldLane은 z 위치 하나의 지형·배치를 나타낸다. 여러 줄의 메시지를 한 번에 송신할 수 있다.
constexpr std::uint8_t State = 10, WorldPosition = 11, Reset = 12;
// 공유 월드 메시지의 모든 정수 필드는 네트워크 바이트 순서를 사용한다.
constexpr std::uint8_t WorldLane = 20, WorldActor = 21, WorldBegin = 22, WorldEnd = 23;
constexpr std::uint8_t ItemBegin = 30, ItemEntry = 31, ItemEnd = 32,
    PickupRequest = 33, ItemRemoved = 34, ItemIdentity = 35;
constexpr std::uint8_t ItemSwitch = 36, ItemUse = 37, InventoryState = 38;
constexpr std::size_t ItemSwitchSize = 13, ItemUseSize = 12, InventoryStateSize = 17;
constexpr std::uint8_t ItemEffect = 39;
constexpr std::size_t ItemEffectSize = 17;
constexpr std::uint8_t BananaPlaced = 40;
constexpr std::size_t BananaPlacedSize = 28;
constexpr std::uint8_t BananaHit = 41;
constexpr std::size_t BananaHitSize = 29;
constexpr std::uint8_t ItemSpawnRequest = 42, ItemSpawned = 43;
constexpr std::size_t ItemSpawnRequestSize = 13, ItemSpawnedSize = 25;
constexpr std::size_t ItemBeginSize = 16, ItemEntrySize = 25, ItemEndSize = 12,
    PickupRequestSize = 16, ItemRemovedSize = 21, ItemIdentitySize = 16;
constexpr std::size_t WorldLaneSize = 33, WorldBeginSize = 24, WorldEndSize = 16;
constexpr std::size_t WorldActorSize = 49;
// WorldLane: 헤더, 라운드 ID, 월드 세대, 부호 있는 z, 지형, 나무·연잎·생성 위치 마스크.
// 마스크의 0번 비트는 x=-15, 30번 비트는 x=15를 뜻한다.
// WorldBegin: 헤더, 라운드 ID, 월드 세대, 초기 최소·최대 z.
// WorldEnd: 헤더, 라운드 ID, 월드 세대.
// WorldActor: 헤더, 라운드 ID, 월드 세대, 틱, 객체 ID, 종류, z/x/속도/너비,
// RGB 색상, 디자인, 기차 상태·타이머·경고등. 접속 시와 이후 주기적으로 전송한다.
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
inline std::uint32_t ReadRoundId(const std::uint8_t* p) {
    return Read32(p + 8);
}
inline void Header(std::uint8_t* p, std::size_t size, std::uint8_t type,
                   std::uint32_t seq, std::uint32_t roundId) {
    p[0] = std::uint8_t(size >> 8); p[1] = std::uint8_t(size); // 0~1: 전체 길이, 상위 바이트부터
    p[2] = 1; p[3] = type; Write32(p + 4, seq); // 2: 현재 버전, 3: 종류, 4~7: 이 송신 방향의 패킷 순번
    Write32(p + 8, roundId); // 8~11: 경기 회차. 수신 측이 이전 라운드의 요청을 구별할 때 사용
}
// IEEE-754 비트도 구조체 패딩 없이 네트워크 바이트 순서로 기록한다.
float ReadFloat(const std::uint8_t* p);
void WriteFloat(std::uint8_t* p, float v);
}
