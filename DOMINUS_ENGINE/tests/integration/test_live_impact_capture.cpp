// tests/integration/test_live_impact_capture.cpp
// Phase 4.1.7: connects the existing real hit path --
//   CombatController -> ApplyImpact() -> ImpactEventCompiler ->
//   WorldHistory.RecordImpactEvent()
// -- so every real impact that already happens gets a provenance
// record, automatically. No new damage, no new reactions: this test
// proves the WIRING, using only mechanisms Phase 4.1.5/4.1.6 already
// built and tested independently.
//
// Acceptance criteria, verified as one continuous chain:
//   Spawn Brooklyn -> Attack -> Collision -> ImpactSolver -> Reaction
//   -> WorldHistory -> Replay verification
#include "CHARACTER/Rig/RigBinder.h"
#include "COMBAT/CombatController.h"
#include "COMBAT/HitSystem/CombatBinder.h"
#include "COMBAT/HitSystem/CombatComponents.h"
#include "COMBAT/PhysicsCombat/ImpactSolver.h"
#include "COMBAT/Provenance/ImpactEventCompiler.h"
#include "COMBAT/Provenance/ImpactEventLog.h"
#include "COMBAT/ReactionSystem/ReactionSystem.h"
#include "CORE/Serialization/DominusSerializer.h"
#include "WORLD/Core/WorldHistory.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::character::CombatPhysicsGenomeComponent;
using dominus::character::GameDesignGenomeComponent;
using dominus::character::MakeMotionGraphEvaluator;
using dominus::character::MaterialGenomeComponent;
using dominus::character::RigBinder;
using dominus::character::VisualStyleGenomeComponent;
using dominus::combat::BuildImpactContext;
using dominus::combat::CombatBinder;
using dominus::combat::CombatController;
using dominus::combat::CombatPhase;
using dominus::combat::ImpactEventCompiler;
using dominus::combat::ImpactEventLog;
using dominus::combat::ImpactGenomeInputs;
using dominus::combat::ImpactSolver;
using dominus::combat::MoveSetComponent;
using dominus::combat::ReactionType;
using dominus::core::DominusSerializer;
using dominus::world::WorldHistory;

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

