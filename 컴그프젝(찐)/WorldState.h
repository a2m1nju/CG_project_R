#pragma once

#include <cstdint>
#include <cstddef>
#include <map>
#include <vector>

struct WorldLaneData {
    std::int32_t z = 0;
    std::uint8_t terrain = 0; // 잔디, 도로, 강, 철로
    std::uint32_t treeMask = 0;
    std::uint32_t padMask = 0;
    std::uint32_t spawnMask = 0;
    std::vector<std::uint32_t> actorIds;
};

enum class WorldActorKind : std::uint8_t { Car = 1, Log = 2, Train = 3 };

struct WorldActorData {
    std::uint32_t id = 0;
    WorldActorKind kind = WorldActorKind::Car;
    float z = 0, x = 0, speed = 0, width = 0;
    std::uint8_t red = 0, green = 0, blue = 0, design = 0;
    std::uint8_t trainState = 0;
    std::uint16_t trainTimer = 0;
    bool trainLight = false;
};

// 네트워크 라운드마다 공유 월드를 하나 유지한다.
// 지형 생성은 요청 순서와 무관하므로 늦게 접속한 클라이언트도 같은 배치를 받는다.
class WorldState {
public:
    void Reset(std::uint32_t roundId);
    std::vector<WorldLaneData> EnsureRange(std::int32_t minZ, std::int32_t maxZ,
                                           std::size_t maxNewLanes);
    const WorldLaneData* FindLane(std::int32_t z) const;
    const WorldActorData* FindActor(std::uint32_t id) const;
    const std::map<std::uint32_t, WorldActorData>& Actors() const { return actors_; }
    void Step(std::uint64_t tick);
    const std::map<std::int32_t, WorldLaneData>& Lanes() const { return lanes_; }
    std::uint32_t RoundId() const { return roundId_; }
    std::uint32_t Generation() const { return generation_; }
    std::int32_t MinZ() const { return lanes_.begin()->first; }
    std::int32_t MaxZ() const { return lanes_.rbegin()->first; }

private:
    WorldLaneData MakeLane(std::int32_t z);
    std::map<std::int32_t, WorldLaneData> lanes_;
    std::map<std::uint32_t, WorldActorData> actors_;
    std::uint32_t nextActorId_ = 1;
    std::uint32_t roundId_ = 1;
    std::uint32_t generation_ = 0;
};
