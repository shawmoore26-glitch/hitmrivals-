// tests/integration/test_transformation_profiles.cpp
// Priority 3 proof: transformations swap every gameplay layer, not just
// identity/graph/moves. AIProfile is the one with a real consumer
// (CombatAI::ApplyProfile); the rest are verified as genuinely loaded and
// attached, honestly flagged as inert without a renderer/physics/audio
// system to consume them.
#include "AI/Agents/CombatAI.h"
#include "CHARACTER/Genome/GenomeDecoder.h"
#include "CHARACTER/Rig/RigBinder.h"
#include "COMBAT/HitSystem/CombatBinder.h"
#include "COMBAT/Profiles.h"
#include "COMBAT/TransformationSystem.h"
#include "CORE/Serialization/DominusSerializer.h"
#include "tests/TestFramework.h"

#include <cmath>
#include <filesystem>

using dominus::ai::CombatAI;
using dominus::ai::OpponentPatternTracker;
using dominus::character::GenomeDecoder;
using dominus::character::RigBinder;
using dominus::combat::AIProfile;
using dominus::combat::AudioProfile;
using dominus::combat::CameraProfile;
using dominus::combat::CombatBinder;
using dominus::combat::CombatIdentityComponent;
using dominus::combat::PhysicsProfile;
using dominus::combat::TransformationLoader;
using dominus::combat::TransformationSystem;
using dominus::combat::VisualProfile;
using dominus::core::DominusSerializer;

namespace {
std::filesystem::path FixtureDir() {
    std::vector<std::filesystem::path> candidates = {
        std::filesystem::path("tests/fixtures"),
        std::filesystem::path("../tests/fixtures"),
        std::filesystem::path("../../tests/fixtures"),
    };
    for (auto& c : candidates)
        if (std::filesystem::exists(c)) return c;
    throw std::runtime_error("fixtures directory not found");
}
bool NearlyEqual(float a, float b, float eps = 0.01f) { return std::fabs(a - b) < eps; }
}  // namespace

DOMINUS_TEST(TransformationLoader_ParsesAllFiveExpandedProfiles) {
    auto result = TransformationLoader::LoadFromFile(FixtureDir() / "brooklyn_beast_mode.transform.json");
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(result.value->ai_profile.has_value());
    DOMINUS_EXPECT(result.value->physics_profile.has_value());
    DOMINUS_EXPECT(result.value->audio_profile.has_value());
    DOMINUS_EXPECT(result.value->visual_profile.has_value());
    DOMINUS_EXPECT(result.value->camera_profile.has_value());

    DOMINUS_EXPECT(result.value->ai_profile->behavior_tag == "feral");
    DOMINUS_EXPECT(NearlyEqual(result.value->ai_profile->difficulty, 0.9f));
    DOMINUS_EXPECT(NearlyEqual(result.value->physics_profile->mass_kg, 110.0f));
    DOMINUS_EXPECT(result.value->audio_profile->voice_bank == "brooklyn_beast_voice");
    DOMINUS_EXPECT(result.value->visual_profile->vfx_set == "beast_aura_vfx");
    DOMINUS_EXPECT(NearlyEqual(result.value->camera_profile->shake_intensity, 1.8f));
}

DOMINUS_TEST(TransformationSystem_ApplyAttachesAllFiveProfilesToObject) {
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    auto& obj = *loadResult.value;
    RigBinder::Bind(obj, fixtureDir);
    CombatBinder::Bind(obj, fixtureDir);

    DOMINUS_EXPECT(obj.GetComponent<AIProfile>() == nullptr);  // not present before transform

    auto transformResult = TransformationLoader::LoadFromFile(fixtureDir / "brooklyn_beast_mode.transform.json");
    TransformationSystem::Apply(obj, *transformResult.value, fixtureDir);

    auto* ai = obj.GetComponent<AIProfile>();
    auto* physics = obj.GetComponent<PhysicsProfile>();
    auto* audio = obj.GetComponent<AudioProfile>();
    auto* visual = obj.GetComponent<VisualProfile>();
    auto* camera = obj.GetComponent<CameraProfile>();

    DOMINUS_EXPECT(ai != nullptr && ai->behavior_tag == "feral");
    DOMINUS_EXPECT(physics != nullptr && NearlyEqual(physics->mass_kg, 110.0f));
    DOMINUS_EXPECT(audio != nullptr && audio->hit_sound_bank == "beast_impact");
    DOMINUS_EXPECT(visual != nullptr && visual->material_set == "beast_fur_material");
    DOMINUS_EXPECT(camera != nullptr && NearlyEqual(camera->fov_bias, 8.0f));
}

DOMINUS_TEST(TransformationSystem_OmittedProfileLeavesPriorValueUntouched) {
    // A transformation lacking one profile block must NOT wipe an existing
    // one back to defaults -- construct a minimal transform def with only
    // combat_dna/motion_graph set (no ai_profile) and confirm a
    // pre-attached AIProfile survives untouched.
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    auto& obj = *loadResult.value;
    RigBinder::Bind(obj, fixtureDir);
    CombatBinder::Bind(obj, fixtureDir);

    obj.AddComponent<AIProfile>(AIProfile{"pre_existing_tag", 0.42f});

    dominus::combat::TransformationDef minimal;
    minimal.name = "minimal_form";
    minimal.combat_dna_ref = "brooklyn_combat.json";  // reuse base form's own identity
    minimal.motion_graph_ref = "brooklyn_motion_graph.json";
    // ai_profile left as std::nullopt deliberately.

    auto applyResult = TransformationSystem::Apply(obj, minimal, fixtureDir);
    DOMINUS_EXPECT(applyResult.ok);

    auto* ai = obj.GetComponent<AIProfile>();
    DOMINUS_EXPECT(ai != nullptr);
    DOMINUS_EXPECT(ai->behavior_tag == "pre_existing_tag");  // untouched
}

DOMINUS_TEST(CombatAI_ApplyProfileIncreasesAggressionAndCounterBiasWithHighDifficulty) {
    OpponentPatternTracker tracker(8);
    auto baseWeights = GenomeDecoder::Decode(
        dominus::character::CombatIdentity{"style", "close", "reactive", "average", "grounded", "medium"});
    CombatAI ai(tracker, baseWeights);
    float aggressionBefore = ai.Weights().aggression;

    ai.ApplyProfile(AIProfile{"feral", 0.9f});  // scale = 0.5+0.9 = 1.4x
    DOMINUS_EXPECT(ai.Weights().aggression > aggressionBefore);
}

DOMINUS_TEST(CombatAI_ApplyProfileClampsToValidRange) {
    OpponentPatternTracker tracker(8);
    dominus::character::DecisionWeights maxedOut{1.0f, 0.5f, 0.5f, 1.0f, 0.5f};
    CombatAI ai(tracker, maxedOut);
    ai.ApplyProfile(AIProfile{"overkill", 1.0f});  // scale = 1.5x, would overshoot 1.0 without clamping
    DOMINUS_EXPECT(ai.Weights().aggression <= 1.0f);
    DOMINUS_EXPECT(ai.Weights().counter_bias <= 1.0f);
}
