// COMBAT/TransformationSystem.h
// LAW C010: a transformation is a full genome swap -- Skeleton, Animation,
// Combat Genome, Abilities, AI Behavior, Physics, Audio, Visual Identity.
// Phase 3.75 expands this from Phase 3.5's three-way swap (identity/graph/
// moves) to cover every layer: Combat Genome, Motion Graph, Abilities, AI
// Profile, Physics Profile, Audio Profile, Visual Profile, and Camera
// Profile all swap atomically. Skeleton is still scoped to a uniform scale
// modifier (SkeletonScaleComponent), not literal bone-topology change --
// AIProfile is the only added profile with a real downstream consumer
// (AI/Agents/CombatAI::ApplyProfile); Physics/Audio/Visual/Camera are
// loaded and attached for real but have no consuming system yet (no
// physics/audio/render/camera implementation exists in this engine).
// Flagged honestly in ROADMAP.md, not hidden.
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "COMBAT/Profiles.h"
#include "CORE/MetaBin/MetaBinObject.h"
#include "CORE/Serialization/DominusSerializer.h"  // for Result<T>/VoidResult

namespace dominus::combat {

struct NamedMoveRef {
    std::string name;
    std::string ref_path;
};

struct TransformationDef {
    std::string name;
    std::string combat_dna_ref;    // resolved relative to the object's baseDir
    std::string motion_graph_ref;
    std::vector<NamedMoveRef> moves;
    float skeleton_scale = 1.0f;

    // LAW C010 expanded (Phase 3.75 Priority 3): every gameplay layer, not
    // just genome/graph/moves. Each is optional -- a transformation that
    // doesn't specify a profile leaves the object's existing one
    // untouched, rather than resetting it to defaults. Inline in the
    // transformation JSON (not separate ref files) since these are small,
    // self-contained structs -- no separate loader needed per profile.
    std::optional<AIProfile> ai_profile;
    std::optional<PhysicsProfile> physics_profile;
    std::optional<AudioProfile> audio_profile;
    std::optional<VisualProfile> visual_profile;
    std::optional<CameraProfile> camera_profile;
};

// Attached to the object by TransformationSystem::Apply -- a uniform bind-
// pose scale multiplier a rendering/physics layer would read. Foundation
// scope: does NOT re-derive bone lengths for IK reach or hurtbox radii;
// systems that care about post-transform proportions need to read this
// and apply it themselves (flagged, not silently ignored).
struct SkeletonScaleComponent {
    float scale = 1.0f;
};

class TransformationLoader {
public:
    static core::Result<TransformationDef> LoadFromFile(const std::filesystem::path& path);
};

class TransformationSystem {
public:
    // Requires the object to have already been through RigBinder::Bind and
    // CombatBinder::Bind at least once (so the components being replaced
    // exist) -- Apply replaces CombatIdentityComponent, MotionGraphComponent,
    // and MoveSetComponent in place rather than requiring a full re-bind.
    static core::VoidResult Apply(core::MetaBinObject& obj, const TransformationDef& def,
                                   const std::filesystem::path& baseDir);
};

}  // namespace dominus::combat
