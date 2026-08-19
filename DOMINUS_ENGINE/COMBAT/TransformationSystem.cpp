// COMBAT/TransformationSystem.cpp
#include "COMBAT/TransformationSystem.h"

#include <fstream>
#include <sstream>

#include "ANIMATION/AnimationGraph/MotionGraphLoader.h"
#include "CHARACTER/Genome/CombatIdentityLoader.h"
#include "CHARACTER/Rig/RigBinder.h"  // for MotionGraphComponent
#include "COMBAT/HitSystem/CombatComponents.h"
#include "COMBAT/HitSystem/MoveLoader.h"
#include "CORE/Serialization/MiniJson.h"

namespace dominus::combat {

using core::Result;
using core::VoidResult;
using core::json::Value;

namespace {
std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open file: " + path.string());
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// Sets a component to `value`, replacing it if it already exists (unlike
// MetaBinObject::AddComponent, which silently no-ops on an existing key --
// transformation semantics require an actual replace, per LAW C010: "a
// transformation is a full genome swap", not a merge).
template <typename Component>
void SetComponent(core::MetaBinObject& obj, Component value) {
    if (auto* existing = obj.GetComponent<Component>()) {
        *existing = std::move(value);
    } else {
        obj.AddComponent<Component>(std::move(value));
    }
}
}  // namespace

Result<TransformationDef> TransformationLoader::LoadFromFile(const std::filesystem::path& path) {
    std::string text;
    try {
        text = ReadFile(path);
    } catch (const std::exception& e) {
        return Result<TransformationDef>::Fail(e.what());
    }

    Value root;
    try {
        root = Value::Parse(text);
    } catch (const std::exception& e) {
        return Result<TransformationDef>::Fail(std::string("Parse error: ") + e.what());
    }

    if (!root.Has("name") || !root.Has("combat_dna_ref") || !root.Has("motion_graph_ref")) {
        return Result<TransformationDef>::Fail(
            "transformation file missing name/combat_dna_ref/motion_graph_ref: " + path.string());
    }

    TransformationDef def;
    def.name = root.Get("name")->AsString();
    def.combat_dna_ref = root.Get("combat_dna_ref")->AsString();
    def.motion_graph_ref = root.Get("motion_graph_ref")->AsString();
    if (auto* scale = root.Get("skeleton_scale")) {
        if (scale->IsNumber()) def.skeleton_scale = static_cast<float>(scale->AsNumber());
    }
    if (const Value* moves = root.Get("moves")) {
        if (moves->IsArray()) {
            for (const Value& entry : moves->AsArray()) {
                auto* name = entry.Get("name");
                auto* ref = entry.Get("ref");
                if (name && ref) def.moves.push_back(NamedMoveRef{name->AsString(), ref->AsString()});
            }
        }
    }

    auto strOr = [](const Value& obj, const char* key, const std::string& fallback) {
        auto* v = obj.Get(key);
        return v && v->IsString() ? v->AsString() : fallback;
    };
    auto numOr = [](const Value& obj, const char* key, float fallback) {
        auto* v = obj.Get(key);
        return v && v->IsNumber() ? static_cast<float>(v->AsNumber()) : fallback;
    };

    if (const Value* ai = root.Get("ai_profile")) {
        AIProfile p;
        p.behavior_tag = strOr(*ai, "behavior_tag", "");
        p.difficulty = numOr(*ai, "difficulty", 0.5f);
        def.ai_profile = p;
    }
    if (const Value* physics = root.Get("physics_profile")) {
        PhysicsProfile p;
        p.mass_kg = numOr(*physics, "mass_kg", 68.0f);
        p.gravity_scale = numOr(*physics, "gravity_scale", 1.0f);
        def.physics_profile = p;
    }
    if (const Value* audio = root.Get("audio_profile")) {
        AudioProfile p;
        p.voice_bank = strOr(*audio, "voice_bank", "");
        p.hit_sound_bank = strOr(*audio, "hit_sound_bank", "");
        def.audio_profile = p;
    }
    if (const Value* visual = root.Get("visual_profile")) {
        VisualProfile p;
        p.material_set = strOr(*visual, "material_set", "");
        p.vfx_set = strOr(*visual, "vfx_set", "");
        def.visual_profile = p;
    }
    if (const Value* camera = root.Get("camera_profile")) {
        CameraProfile p;
        p.fov_bias = numOr(*camera, "fov_bias", 0.0f);
        p.shake_intensity = numOr(*camera, "shake_intensity", 1.0f);
        def.camera_profile = p;
    }

    return Result<TransformationDef>::Ok(std::move(def));
}

VoidResult TransformationSystem::Apply(core::MetaBinObject& obj, const TransformationDef& def,
                                        const std::filesystem::path& baseDir) {
    auto identityResult = character::CombatIdentityLoader::LoadFromFile(baseDir / def.combat_dna_ref);
    if (!identityResult.ok) {
        return VoidResult::Fail("transformation '" + def.name + "' failed to load combat_dna: " + identityResult.error);
    }

    auto graphResult = animation::MotionGraphLoader::LoadFromFile(baseDir / def.motion_graph_ref);
    if (!graphResult.ok) {
        return VoidResult::Fail("transformation '" + def.name + "' failed to load motion_graph: " + graphResult.error);
    }

    MoveSetComponent newMoveSet;
    for (const auto& ref : def.moves) {
        auto moveResult = MoveLoader::LoadFromFile(baseDir / ref.ref_path);
        if (!moveResult.ok) {
            return VoidResult::Fail("transformation '" + def.name + "' failed to load move '" + ref.name +
                                     "': " + moveResult.error);
        }
        newMoveSet.moves.emplace(ref.name, std::move(*moveResult.value));
    }

    // All loads succeeded -- commit. Doing every load before any mutation
    // means a failed transformation leaves the object in its prior valid
    // state rather than half-swapped (LAW C014: no fake/partial systems).
    SetComponent<CombatIdentityComponent>(obj, CombatIdentityComponent{std::move(*identityResult.value)});
    SetComponent<character::MotionGraphComponent>(obj, character::MotionGraphComponent{std::move(*graphResult.value)});
    SetComponent<MoveSetComponent>(obj, std::move(newMoveSet));
    SetComponent<SkeletonScaleComponent>(obj, SkeletonScaleComponent{def.skeleton_scale});

    // LAW C010 expanded: each remaining layer only changes if this
    // transformation actually specifies it -- omitted profiles leave the
    // object's current values in place rather than resetting to defaults.
    if (def.ai_profile) SetComponent<AIProfile>(obj, *def.ai_profile);
    if (def.physics_profile) SetComponent<PhysicsProfile>(obj, *def.physics_profile);
    if (def.audio_profile) SetComponent<AudioProfile>(obj, *def.audio_profile);
    if (def.visual_profile) SetComponent<VisualProfile>(obj, *def.visual_profile);
    if (def.camera_profile) SetComponent<CameraProfile>(obj, *def.camera_profile);

    return VoidResult::Ok();
}

}  // namespace dominus::combat
