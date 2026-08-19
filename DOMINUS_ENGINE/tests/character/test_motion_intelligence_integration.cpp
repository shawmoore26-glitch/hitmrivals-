// tests/character/test_motion_intelligence_integration.cpp
// This is the integration proof for the whole DOMINUS MOTION INTELLIGENCE
// SYSTEM: load Brooklyn from a single .dominus file, bind through
// CHARACTER/Rig, and drive a real MotionGraphEvaluator (state machine +
// blend transitions) entirely from data that came in through .dominus
// references -- no hardcoded graph, no hardcoded clip data, no bypassing
// RigBinder. If this file is green, "Maintain Meta-Bin architecture / all
// systems must load through .dominus references" is a met requirement, not
// a claim.
#include "CHARACTER/Rig/RigBinder.h"
#include "CORE/Serialization/DominusSerializer.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::character::MakeMotionGraphEvaluator;
using dominus::character::MotionGraphComponent;
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

DOMINUS_TEST(RigBinder_ResolvesMotionGraphRefFromDotDominus) {
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    DOMINUS_EXPECT(loadResult.ok);

    auto& obj = *loadResult.value;
    auto bindResult = dominus::character::RigBinder::Bind(obj, fixtureDir);
    DOMINUS_EXPECT(bindResult.ok);

    auto* motionGraph = obj.GetComponent<MotionGraphComponent>();
    DOMINUS_EXPECT(motionGraph != nullptr);
    DOMINUS_EXPECT(motionGraph->graph.entry_state == "idle");
    DOMINUS_EXPECT(motionGraph->graph.states.size() == 13);  // Phase 3.9: full move roster + reactions
}

DOMINUS_TEST(Integration_FullPipeline_DotDominusToLiveStateMachine) {
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    auto& obj = *loadResult.value;
    dominus::character::RigBinder::Bind(obj, fixtureDir);

    auto evaluator = MakeMotionGraphEvaluator(obj);
    DOMINUS_EXPECT(evaluator != nullptr);
    DOMINUS_EXPECT(evaluator->CurrentState() == "idle");

    evaluator->Update(0.2f);
    bool triggered = evaluator->Trigger("attack");
    DOMINUS_EXPECT(triggered);
    DOMINUS_EXPECT(evaluator->IsTransitioning());

    // Drive well past the blend duration and the attack clip's own
    // duration, plus its auto-transition back to idle.
    for (int i = 0; i < 30; ++i) evaluator->Update(0.05f);  // 1.5s total
    DOMINUS_EXPECT(evaluator->CurrentState() == "idle");
    DOMINUS_EXPECT(!evaluator->IsTransitioning());
}

DOMINUS_TEST(Integration_MissingMotionGraphReturnsNullEvaluatorNotCrash) {
    auto fixtureDir = FixtureDir();
    // ik_test_rig.dominus has a skeleton but no animations/motion_graph --
    // MakeMotionGraphEvaluator must fail gracefully, not throw or segfault.
    auto loadResult = DominusSerializer::Load(fixtureDir / "ik_test_rig.dominus");
    DOMINUS_EXPECT(loadResult.ok);
    auto& obj = *loadResult.value;
    dominus::character::RigBinder::Bind(obj, fixtureDir);

    auto evaluator = MakeMotionGraphEvaluator(obj);
    DOMINUS_EXPECT(evaluator == nullptr);
}
