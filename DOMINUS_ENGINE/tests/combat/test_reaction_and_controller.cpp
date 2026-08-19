// tests/combat/test_reaction_and_controller.cpp
// This is the LAW C012 proof: CombatController drives the SAME
// MotionGraphEvaluator from Phase 2.5 -- no separate animation system.
#include "CHARACTER/Rig/RigBinder.h"
#include "COMBAT/CombatController.h"
#include "COMBAT/HitSystem/CombatBinder.h"
#include "COMBAT/HitSystem/CombatComponents.h"
#include "COMBAT/ReactionSystem/ReactionSystem.h"
#include "CORE/Serialization/DominusSerializer.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::character::MakeMotionGraphEvaluator;
using dominus::character::RigBinder;
using dominus::combat::CombatBinder;
using dominus::combat::CombatController;
using dominus::combat::CombatPhase;
using dominus::combat::MoveSetComponent;
using dominus::combat::ReactionInput;
using dominus::combat::ReactionSystem;
using dominus::combat::ReactionType;
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

DOMINUS_TEST(ReactionSystem_LowPowerCausesStagger) {
    auto result = ReactionSystem::Determine(ReactionInput{.hit_power = 10.0f}, 1.0f);
    DOMINUS_EXPECT(result.type == ReactionType::kStagger);
    DOMINUS_EXPECT(result.motion_trigger == "stagger");
}

DOMINUS_TEST(ReactionSystem_HighPowerCausesKnockdown) {
    auto result = ReactionSystem::Determine(ReactionInput{.hit_power = 50.0f}, 1.0f);
    DOMINUS_EXPECT(result.type == ReactionType::kKnockdown);
    DOMINUS_EXPECT(result.force_y > 0.0f);
}

DOMINUS_TEST(ReactionSystem_BlockingProducesNoReactionButBlockstunTrigger) {
    auto result = ReactionSystem::Determine(ReactionInput{.hit_power = 50.0f, .defender_blocking = true}, 1.0f);
    DOMINUS_EXPECT(result.type == ReactionType::kNone);
    DOMINUS_EXPECT(result.motion_trigger == "block_impact");
}

DOMINUS_TEST(ReactionSystem_RepeatHitOnStaggeredTargetLaunches) {
    auto result =
        ReactionSystem::Determine(ReactionInput{.hit_power = 10.0f, .defender_already_staggered = true}, 1.0f);
    DOMINUS_EXPECT(result.type == ReactionType::kLaunch);
}

DOMINUS_TEST(ReactionSystem_ForceDirectionFollowsImpactSide) {
    auto rightResult = ReactionSystem::Determine(ReactionInput{.hit_power = 30.0f}, 1.0f);
    auto leftResult = ReactionSystem::Determine(ReactionInput{.hit_power = 30.0f}, -1.0f);
    DOMINUS_EXPECT(rightResult.force_x > 0.0f);
    DOMINUS_EXPECT(leftResult.force_x < 0.0f);
}

DOMINUS_TEST(CombatController_StartMoveSucceedsWhenMotionTriggerExists) {
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    DOMINUS_EXPECT(loadResult.ok);
    auto& obj = *loadResult.value;

    auto rigResult = RigBinder::Bind(obj, fixtureDir);
    DOMINUS_EXPECT(rigResult.ok);
    auto combatResult = CombatBinder::Bind(obj, fixtureDir);
    DOMINUS_EXPECT(combatResult.ok);

    auto evaluator = MakeMotionGraphEvaluator(obj);
    DOMINUS_EXPECT(evaluator != nullptr);
    auto* moveSet = obj.GetComponent<MoveSetComponent>();
    DOMINUS_EXPECT(moveSet != nullptr);

    CombatController controller(*evaluator, *moveSet);
    // "jab"'s motion_trigger is "attack", which DOES exist as a transition
    // in brooklyn_motion_graph.json -- this must succeed.
    bool started = controller.StartMove("jab");
    DOMINUS_EXPECT(started);
    DOMINUS_EXPECT(controller.Phase() == CombatPhase::kStartup);
    DOMINUS_EXPECT(controller.CurrentMove() != nullptr);
    DOMINUS_EXPECT(controller.CurrentMove()->name == "jab");
}

