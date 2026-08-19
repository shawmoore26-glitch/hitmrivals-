// WORLD/Core/World.h
// The Phase 4.0 milestone itself: "a blank universe that can run a 2D
// fighter, a 3D RPG, or a simulation without changing the core engine."
// World owns an EntityRegistry (WORLD LAW 002) and a WorldTick (WORLD LAW
// 003) -- that's the whole kernel. It knows nothing about combat,
// characters, physics, or rendering; those are systems an extension
// registers from OUTSIDE this file (see tests/world/
// test_hitm_rivals_as_world_entity.cpp for COMBAT/CHARACTER doing exactly
// that without World.h ever including their headers).
#pragma once

#include "WORLD/Core/EntityRegistry.h"
#include "WORLD/Core/WorldHistory.h"
#include "WORLD/Core/WorldTick.h"

namespace dominus::world {

class World {
public:
    EntityRegistry& Entities() { return registry_; }
    const EntityRegistry& Entities() const { return registry_; }
    WorldTick& Systems() { return tick_; }
    WorldHistory& History() { return history_; }
    const WorldHistory& History() const { return history_; }

    void Tick(float dt) {
        tick_.Tick(registry_, dt);
        elapsedSeconds_ += dt;
    }

    float ElapsedSeconds() const { return elapsedSeconds_; }
    void SetElapsedSeconds(float seconds) { elapsedSeconds_ = seconds; }  // for restoring a loaded world's clock

private:
    EntityRegistry registry_;
    WorldTick tick_;
    WorldHistory history_;
    float elapsedSeconds_ = 0.0f;
};

}  // namespace dominus::world
