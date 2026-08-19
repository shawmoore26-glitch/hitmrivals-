// COMBAT/HitSystem/CollisionEvaluator.h
// LAW C001: collision evaluation is a distinct stage between "motion
// request" and "physics result" -- it never runs off animation frame
// events, it runs off the current world-space Pose the skeleton runtime
// already computed. LAW C006: every hit is bone + impact location, never
// an abstract "hit enemy" flag.
#pragma once

#include <cmath>
#include <optional>
#include <string>
#include <vector>

#include "ANIMATION/ProceduralMotion/AnimationPlayer.h"  // for Pose
#include "ANIMATION/SkeletonSystem/Skeleton.h"
#include "COMBAT/HitSystem/Hurtbox.h"
#include "COMBAT/HitSystem/MoveDef.h"

namespace dominus::combat {

struct HitResult {
    std::string attacker_bone;
    std::string defender_bone;
    float impact_x = 0.0f;
    float impact_y = 0.0f;
};

class CollisionEvaluator {
public:
    // Tests every hitbox in `move` (positioned via `attackerPose` +
    // `attackerSkeleton`) against every box in `defenderHurtboxes`
    // (positioned via `defenderPose` + `defenderSkeleton`). Returns every
    // overlapping pair -- callers decide how many hits to actually apply
    // (e.g. one hit per active-frame window, not one per overlapping box
    // pair, which is a ReactionSystem/CombatController concern, not this
    // one's).
    static std::vector<HitResult> Evaluate(const animation::Skeleton& attackerSkeleton,
                                            const animation::Pose& attackerPose, const MoveDef& move,
                                            const animation::Skeleton& defenderSkeleton,
                                            const animation::Pose& defenderPose, const HurtboxSet& defenderHurtboxes) {
        std::vector<HitResult> hits;

        for (const HitboxDef& hb : move.hitboxes) {
            auto attackerBoneIdx = attackerSkeleton.FindBoneIndex(hb.bone);
            if (!attackerBoneIdx) continue;
            float hbWorldX = attackerPose[*attackerBoneIdx].x + hb.offset_x;
            float hbWorldY = attackerPose[*attackerBoneIdx].y + hb.offset_y;

            for (const HurtboxDef& hurtbox : defenderHurtboxes.boxes) {
                auto defenderBoneIdx = defenderSkeleton.FindBoneIndex(hurtbox.bone);
                if (!defenderBoneIdx) continue;
                float hurtWorldX = defenderPose[*defenderBoneIdx].x + hurtbox.offset_x;
                float hurtWorldY = defenderPose[*defenderBoneIdx].y + hurtbox.offset_y;

                float dx = hurtWorldX - hbWorldX;
                float dy = hurtWorldY - hbWorldY;
                float distSq = dx * dx + dy * dy;
                float reachSum = hb.radius + hurtbox.radius;

                if (distSq <= reachSum * reachSum) {
                    HitResult hit;
                    hit.attacker_bone = hb.bone;
                    hit.defender_bone = hurtbox.bone;
                    hit.impact_x = (hbWorldX + hurtWorldX) * 0.5f;
                    hit.impact_y = (hbWorldY + hurtWorldY) * 0.5f;
                    hits.push_back(hit);
                }
            }
        }
        return hits;
    }
};

}  // namespace dominus::combat
