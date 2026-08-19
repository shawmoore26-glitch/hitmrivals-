// CORE/Runtime/EcsWorld.h
// Phase 1 scope: a thin registry of MetaBinObjects the frame loop knows
// about. This is deliberately NOT a full archetype/sparse-set ECS yet --
// that's a Phase 2 upgrade once GRAPHICS/ANIMATION/COMBAT systems exist to
// prove which query patterns actually matter. Building a "proper" ECS
// against zero real systems would violate Law 4 (scalability before
// spectacle): premature architecture for workloads that don't exist yet.
#pragma once

#include <string>
#include <unordered_map>

#include "CORE/MetaBin/MetaBinObject.h"

namespace dominus::core {

class EcsWorld {
public:
    MetaBinObject& AddObject(MetaBinObject obj) {
        std::string id = obj.Id();
        auto [it, inserted] = objects_.emplace(id, std::move(obj));
        return it->second;
    }

    MetaBinObject* FindObject(const std::string& id) {
        auto it = objects_.find(id);
        return it == objects_.end() ? nullptr : &it->second;
    }

    size_t ObjectCount() const { return objects_.size(); }

private:
    std::unordered_map<std::string, MetaBinObject> objects_;
};

}  // namespace dominus::core
