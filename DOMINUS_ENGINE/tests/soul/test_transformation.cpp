// tests/soul/test_transformation.cpp
#include "ANIMATION/AnimationGraph/MotionGraphEvaluator.h"
#include "CHARACTER/Rig/RigBinder.h"
#include "COMBAT/HitSystem/CombatBinder.h"
#include "COMBAT/HitSystem/CombatComponents.h"
#include "COMBAT/TransformationSystem.h"
#include "CORE/Serialization/DominusSerializer.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::character::MakeMotionGraphEvaluator;
using dominus::character::MotionGraphComponent;
using dominus::character::RigBinder;
using dominus::combat::CombatBinder;
using dominus::combat::CombatIdentityComponent;
using dominus::combat::MoveSetComponent;
using dominus::combat::SkeletonScaleComponent;
using dominus::combat::TransformationDef;
using dominus::combat::TransformationLoader;
using dominus::combat::TransformationSystem;
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
}  // namespace

DOMINUS_TEST(TransformationLoader_LoadsBeastModeFixture) {
    auto result = TransformationLoader::LoadFromFile(FixtureDir() / "brooklyn_beast_mode.transform.json");
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(result.value->name == "beast_mode");
    DOMINUS_EXPECT(result.value->moves.size() == 1);
    DOMINUS_EXPECT(result.value->moves[0].name == "beast_slam");
    DOMINUS_EXPECT(result.value->skeleton_scale == 1.5f);
}

DOMINUS_TEST(RigBinder_ResolvesTransformationRefListFromDotDominus) {
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    DOMINUS_EXPECT(loadResult.ok);
    auto* refs = loadResult.value->GetComponent<dominus::core::TransformationRefListComponent>();
    DOMINUS_EXPECT(refs != nullptr);
    DOMINUS_EXPECT(refs->transformations.size() == 1);
    DOMINUS_EXPECT(refs->transformations[0].name == "beast_mode");
}

DOMINUS_TEST(TransformationSystem_ApplySwapsCombatIdentityMovesAndMotionGraph) {
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    auto& obj = *loadResult.value;
    RigBinder::Bind(obj, fixtureDir);
    CombatBinder::Bind(obj, fixtureDir);

    // Before: base-form identity and moves.
    auto* identityBefore = obj.GetComponent<CombatIdentityComponent>();
    DOMINUS_EXPECT(identityBefore->identity.style == "psycho_drunken_martial_arts");
    auto* movesBefore = obj.GetComponent<MoveSetComponent>();
    DOMINUS_EXPECT(movesBefore->Find("beast_slam") == nullptr);
    DOMINUS_EXPECT(movesBefore->Find("jab") != nullptr);

    auto transformResult = TransformationLoader::LoadFromFile(fixtureDir / "brooklyn_beast_mode.transform.json");
    DOMINUS_EXPECT(transformResult.ok);
    auto applyResult = TransformationSystem::Apply(obj, *transformResult.value, fixtureDir);
    DOMINUS_EXPECT(applyResult.ok);

    // After: LAW C010 -- Combat Genome, Motion Graph, and Abilities all
    // changed as one coordinated swap, not independently.
    auto* identityAfter = obj.GetComponent<CombatIdentityComponent>();
    DOMINUS_EXPECT(identityAfter->identity.style == "beast_drunken_fury");
    DOMINUS_EXPECT(identityAfter->identity.pressure == "reckless");

    auto* movesAfter = obj.GetComponent<MoveSetComponent>();
    DOMINUS_EXPECT(movesAfter->Find("beast_slam") != nullptr);
    // "Abilities" changed means the OLD moveset is gone too -- a full swap,
    // not an additive merge (see TransformationSystem.h's LAW C010 note).
    DOMINUS_EXPECT(movesAfter->Find("jab") == nullptr);

    auto* scale = obj.GetComponent<SkeletonScaleComponent>();
    DOMINUS_EXPECT(scale != nullptr);
    DOMINUS_EXPECT(scale->scale == 1.5f);
}

DOMINUS_TEST(TransformationSystem_MotionGraphActuallyChangesBlendTiming) {
    // Proves the swapped motion graph is genuinely live, not just present
    // as inert data -- beast mode's idle->attack blend is 0.03s vs base
    // form's 0.1s.
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    auto& obj = *loadResult.value;
    RigBinder::Bind(obj, fixtureDir);
    CombatBinder::Bind(obj, fixtureDir);

    auto transformResult = TransformationLoader::LoadFromFile(fixtureDir / "brooklyn_beast_mode.transform.json");
    TransformationSystem::Apply(obj, *transformResult.value, fixtureDir);

    auto evaluator = MakeMotionGraphEvaluator(obj);
    DOMINUS_EXPECT(evaluator != nullptr);
    evaluator->Trigger("attack");
    DOMINUS_EXPECT(evaluator->IsTransitioning());

    // Beast mode's blend_duration is 0.03s -- 0.04s should already be past
    // it and committed to "attack", whereas base form's 0.1s blend would
    // still be mid-transition at this point.
    evaluator->Update(0.04f);
    DOMINUS_EXPECT(!evaluator->IsTransitioning());
    DOMINUS_EXPECT(evaluator->CurrentState() == "attack");
}

DOMINUS_TEST(TransformationSystem_FailedLoadLeavesObjectUnchanged) {
    // LAW C014: a failed transformation must not half-apply.
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    auto& obj = *loadResult.value;
    RigBinder::Bind(obj, fixtureDir);
    CombatBinder::Bind(obj, fixtureDir);

    TransformationDef broken;
    broken.name = "broken_form";
    broken.combat_dna_ref = "does_not_exist.json";
    broken.motion_graph_ref = "brooklyn_beast_motion_graph.json";

    auto applyResult = TransformationSystem::Apply(obj, broken, fixtureDir);
    DOMINUS_EXPECT(!applyResult.ok);

    // Original identity must still be intact.
    auto* identity = obj.GetComponent<CombatIdentityComponent>();
    DOMINUS_EXPECT(identity->identity.style == "psycho_drunken_martial_arts");
}
