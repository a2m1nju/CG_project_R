#include "WorldState.h"

#include <algorithm>

namespace {
std::uint32_t Mix(std::uint32_t value) {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    return value ^ (value >> 16);
}

std::uint32_t LaneRandom(std::uint32_t generation, std::int32_t z,
                         std::uint32_t salt) {
    return Mix(generation * 0x9e3779b9u ^
               static_cast<std::uint32_t>(z) * 0x85ebca6bu ^ salt);
}
}

void WorldState::Reset(std::uint32_t roundId) {
    roundId_ = roundId;
    ++generation_;
    lanes_.clear();
    actors_.clear();
    nextActorId_ = 1;
    EnsureRange(-30, 10, 41);
}

WorldLaneData WorldState::MakeLane(std::int32_t z) {
    WorldLaneData lane{};
    lane.z = z;
    if (z >= -2 && z <= 2) return lane;

    const auto roll = LaneRandom(generation_, z, 0) % 10;
    lane.terrain = roll < 5 ? 1 : roll < 7 ? 2 : roll < 8 ? 3 : 0;
    if (z == -3) lane.terrain = static_cast<std::uint8_t>(1 + generation_ % 3);

    for (int x = -15; x <= 15; ++x) {
        const auto bit = std::uint32_t{1} << (x + 15);
        const auto random = LaneRandom(generation_, z, static_cast<std::uint32_t>(x + 16));
        if (lane.terrain == 0) {
            if (random % 10 == 0) lane.treeMask |= bit;
            else if (x >= -6 && x <= 6 && (random >> 8) % 16 == 0)
                lane.spawnMask |= bit;
        } else if (lane.terrain == 2 && (x == 0 || random % 5 == 0)) {
            lane.padMask |= bit;
        }
    }
    const auto addActor = [&](WorldActorData actor) {
        actor.id = nextActorId_++;
        lane.actorIds.push_back(actor.id);
        actors_.emplace(actor.id, actor);
    };
    if (lane.terrain == 1) {
        const int count = 1 + LaneRandom(generation_, z, 101) % 2;
        for (int i = 0; i < count; ++i) {
            const auto random = LaneRandom(generation_, z, 110 + i);
            WorldActorData actor{};
            actor.kind = WorldActorKind::Car;
            actor.z = static_cast<float>(z);
            actor.x = static_cast<float>(int(random % 30) - 15 + i * (30 / count));
            if (actor.x > 18.0f) actor.x -= 36.0f;
            actor.speed = (0.05f + float((random >> 8) % 3) * 0.02f) *
                          ((random & 0x10000u) ? 1.0f : -1.0f);
            actor.red = static_cast<std::uint8_t>(50 + (random >> 16) % 206);
            actor.green = static_cast<std::uint8_t>(50 + (random >> 21) % 206);
            actor.blue = static_cast<std::uint8_t>(50 + (random >> 26) % 206);
            actor.design = static_cast<std::uint8_t>(random % 4);
            addActor(actor);
        }
    } else if (lane.terrain == 2) {
        const int count = 1 + LaneRandom(generation_, z, 201) % 2;
        for (int i = 0; i < count; ++i) {
            const auto random = LaneRandom(generation_, z, 210 + i);
            WorldActorData actor{};
            actor.kind = WorldActorKind::Log;
            actor.z = static_cast<float>(z);
            actor.x = static_cast<float>(int(random % 25) - 12 + i * 8);
            actor.speed = (0.03f + float((random >> 8) % 3) * 0.01f) *
                          ((random & 0x10000u) ? 1.0f : -1.0f);
            actor.width = 3.0f + float((random >> 12) % 3);
            addActor(actor);
        }
    } else if (lane.terrain == 3) {
        WorldActorData actor{};
        actor.kind = WorldActorKind::Train;
        actor.z = static_cast<float>(z);
        actor.x = -50.0f;
        actor.trainTimer = static_cast<std::uint16_t>(100 + LaneRandom(generation_, z, 301) % 200);
        addActor(actor);
    }
    return lane;
}

std::vector<WorldLaneData> WorldState::EnsureRange(std::int32_t minZ,
                                                   std::int32_t maxZ,
                                                   std::size_t maxNewLanes) {
    std::vector<WorldLaneData> added;
    if (minZ > maxZ || maxNewLanes == 0) return added;
    added.reserve(std::min<std::size_t>(maxNewLanes, 64));

    // 저장된 연속 구간을 한 줄씩 확장한다.
    // 클라이언트 위치가 크게 바뀌어도 틱당 생성량을 제한한다.
    if (lanes_.empty()) {
        for (auto z = minZ; z <= maxZ && added.size() < maxNewLanes; ++z) {
            auto lane = MakeLane(z);
            lanes_.emplace(z, lane);
            added.push_back(lane);
        }
        return added;
    }
    while (MinZ() > minZ && added.size() < maxNewLanes) {
        const auto z = MinZ() - 1;
        auto lane = MakeLane(z);
        lanes_.emplace(z, lane);
        added.push_back(lane);
    }
    while (MaxZ() < maxZ && added.size() < maxNewLanes) {
        const auto z = MaxZ() + 1;
        auto lane = MakeLane(z);
        lanes_.emplace(z, lane);
        added.push_back(lane);
    }
    return added;
}

const WorldLaneData* WorldState::FindLane(std::int32_t z) const {
    const auto it = lanes_.find(z);
    return it == lanes_.end() ? nullptr : &it->second;
}

const WorldActorData* WorldState::FindActor(std::uint32_t id) const {
    const auto it = actors_.find(id);
    return it == actors_.end() ? nullptr : &it->second;
}

void WorldState::Step(std::uint64_t tick) {
    for (auto& entry : actors_) {
        auto& actor = entry.second;
        if (actor.kind == WorldActorKind::Car) {
            actor.x += actor.speed * 2.0f;
            if (actor.speed > 0 && actor.x > 18.0f) actor.x = -18.0f;
            else if (actor.speed < 0 && actor.x < -18.0f) actor.x = 18.0f;
        } else if (actor.kind == WorldActorKind::Log) {
            actor.x += actor.speed * 2.0f;
            if (actor.speed > 0 && actor.x > 20.0f) actor.x = -20.0f;
            else if (actor.speed < 0 && actor.x < -20.0f) actor.x = 20.0f;
        } else if (actor.trainState == 0) {
            actor.trainTimer = actor.trainTimer > 2 ? actor.trainTimer - 2 : 0;
            if (actor.trainTimer == 0) { actor.trainState = 1; actor.trainTimer = 120; }
        } else if (actor.trainState == 1) {
            actor.trainTimer = actor.trainTimer > 2 ? actor.trainTimer - 2 : 0;
            actor.trainLight = (actor.trainTimer / 10) % 2 == 0;
            if (actor.trainTimer == 0) {
                actor.trainState = 2; actor.x = -60.0f; actor.speed = 3.0f;
            }
        } else {
            actor.x += actor.speed * 2.0f;
            if (actor.x > 60.0f) {
                actor.trainState = 0;
                actor.trainLight = false;
                actor.x = -50.0f;
                actor.speed = 0;
                actor.trainTimer = static_cast<std::uint16_t>(200 +
                    LaneRandom(generation_, static_cast<std::int32_t>(actor.z),
                               static_cast<std::uint32_t>(tick)) % 300);
            }
        }
    }
}
