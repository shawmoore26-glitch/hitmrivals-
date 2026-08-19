// PHYSICS/ConstraintSolver.h
// Foundation scope: a single constraint type, distance (keep two
// entities a fixed distance apart -- the primitive a rope, a joint, or a
// leash is built from). Position-based, Jakobsen-style correction, not a
// full Lagrange multiplier solver -- correct and stable for the simple
// case, not a general-purpose constraint stack. Additional constraint
// types (hinge, spring, angle limits) are real future work, not invented
// speculatively here.
#pragma once

#include <cmath>
#include <string>
#include <vector>

#include "WORLD/Core/EntityRegistry.h"
#include "WORLD/Core/SpatialComponent.h"

namespace dominus::physics {

struct DistanceConstraint {
    std::string entity_a;
    std::string entity_b;
    float rest_length = 1.0f;
    float stiffness = 1.0f;  // 0 = no correction, 1 = full correction in one solve
};

class ConstraintSolver {
public:
    static void Solve(world::EntityRegistry& registry, const std::vector<DistanceConstraint>& constraints) {
        for (const auto& c : constraints) {
            auto* a = registry.Find(c.entity_a);
            auto* b = registry.Find(c.entity_b);
            if (!a || !b) continue;

            auto* spA = a->GetComponent<world::SpatialComponent>();
            auto* spB = b->GetComponent<world::SpatialComponent>();
            if (!spA || !spB) continue;

            float dx = spB->x - spA->x;
            float dy = spB->y - spA->y;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist < 0.0001f) continue;

            float diff = (dist - c.rest_length) / dist * 0.5f * c.stiffness;
            float correctionX = dx * diff;
            float correctionY = dy * diff;

            spA->x += correctionX;
            spA->y += correctionY;
            spB->x -= correctionX;
            spB->y -= correctionY;
        }
    }
};

}  // namespace dominus::physics