DOMINUS_TEST(LiveImpactCapture_FullAcceptanceChain_SpawnAttackCollisionSolverReactionHistoryReplay) {
    auto fixtureDir = FixtureDir();

    // --- Spawn Brooklyn ------------------------------------------------
    auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    DOMINUS_EXPECT(loadResult.ok);
    auto& obj = *loadResult.value;
    auto rigResult = RigBinder::Bind(obj, fixtureDir);
    auto combatResult = CombatBinder::Bind(obj, fixtureDir);
    DOMINUS_EXPECT(rigResult.ok);
    DOMINUS_EXPECT(combatResult.ok);

    auto evaluator = MakeMotionGraphEvaluator(obj);
    auto* moveSet = obj.GetComponent<MoveSetComponent>();
    DOMINUS_EXPECT(evaluator != nullptr);
    DOMINUS_EXPECT(moveSet != nullptr);

    CombatController controller(*evaluator, *moveSet);
    controller.SetEntityId(obj.Id());
    DOMINUS_EXPECT(controller.EntityId() == "brooklyn");

    // Wire the real provenance seam -- this is the actual "connection"
    // this phase is about, not a hand-assembled event.
    ImpactEventLog provenanceLog;
    WorldHistory history;
    controller.SetProvenanceLog(&provenanceLog);
    controller.SetWorldHistory(&history);

    // --- Attack ----------------------------------------------------------
    bool started = controller.StartMove("jab");
    DOMINUS_EXPECT(started);
    DOMINUS_EXPECT(controller.Phase() == CombatPhase::kStartup);
    DOMINUS_EXPECT(provenanceLog.Count() == 0);  // starting a move alone is not an impact

    // --- Collision -> ImpactSolver -> Reaction, via the REAL,
    // genome-driven ApplyImpact path -- Brooklyn's own bound genomes are
    // the defender data, not synthetic stand-ins. ---
    ImpactGenomeInputs attackerInputs;  // a plain opponent, no genome refs of its own
    ImpactGenomeInputs defenderInputs;
    defenderInputs.physics = obj.GetComponent<CombatPhysicsGenomeComponent>();
    defenderInputs.material = obj.GetComponent<MaterialGenomeComponent>();
    defenderInputs.design = obj.GetComponent<GameDesignGenomeComponent>();
    defenderInputs.visualStyle = obj.GetComponent<VisualStyleGenomeComponent>();
    DOMINUS_EXPECT(defenderInputs.physics != nullptr);  // real, bound data -- not null

    const std::uint64_t tick = 48291;
    auto reaction = controller.ApplyImpact(attackerInputs, defenderInputs, /*attackerVelocity=*/8.0f, "head",
                                            /*impactDirX=*/1.0f, /*defenderBlocking=*/false,
                                            /*defenderAlreadyStaggered=*/false, /*attackerId=*/"attacker_001", tick);

    // Reaction: a real state change happened, driven by the real
    // ReactionSystem::Apply -- head strikes at these numbers land
    // knockdown-tier.
    DOMINUS_EXPECT(reaction.type == ReactionType::kKnockdown || reaction.type == ReactionType::kKnockback);
    DOMINUS_EXPECT(!reaction.motion_trigger.empty());
    DOMINUS_EXPECT(controller.CurrentMove() == nullptr);  // the attack was interrupted by the reaction

    // --- WorldHistory ----------------------------------------------------
    DOMINUS_EXPECT(provenanceLog.Count() == 1);
    DOMINUS_EXPECT(history.Count() == 1);

    const auto& capturedEvent = provenanceLog.Events()[0];
    DOMINUS_EXPECT(capturedEvent.event == "IMPACT");
    DOMINUS_EXPECT(capturedEvent.attacker == "attacker_001");
    DOMINUS_EXPECT(capturedEvent.target == "brooklyn");
    DOMINUS_EXPECT(capturedEvent.tick == tick);
    DOMINUS_EXPECT(capturedEvent.context_hash.size() == 64);
    DOMINUS_EXPECT(capturedEvent.result_hash.size() == 64);

    auto queryable = history.EventsForEntity("attacker_001");
    DOMINUS_EXPECT(queryable.size() == 1);
    DOMINUS_EXPECT(queryable[0].event_type == "IMPACT");

    // --- Replay verification ----------------------------------------------
    // A "process restart": serialize the captured log to a string,
    // reload it into a brand-new log with zero shared state, then
    // independently rebuild the impact context from the SAME recorded
    // inputs (genome refs + situational parameters) and solve it fresh.
    // If the hashes still agree, this is a real gameplay proof, not a
    // hand-assembled one.
    std::string serialized = provenanceLog.Serialize();
    auto reloadedLog = ImpactEventLog::Deserialize(serialized);
    DOMINUS_EXPECT(reloadedLog.Count() == 1);
    const auto& reloadedEvent = reloadedLog.Events()[0];
    DOMINUS_EXPECT(reloadedEvent.context_hash == capturedEvent.context_hash);
    DOMINUS_EXPECT(reloadedEvent.result_hash == capturedEvent.result_hash);

    auto replayCtx = BuildImpactContext(attackerInputs, defenderInputs, 8.0f, "head", 1.0f, false, false);
    auto replayResult = ImpactSolver::Solve(replayCtx);
    DOMINUS_EXPECT(ImpactEventCompiler::VerifyMatches(reloadedEvent, replayCtx, replayResult));
}

DOMINUS_TEST(LiveImpactCapture_MultipleRealImpactsEachGetTheirOwnProvenanceRecord) {
    // A short real "match": three separate impacts against the same
    // controller, each with a different tick -- proves capture doesn't
    // silently overwrite or merge events.
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    auto& obj = *loadResult.value;
    RigBinder::Bind(obj, fixtureDir);
    CombatBinder::Bind(obj, fixtureDir);

    auto evaluator = MakeMotionGraphEvaluator(obj);
    auto* moveSet = obj.GetComponent<MoveSetComponent>();
    CombatController controller(*evaluator, *moveSet);
    controller.SetEntityId("brooklyn");

    ImpactEventLog log;
    WorldHistory history;
    controller.SetProvenanceLog(&log);
    controller.SetWorldHistory(&history);

    ImpactGenomeInputs attacker;
    ImpactGenomeInputs defender;
    defender.physics = obj.GetComponent<CombatPhysicsGenomeComponent>();

    for (std::uint64_t tick = 1; tick <= 3; ++tick) {
        controller.StartMove("jab");
        controller.ApplyImpact(attacker, defender, 8.0f, "torso", 1.0f, false, false, "attacker_001", tick * 100);
    }

    DOMINUS_EXPECT(log.Count() == 3);
    DOMINUS_EXPECT(history.Count() == 3);
    for (std::uint64_t i = 0; i < 3; ++i) {
        DOMINUS_EXPECT(log.Events()[i].tick == (i + 1) * 100);
    }

    // Every one of them independently replays correctly after a full
    // serialize/reload cycle.
    auto reloaded = ImpactEventLog::Deserialize(log.Serialize());
    DOMINUS_EXPECT(reloaded.Count() == 3);
    auto replayCtx = BuildImpactContext(attacker, defender, 8.0f, "torso", 1.0f, false, false);
    auto replayResult = ImpactSolver::Solve(replayCtx);
    for (const auto& event : reloaded.Events()) {
        DOMINUS_EXPECT(ImpactEventCompiler::VerifyMatches(event, replayCtx, replayResult));
    }
}
