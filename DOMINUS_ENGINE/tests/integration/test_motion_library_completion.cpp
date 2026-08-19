// tests/integration/test_motion_library_completion.cpp
// Phase 3.9 proof: every move referenced by Brooklyn's combat genome now
// resolves to a real motion graph transition, verified by
// AssetValidation rather than asserted by hand -- and the specific gap
// this phase was created to close (counter) now genuinely completes.
#include "AI/Agents/CombatAI.h"
#include "CHARACTER/Genome/GenomeDecoder.h"
#include "CHARACTER/Rig/RigBinder.h"
#include "COMBAT/CombatController.h"
#include "COMBAT/HitSystem/AssetValidation.h"
#include "COMBAT/HitSystem/CombatBinder.h"
#include "CORE/Serialization/DominusSerializer.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::ai::CombatAI;
using dominus::ai::OpponentPatternTracker;
using dominus::character::GenomeDecoder;
using dominus::character::MakeMotionGraphEvaluator;
using dominus::character::MotionGraphComponent;
using dominus::character::RigBinder;
using dominus::combat::AssetValidation;
using dominus::combat::CombatBinder;
using dominus::combat::CombatController;
using dominus::combat::CombatIdentityComponent;
using dominus::combat::MoveSetComponent;
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

DOMINUS_TEST(AssetValidation_AllOfBrooklynsMovesResolveInTheMotionGraph) {
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    DOMINUS_EXPECT(loadResult.ok);
    auto& obj = *loadResult.value;
    RigBinder::Bind(obj, fixtureDir);
    CombatBinder::Bind(obj, fixtureDir);

    auto* moves = obj.GetComponent<MoveSetComponent>();
    auto* graphComp = obj.GetComponent<MotionGraphComponent>();
    DOMINUS_EXPECT(moves != nullptr && graphComp != nullptr);
    DOMINUS_EXPECT(moves->moves.size() == 6);  // jab, dodge, counter, combo_starter, launcher, air_combo

    auto report = AssetValidation::CheckMotionCoverage(*moves, graphComp->graph);
    DOMINUS_EXPECT(report.AllMovesResolve());
    DOMINUS_EXPECT(report.unreachable_moves.empty());
}

DOMINUS_TEST(AssetValidation_DetectsAnIntentionallyUnwiredMove) {
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    auto& obj = *loadResult.value;
    RigBinder::Bind(obj, fixtureDir);
    CombatBinder::Bind(obj, fixtureDir);

    auto* graphComp = obj.GetComponent<MotionGraphComponent>();
    MoveSetComponent movesWithGhost = *obj.GetComponent<MoveSetComponent>();
    dominus::combat::MoveDef ghost;
    ghost.name = "ghost_move";
    ghost.motion_trigger = "nonexistent_trigger";
    movesWithGhost.moves.emplace("ghost_move", ghost);

    auto report = AssetValidation::CheckMotionCoverage(movesWithGhost, graphComp->graph);
    DOMINUS_EXPECT(!report.AllMovesResolve());
    DOMINUS_EXPECT(report.unreachable_moves.size() == 1);
    DOMINUS_EXPECT(report.unreachable_moves[0] == "ghost_move");
}

DOMINUS_TEST(AssetValidation_ReportIsSortedForDeterminism) {
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    auto& obj = *loadResult.value;
    RigBinder::Bind(obj, fixtureDir);
    CombatBinder::Bind(obj, fixtureDir);

    auto* graphComp = obj.GetComponent<MotionGraphComponent>();
    MoveSetComponent movesWithGhosts = *obj.GetComponent<MoveSetComponent>();
    dominus::combat::MoveDef zeta, alpha;
    zeta.name = "zeta_ghost";
    zeta.motion_trigger = "missing_z";
    alpha.name = "alpha_ghost";
    alpha.motion_trigger = "missing_a";
    movesWithGhosts.moves.emplace("zeta_ghost", zeta);
    movesWithGhosts.moves.emplace("alpha_ghost", alpha);

    auto report = AssetValidation::CheckMotionCoverage(movesWithGhosts, graphComp->graph);
    DOMINUS_EXPECT(report.unreachable_moves.size() == 2);
    DOMINUS_EXPECT(report.unreachable_moves[0] == "alpha_ghost");  // sorted before zeta_ghost
    DOMINUS_EXPECT(report.unreachable_moves[1] == "zeta_ghost");
}

