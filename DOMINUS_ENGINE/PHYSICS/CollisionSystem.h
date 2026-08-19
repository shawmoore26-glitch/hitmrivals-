// PHYSICS/CollisionSystem.h
// Detects and resolves overlaps between any two entities carrying a
// Collider + WORLD::SpatialComponent. Foundation scope: circle-circle
// only (box-box and circle-box are flagged unresolved, not silently
// assumed away -- see ROADMAP.md Phase 4.1). Resolution is simple
// positional separation plus a velocity damping response, not a full
// impulse/restitution solver -- correct enough to prove the universal
// claim (entities push apart, physics doesn't know who they are), not a
// production-grade solver.
#pragma once

#include <cmath>
#include <optional>
#include <string>
#include <vector>

#include "PHYSICS/Collider.h"
#include "PHYSICS/RigidBody.h"
#include "WORLD/Core/EntityRegistry.h"
#include "WORLD/Core/SpatialComponent.h"
#include "WORLD/Core/WorldTick.h"

namespace dominus::physics {

struct CollisionPair {
    std::string entity_a;
    std::string entity_b;
    float overlap = 0.0f;
    float normal_x = 0.0f;  // points from a to b
    float normal_y = 0.0f;
};

class CollisionSystem {
public:
    // O(n^2) pairwise scan -- correct and simple, matching
    // COMBAT::CollisionEvaluator's own stated tradeoff. A spatial index
    // (grid/quadtree) is Phase 4.1+ follow-up once entity counts make
    // this the bottleneck, not invented speculatively.
    static std::vector<CollisionPair> Detect(world::EntityRegistry& registry) {
        std::vector<CollisionPair> pairs;
        auto entities = registry.WithComponent<Collider>();

        for (size_t i = 0; i < entities.size(); ++i) {
            for (size_t j = i + 1; j < entities.size(); ++j) {
                auto* a = entities[i];
                auto* b = entities[j];
                auto* colA = a->GetComponent<Collider>();
                auto* colB = b->GetComponent<Collider>();
                auto* spA = a->GetComponent<world::SpatialComponent>();
                auto* spB = b->GetComponent<world::SpatialComponent>();
                if (!colA || !colB || !spA || !spB) continue;
                if (!Collider::LayersInteract(*colA, *colB)) continue;

                if (colA->shape == ColliderShape::kCircle && colB->shape == ColliderShape::kCircle) {
                    if (auto pair = TestCircleCircle(a->Id(), *spA, *colA, b->Id(), *spB, *colB)) {
                        pairs.push_back(*pair);
                    }
                }
                // box-box / circle-box: not implemented this pass -- see
                // ROADMAP.md Phase 4.1 unresolved list.
            }
        }
        return pairs;
    }

    // Positional separation (proportional to inverse mass -- a static or
    // heavier body moves less) plus a simple velocity damping response.
    static void Resolve(world::EntityRegistry& registry, const std::vector<CollisionPair>& pairs) {
        for (const auto& pair : pairs) {
            auto* a = registry.Find(pair.entity_a);
            auto* b = registry.Find(pair.entity_b);
            if (!a || !b) continue;

            auto* spA = a->GetComponent<world::SpatialComponent>();
            auto* spB = b->GetComponent<world::SpatialComponent>();
            if (!spA || !spB) continue;

            auto* rbA = a->GetComponent<RigidBody>();
            auto* rbB = b->GetComponent<RigidBody>();
            bool aStatic = !rbA || rbA->is_static;
            bool bStatic = !rbB || rbB->is_static;
            if (aStatic && bStatic) continue;

            float pushA = bStatic ? 1.0f : (aStatic ? 0.0f : 0.5f);
            float pushB = 1.0f - pushA;
            if (!aStatic) {
                spA->x -= pair.normal_x * pair.overlap * pushA;
                spA->y -= pair.normal_y * pair.overlap * pushA;
            }
            if (!bStatic) {
                spB->x += pair.normal_x * pair.overlap * pushB;
                spB->y += pair.normal_y * pair.overlap * pushB;
            }

            if (rbA && !rbA->is_static) {
                rbA->velocity_x *= -0.3f;
                rbA->velocity_y *= -0.3f;
            }
            if (rbB && !rbB->is_static) {
                rbB->velocity_x *= -0.3f;
                rbB->velocity_y *= -0.3f;
            }
        }
    }

    static world::WorldSystemFn AsWorldSystem() {
        return [](world::EntityRegistry& registry, float) {
            auto pairs = Detect(registry);
            Resolve(registry, pairs);
        };
    }

private:
    static std::optional<CollisionPair> TestCircleCircle(const std::string& idA, const world::SpatialComponent& spA,
                                                           const Collider& colA, const std::string& idB,
                                                           const world::SpatialComponent& spB, const Collider& colB) {
        float dx = spB.x - spA.x;
        float dy = spB.y - spA.y;
        float distSq = dx * dx + dy * dy;
        float reach = colA.radius + colB.radius;
        if (distSq > reach * reach) return std::nullopt;

        float dist = std::sqrt(distSq);
        float nx = dist > 0.0001f ? dx / dist : 1.0f;
        float ny = dist > 0.0001f ? dy / dist : 0.0f;

        CollisionPair pair;
        pair.entity_a = idA;
        pair.entity_b = idB;
        pair.overlap = reach - dist;
        pair.normal_x = nx;
        pair.normal_y = ny;
        return pair;
    }
};

}  // namespace dominus::physics