DOMINUS_TEST(CombatController_StartMoveFailsHonestlyWhenMotionTriggerMissing) {
    // LAW C014 (no fake systems): as of Phase 3.9, every one of Brooklyn's
    // real moves resolves in the motion graph (see
    // tests/integration/test_motion_library_completion.cpp for that
    // proof) -- so this test now constructs a synthetic move with a
    // trigger that deliberately doesn't exist, to keep proving the
    // REFUSAL BEHAVIOR itself generically, independent of any one
    // fixture's current content completeness.
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    auto& obj = *loadResult.value;
    RigBinder::Bind(obj, fixtureDir);
    CombatBinder::Bind(obj, fixtureDir);

    auto evaluator = MakeMotionGraphEvaluator(obj);
    auto* realMoveSet = obj.GetComponent<MoveSetComponent>();

    MoveSetComponent syntheticMoves = *realMoveSet;
    dominus::combat::MoveDef fake;
    fake.name = "phantom_move";
    fake.motion_trigger = "definitely_not_a_real_motion_state";
    syntheticMoves.moves.emplace("phantom_move", fake);

    CombatController controller(*evaluator, syntheticMoves);
    bool started = controller.StartMove("phantom_move");
    DOMINUS_EXPECT(!started);
    DOMINUS_EXPECT(controller.CurrentMove() == nullptr);
    DOMINUS_EXPECT(controller.Phase() == CombatPhase::kNeutral);
}

DOMINUS_TEST(CombatController_UpdateAdvancesPhaseThroughStartupActiveRecovery) {
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    auto& obj = *loadResult.value;
    RigBinder::Bind(obj, fixtureDir);
    CombatBinder::Bind(obj, fixtureDir);

    auto evaluator = MakeMotionGraphEvaluator(obj);
    auto* moveSet = obj.GetComponent<MoveSetComponent>();
    CombatController controller(*evaluator, *moveSet);

    controller.StartMove("jab");  // startup=5, active=4, recovery=15 frames @60fps
    DOMINUS_EXPECT(controller.Phase() == CombatPhase::kStartup);

    controller.Update(dominus::combat::FramesToSeconds(6));  // past startup(5) into active
    DOMINUS_EXPECT(controller.Phase() == CombatPhase::kActive);
    DOMINUS_EXPECT(controller.IsActiveFrame());

    controller.Update(dominus::combat::FramesToSeconds(4));  // past active(5+4=9) into recovery
    DOMINUS_EXPECT(controller.Phase() == CombatPhase::kRecovery);

    controller.Update(dominus::combat::FramesToSeconds(20));  // well past total (24 frames)
    DOMINUS_EXPECT(controller.Phase() == CombatPhase::kNeutral);
    DOMINUS_EXPECT(controller.CurrentMove() == nullptr);
}

DOMINUS_TEST(CombatController_CancelWindowAllowsComboStarterAfterJab) {
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    auto& obj = *loadResult.value;
    RigBinder::Bind(obj, fixtureDir);
    CombatBinder::Bind(obj, fixtureDir);

    auto evaluator = MakeMotionGraphEvaluator(obj);
    auto* moveSet = obj.GetComponent<MoveSetComponent>();
    CombatController controller(*evaluator, *moveSet);

    controller.StartMove("jab");
    controller.Update(dominus::combat::FramesToSeconds(10));  // inside jab's cancel window (5..19)

    // Phase 3.9: combo_starter's motion_trigger now resolves in the motion
    // graph (attack -> combo_starter), so the cancel legality AND the
    // actual motion request both succeed -- the completed combo chain,
    // proven positively rather than by refusal.
    DOMINUS_EXPECT(dominus::combat::ComboEngine::CanCancelInto(*controller.CurrentMove(), 10, "combo_starter"));
    bool cancelled = controller.TryCancelInto("combo_starter");
    DOMINUS_EXPECT(cancelled);
    DOMINUS_EXPECT(controller.CurrentMove()->name == "combo_starter");
}

DOMINUS_TEST(CombatController_ApplyHitInterruptsCurrentMoveAndSetsPhase) {
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    auto& obj = *loadResult.value;
    RigBinder::Bind(obj, fixtureDir);
    CombatBinder::Bind(obj, fixtureDir);

    auto evaluator = MakeMotionGraphEvaluator(obj);
    auto* moveSet = obj.GetComponent<MoveSetComponent>();
    CombatController controller(*evaluator, *moveSet);

    controller.StartMove("jab");
    DOMINUS_EXPECT(controller.CurrentMove() != nullptr);

    auto result = controller.ApplyHit(ReactionInput{.hit_power = 50.0f}, 1.0f);
    DOMINUS_EXPECT(result.type == ReactionType::kKnockdown);
    DOMINUS_EXPECT(controller.Phase() == CombatPhase::kKnockdown);
    DOMINUS_EXPECT(controller.CurrentMove() == nullptr);  // interrupted
}
