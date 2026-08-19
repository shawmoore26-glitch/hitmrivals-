// WORLD/Core/EntityRegistry.h
// WORLD LAW 002 — Everything Is An Entity: no special-cased Enemy/Tree/
// Building/QuestNPC types, just Entity + Components. That substrate
// already exists -- CORE::MetaBinObject IS an entity: a stable id plus a
// type-keyed component bag (see CORE/MetaBin/MetaBinObject.h). What was
// missing wasn't a new entity type, it was somewhere to put MANY of them
// together: EntityRegistry is that container. This file depends on CORE
// only -- never CHARACTER/COMBAT/ANIMATION -- because WORLD is the
// substrate everything else runs on, not the other way around.
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "CORE/MetaBin/MetaBinObject.h"

namespace dominus::world {

class EntityRegistry {
public:
    // Takes ownership of an already-constructed entity (typically one
    // loaded via CORE::DominusSerializer::Load elsewhere and handed in) --
    // EntityRegistry doesn't know how to build one from scratch, matching
    // WORLD LAW 002: entities are generic, but constructing a MEANINGFUL
    // one is still a domain concern (CHARACTER/COMBAT/whatever extension).
    core::MetaBinObject& CreateEntity(core::MetaBinObject entity) {
        std::string id = entity.Id();
        auto [it, inserted] = entities_.emplace(id, std::move(entity));
        return it->second;
    }

    core::MetaBinObject* Find(const std::string& id) {
        auto it = entities_.find(id);
        return it == entities_.end() ? nullptr : &it->second;
    }

    const core::MetaBinObject* Find(const std::string& id) const {
        auto it = entities_.find(id);
        return it == entities_.end() ? nullptr : &it->second;
    }

    bool Remove(const std::string& id) { return entities_.erase(id) > 0; }

    size_t Count() const { return entities_.size(); }

    std::vector<std::string> AllIds() const {
        std::vector<std::string> ids;
        ids.reserve(entities_.size());
        for (const auto& [id, entity] : entities_) ids.push_back(id);
        return ids;
    }

    // Every entity with a given component type -- the basic query shape
    // every system (AI, physics, combat, whatever) actually needs: "give
    // me everyone who has X". Foundation scope: linear scan, correct
    // and simple. A real archetype/sparse-set index is a Module 1
    // follow-up once profiling says this scan is the bottleneck at scale,
    // not invented speculatively.
    template <typename Component>
    std::vector<core::MetaBinObject*> WithComponent() {
        std::vector<core::MetaBinObject*> results;
        for (auto& [id, entity] : entities_) {
            if (entity.GetComponent<Component>() != nullptr) results.push_back(&entity);
        }
        return results;
    }

private:
    std::unordered_map<std::string, core::MetaBinObject> entities_;
};

}  // namespace dominus::world