DOMINUS_TEST(Integration_CounterMoveNowFullyCompletesTheCombatLoop) {
    // The exact scenario from the Phase 3.9 request: an expert-counter
    // genome facing a repeated jab pattern decides to counter, and that
    // decision now actually reaches the skeleton runtime instead of being
    // honestly refused.
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    auto& obj = *loadResult.value;
    RigBinder::Bind(obj, fixtureDir);
    CombatBinder::Bind(obj, fixtureDir);

    auto* identity = obj.GetComponent<CombatIdentityComponent>();
    auto* moves = obj.GetComponent<MoveSetComponent>();
    auto weights = GenomeDecoder::Decode(identity->identity);

    OpponentPatternTracker tracker(8);
    tracker.RecordMove("jab");
    tracker.RecordMove("jab");
    tracker.RecordMove("jab");

    CombatAI ai(tracker, weights);
    std::string category = ai.Decide();
    DOMINUS_EXPECT(category == "counter");

    std::string chosen = ai.DecideMoveName(*moves, {"jab"});
    DOMINUS_EXPECT(chosen == "counter");

    auto evaluator = MakeMotionGraphEvaluator(obj);
    CombatController controller(*evaluator, *moves);
    bool started = controller.StartMove(chosen);
    DOMINUS_EXPECT(started);  // Phase 3-3.75: this was false. Phase 3.9: true.
    DOMINUS_EXPECT(controller.CurrentMove()->name == "counter");
    DOMINUS_EXPECT(controller.Phase() == dominus::combat::CombatPhase::kStartup);
}

DOMINUS_TEST(Integration_FullComboChainReachesAirCombo) {
    // jab -> combo_starter -> launcher -> air_combo, entirely through
    // cancel windows and real motion-graph transitions.
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    auto& obj = *loadResult.value;
    RigBinder::Bind(obj, fixtureDir);
    CombatBinder::Bind(obj, fixtureDir);

    auto* moves = obj.GetComponent<MoveSetComponent>();
    auto evaluator = MakeMotionGraphEvaluator(obj);
    CombatController controller(*evaluator, *moves);

    DOMINUS_EXPECT(controller.StartMove("jab"));
    controller.Update(dominus::combat::FramesToSeconds(10));  // inside jab's cancel window
    DOMINUS_EXPECT(controller.TryCancelInto("combo_starter"));

    controller.Update(dominus::combat::FramesToSeconds(8));  // inside combo_starter's cancel window (6..14)
    DOMINUS_EXPECT(controller.TryCancelInto("launcher"));
    DOMINUS_EXPECT(controller.CurrentMove()->name == "launcher");

    controller.Update(dominus::combat::FramesToSeconds(10));  // inside launcher's cancel window (7..13)
    DOMINUS_EXPECT(controller.TryCancelInto("air_combo"));
    DOMINUS_EXPECT(controller.CurrentMove()->name == "air_combo");
}

DOMINUS_TEST(Integration_KnockdownNowReachesRecoveryThenIdle) {
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    auto& obj = *loadResult.value;
    RigBinder::Bind(obj, fixtureDir);
    CombatBinder::Bind(obj, fixtureDir);

    auto* moves = obj.GetComponent<MoveSetComponent>();
    auto evaluator = MakeMotionGraphEvaluator(obj);
    CombatController controller(*evaluator, *moves);

    auto result = controller.ApplyHit(dominus::combat::ReactionInput{.hit_power = 50.0f}, 1.0f);
    DOMINUS_EXPECT(result.type == dominus::combat::ReactionType::kKnockdown);
    DOMINUS_EXPECT(controller.Phase() == dominus::combat::CombatPhase::kKnockdown);
    DOMINUS_EXPECT(evaluator->CurrentState() == "idle");  // still mid-blend into knockdown
    DOMINUS_EXPECT(evaluator->IsTransitioning());

    // knockdown (0.5s clip, no loop) -> auto to knockdown_recovery (0.4s) -> auto to idle.
    for (int i = 0; i < 40; ++i) evaluator->Update(0.05f);  // 2.0s, comfortably past both
    DOMINUS_EXPECT(evaluator->CurrentState() == "idle");
    DOMINUS_EXPECT(!evaluator->IsTransitioning());
}
