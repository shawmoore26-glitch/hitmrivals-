// CHARACTER/Rig/RigBinder.h
// The Phase 2 "CHARACTER composition layer" named in
// docs/ARCHITECTURE_v0.1.md section 2: takes a loaded MetaBinObject (which
// only carries generic SkeletonRefComponent / AnimationRefListComponent
// path refs from CORE) and resolves those refs into real ANIMATION-owned
// Skeleton/AnimationClip data, attaching them back onto the object as
// SkeletonComponent / AnimationSetComponent. This is what proves a
// .dominus object is not just valid JSON but an actually-loadable,
// actually-playable fighter -- the same contract hitm-character-forge
// validates at the design-doc level, made runtime-loadable here.
#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

#include "ANIMATION/AnimationGraph/MotionGraph.h"
#include "ANIMATION/AnimationGraph/MotionGraphEvaluator.h"
#include "ANIMATION/IK/IKChain.h"
#include "ANIMATION/Retargeting/RetargetMap.h"
#include "ANIMATION/SkeletonSystem/AnimationClip.h"
#include "ANIMATION/SkeletonSystem/Skeleton.h"
#include "CHARACTER/Genome/CombatPhysicsGenome.h"
#include "CHARACTER/Genome/CombatStyleGenome.h"
#include "CHARACTER/Genome/CreatureGenome.h"
#include "CHARACTER/Genome/GameDesignGenome.h"
#include "CHARACTER/Genome/MaterialGenome.h"
#include "CHARACTER/Genome/VisualGenome.h"
#include "CHARACTER/Genome/VisualStyleGenome.h"
#include "CHARACTER/Genome/SocialGenome.h"
#include "CORE/MetaBin/MetaBinObject.h"
#include "CORE/Serialization/DominusSerializer.h"  // for VoidResult

namespace dominus::character {

// Attached to a MetaBinObject once RigBinder::Bind succeeds.
struct SkeletonComponent {
    animation::Skeleton skeleton;
};

// Implements animation::IClipSource so a MotionGraphEvaluator can be built
// directly from this component without ANIMATION depending on CHARACTER.
struct AnimationSetComponent : public animation::IClipSource {
    std::unordered_map<std::string, animation::AnimationClip> clips;

    const animation::AnimationClip* Find(const std::string& name) const override {
        auto it = clips.find(name);
        return it == clips.end() ? nullptr : &it->second;
    }
};

struct IKChainSetComponent {
    std::unordered_map<std::string, animation::IKChainDef> chains;

    const animation::IKChainDef* Find(const std::string& name) const {
        auto it = chains.find(name);
        return it == chains.end() ? nullptr : &it->second;
    }
};

struct RetargetMapComponent {
    animation::RetargetMap map;
};

struct MotionGraphComponent {
    animation::MotionGraph graph;
};

// Bound from a SocialGenomeRefComponent. NOTE (flagged, not hidden):
// RigBinder::Bind currently requires a SkeletonRefComponent to proceed
// at all (see the "nothing to bind" failure at the top of Bind's
// implementation) -- an entity type that genuinely has no skeleton (a
// City, a Company, a Faction) cannot currently get its social genome
// resolved through this path, even though SocialGenome itself has no
// dependency on skeletal data whatsoever. Splitting binder
// responsibilities so non-organism entities can bind social/provenance
// data without a skeleton is real future work, not attempted here.
struct SocialGenomeComponent {
    SocialGenome genome;
};

// Bound from a CreatureGenomeRefComponent, same path and same flagged
// limitation as SocialGenomeComponent above (requires a
// SkeletonRefComponent to bind at all, even though CreatureGenome has
// no dependency on skeletal data either).
struct CreatureGenomeComponent {
    CreatureGenome genome;
};

// Bound from a VisualGenomeRefComponent, same path and same flagged
// limitation as SocialGenomeComponent/CreatureGenomeComponent above.
struct VisualGenomeComponent {
    VisualGenome genome;
};

// Bound from a CombatStyleGenomeRefComponent, same path and same flagged
// limitation as the components above. Note this does NOT replace
// CombatIdentity.style (still a plain string, LAW C002/C003 untouched)
// -- an entity can carry both: CombatIdentity.style names the style,
// this component is the fully resolved genome behind that name, when
// the .dominus file chooses to reference one.
struct CombatStyleGenomeComponent {
    CombatStyleGenome genome;
};

// Bound from a CombatPhysicsGenomeRefComponent, same path/limitation.
struct CombatPhysicsGenomeComponent {
    CombatPhysicsGenome genome;
};

// Bound from a GameDesignGenomeRefComponent, same path/limitation.
struct GameDesignGenomeComponent {
    GameDesignGenome genome;
};

// Bound from a MaterialGenomeRefComponent, same path/limitation.
struct MaterialGenomeComponent {
    MaterialGenome genome;
};

// Bound from a VisualStyleGenomeRefComponent, same path/limitation.
struct VisualStyleGenomeComponent {
    VisualStyleGenome genome;
};

// Convenience: builds a MotionGraphEvaluator directly from a MetaBinObject
// that has already been through RigBinder::Bind and has all three of
// SkeletonComponent / AnimationSetComponent / MotionGraphComponent. Returns
// nullptr if any are missing rather than throwing -- callers decide whether
// a missing motion graph is an error for their use case.
inline std::unique_ptr<animation::MotionGraphEvaluator> MakeMotionGraphEvaluator(core::MetaBinObject& obj) {
    auto* skeleton = obj.GetComponent<SkeletonComponent>();
    auto* animSet = obj.GetComponent<AnimationSetComponent>();
    auto* motionGraph = obj.GetComponent<MotionGraphComponent>();
    if (!skeleton || !animSet || !motionGraph) return nullptr;
    return std::make_unique<animation::MotionGraphEvaluator>(skeleton->skeleton, *animSet, motionGraph->graph);
}

class RigBinder {
public:
    // `baseDir` is the directory ref paths are resolved relative to --
    // matches the on-disk convention in docs/ARCHITECTURE_v0.1.md section 5
    // ("Every ref field is a relative path resolved against the object's
    // own directory").
    static core::VoidResult Bind(core::MetaBinObject& obj, const std::filesystem::path& baseDir);
};

}  // namespace dominus::character
