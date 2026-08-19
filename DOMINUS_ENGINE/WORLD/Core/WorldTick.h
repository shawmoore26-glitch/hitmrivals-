// WORLD/Core/WorldTick.h
// WORLD LAW 003 — Simulation Before Rendering: the world must exist
// without graphics. WorldTick runs a named, ordered list of systems over
// the EntityRegistry each frame -- AI, physics, combat, whatever an
// extension registers -- with no rendering step anywhere in this file.
// A renderer is a system that would READ the world state afterward, not
// something WorldTick calls. This is what makes headless servers,
// simulations, and multiplayer authoritative-state possible: nothing
// here assumes a screen exists.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "WORLD/Core/EntityRegistry.h"

namespace dominus::world {

using WorldSystemFn = std::function<void(EntityRegistry&, float dt)>;

class WorldTick {
public:
    // Systems run in registration order -- deterministic, same reasoning
    // as ANIMATION's AnimationLayerStack and ProceduralHookStack: order is
    // part of the contract, not an implementation detail.
    void RegisterSystem(std::string name, WorldSystemFn fn) {
        systems_.push_back(SystemEntry{std::move(name), std::move(fn)});
    }

    void Tick(EntityRegistry& registry, float dt) {
        for (auto& system : systems_) {
            system.fn(registry, dt);
        }
    }

    size_t SystemCount() const { return systems_.size(); }

    std::vector<std::string> SystemNamesInOrder() const {
        std::vector<std::string> names;
        names.reserve(systems_.size());
        for (auto& s : systems_) names.push_back(s.name);
        return names;
    }

private:
    struct SystemEntry {
        std::string name;
        WorldSystemFn fn;
    };
    std::vector<SystemEntry> systems_;
};

}  // namespace dominus::world
