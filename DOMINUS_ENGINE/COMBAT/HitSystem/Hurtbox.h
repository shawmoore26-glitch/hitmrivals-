// COMBAT/HitSystem/Hurtbox.h
// LAW C006: hurtboxes are also bone-relative volumes, same shape as
// hitboxes -- a punch lands on a bone, not on "the enemy."
#pragma once

#include <string>
#include <vector>

namespace dominus::combat {

struct HurtboxDef {
    std::string bone;
    float radius = 5.0f;
    float offset_x = 0.0f;
    float offset_y = 0.0f;
};

struct HurtboxSet {
    std::vector<HurtboxDef> boxes;
};

}  // namespace dominus::combat
