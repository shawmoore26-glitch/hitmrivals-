// tests/combat/test_collision_impact_loop.cpp
// Closes the real gap: CollisionEvaluator was a pure query with no
// caller that ever turned an overlap into an applied impact.
// CombatController::EvaluateCollisionAndApplyImpact is that caller now.
// Called on the ATTACKER's controller, applying the reaction to a
// separate DEFENDER controller -- two genuinely different fighters,
// matching real combat (an attack does not interrupt itself). Six
// proofs, one per required guarantee:
//   1. valid collision produces exactly one impact
//   2. invalid input fails cleanly
//   3. impact result is deterministic
//   4. reaction is actually applied
//   5. repeated evaluation doesn't double-apply
//   6. serialized/deserialized impact remains equivalent
#include "ANIMATION/SkeletonSystem/SkeletonLoader.h"
#include "CHARACTER/Rig/RigBinder.h"
#include "COMBAT/CombatController.h"
#include "COMBAT/HitSystem/CombatBinder.h"
#include "COMBAT/HitSystem/HurtboxLoader.h"
#include "COMBAT/HitSystem/MoveLoader.h"
#include "COMBAT/Provenance/ImpactEventCompiler.h"
#include "COMBAT/Provenance/ImpactEventLog.h"
#include "COMBAT/ReactionSystem/ReactionSystem.h"
#include "CORE/Serialization/DominusSerializer.h"
#include "tests/TestFramework.h"

#include <filesystem>
#include <memory>

using dominus::animation::SkeletonLoader;
using dominus::character::MakeMotionGraphEvaluator;
using dominus::character::RigBinder;
using dominus::combat::CombatBinder;
using dominus::combat::CombatController;
using dominus::combat::CombatPhase;
using dominus::combat::FramesToSeconds;
using dominus::combat::HurtboxLoader;
using dominus::combat::ImpactEventCompiler;
using dominus::combat::ImpactEventLog;
using dominus::combat::ImpactGenomeInputs;
using dominus::combat::MoveLoader;
using dominus::combat::MoveSetComponent;
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

dominus::animation::Pose Translated(dominus::animation::Pose pose, float dx, float dy) {
    for (auto& t : pose) {
        t.x += dx;
        t.y += dy;
    }
    return pose;
}

// Owns everything a live fighter needs for the duration of a test --
// its own bound MetaBinObject, its own MotionGraphEvaluator, its own
// CombatController -- so two genuinely separate fighters (attacker and
// defender) can exist in the same test without one's state leaking
// into the other's.
struct Fighter {
    dominus::core::Result<dominus::core::MetaBinObject> loadResult;
    std::unique_ptr<dominus::animation::MotionGraphEvaluator> evaluator;
    std::unique_ptr<CombatController> controller;
};

std::unique_ptr<Fighter> LoadFighter(const std::filesystem::path& fixtureDir, const std::string& entityId) {
    auto f = std::make_unique<Fighter>();
    f->loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    if (!f->loadResult.ok) return f;
    auto& obj = *f->loadResult.value;
    RigBinder::Bind(obj, fixtureDir);
    CombatBinder::Bind(obj, fixtureDir);
    f->evaluator = MakeMotionGraphEvaluator(obj);
    auto* moveSet = obj.GetComponent<MoveSetComponent>();
    f->controller = std::make_unique<CombatController>(*f->evaluator, *moveSet);
    f->controller->SetEntityId(entityId);
    return f;
}
}  // namespace

// --- 1. Valid collision produces exactly one impact -------------------------

DOMINUS_TEST(EvaluateCollisionAndApplyImpact_ValidCollisionProducesExactlyOneImpact) {
    auto dir = FixtureDir();
    auto attacker = LoadFighter(dir, "attacker");
    auto defender = LoadFighter(dir, "defender");
    DOMINUS_EXPECT(attacker->loadResult.ok && defender->loadResult.ok);

    auto skel = SkeletonLoader::LoadFromFile(dir / "brooklyn.skel.json");
    auto hurtboxes = HurtboxLoader::LoadFromFile(dir / "brooklyn_hurtboxes.json");
    DOMINUS_EXPECT(skel.ok && hurtboxes.ok);

    auto attackerPose = skel.value->ComputeBindPoseWorld();
    // jab's hitbox is on arm_r at world ~(20, 50) -- place the defender
    // close enough that torso (radius 12) overlaps.
    auto defenderPose = Translated(skel.value->ComputeBindPoseWorld(), 20.0f, 10.0f);

    attacker->controller->StartMove("jab");
    attacker->controller->Update(FramesToSeconds(6));  // past startup(5) into active
    DOMINUS_EXPECT(attacker->controller->Phase() == CombatPhase::kActive);

    ImpactGenomeInputs attackerGenomes, defenderGenomes;
    auto reaction = attacker->controller->EvaluateCollisionAndApplyImpact(
        *defender->controller, *skel.value, attackerPose, *skel.value, defenderPose, *hurtboxes.value,
        attackerGenomes, defenderGenomes, /*attackerVelocity=*/5.0f);
    DOMINUS_EXPECT(reaction.has_value());
    DOMINUS_EXPECT(!reaction->motion_trigger.empty());
}

// --- 2. Invalid input fails cleanly ------------------------------------------

DOMINUS_TEST(EvaluateCollisionAndApplyImpact_NoCurrentMoveFailsCleanlyToNullopt) {
    auto dir = FixtureDir();
    auto attacker = LoadFighter(dir, "attacker");
    auto defender = LoadFighter(dir, "defender");  // StartMove never called on attacker

    auto skel = SkeletonLoader::LoadFromFile(dir / "brooklyn.skel.json");
    auto hurtboxes = HurtboxLoader::LoadFromFile(dir / "brooklyn_hurtboxes.json");
    auto pose = skel.value->ComputeBindPoseWorld();

    ImpactGenomeInputs a, d;
    auto reaction = attacker->controller->EvaluateCollisionAndApplyImpact(
        *defender->controller, *skel.value, pose, *skel.value, pose, *hurtboxes.value, a, d, 5.0f);
    DOMINUS_EXPECT(!reaction.has_value());  // no crash, no fabricated impact
}

DOMINUS_TEST(EvaluateCollisionAndApplyImpact_WrongPhaseFailsCleanlyToNullopt) {
    auto dir = FixtureDir();
    auto attacker = LoadFighter(dir, "attacker");
    auto defender = LoadFighter(dir, "defender");

    auto skel = SkeletonLoader::LoadFromFile(dir / "brooklyn.skel.json");
    auto hurtboxes = HurtboxLoader::LoadFromFile(dir / "brooklyn_hurtboxes.json");
    auto attackerPose = skel.value->ComputeBindPoseWorld();
    auto defenderPose = Translated(skel.value->ComputeBindPoseWorld(), 20.0f, 10.0f);

    attacker->controller->StartMove("jab");
    DOMINUS_EXPECT(attacker->controller->Phase() == CombatPhase::kStartup);  // still in startup, not active

    ImpactGenomeInputs a, d;
    auto reaction = attacker->controller->EvaluateCollisionAndApplyImpact(
        *defender->controller, *skel.value, attackerPose, *skel.value, defenderPose, *hurtboxes.value, a, d, 5.0f);
    DOMINUS_EXPECT(!reaction.has_value());  // hitboxes aren't live yet -- fails cleanly, doesn't fabricate a hit
}

DOMINUS_TEST(EvaluateCollisionAndApplyImpact_OpponentOutOfRangeFailsCleanlyToNullopt) {
    auto dir = FixtureDir();
    auto attacker = LoadFighter(dir, "attacker");
    auto defender = LoadFighter(dir, "defender");

    auto skel = SkeletonLoader::LoadFromFile(dir / "brooklyn.skel.json");
    auto hurtboxes = HurtboxLoader::LoadFromFile(dir / "brooklyn_hurtboxes.json");
    auto attackerPose = skel.value->ComputeBindPoseWorld();
    auto farDefenderPose = Translated(skel.value->ComputeBindPoseWorld(), 500.0f, 500.0f);  // nowhere close

    attacker->controller->StartMove("jab");
    attacker->controller->Update(FramesToSeconds(6));
    DOMINUS_EXPECT(attacker->controller->Phase() == CombatPhase::kActive);

    ImpactGenomeInputs a, d;
    auto reaction = attacker->controller->EvaluateCollisionAndApplyImpact(
        *defender->controller, *skel.value, attackerPose, *skel.value, farDefenderPose, *hurtboxes.value, a, d, 5.0f);
    DOMINUS_EXPECT(!reaction.has_value());  // genuinely no overlap -- correctly nothing, not a fabricated hit
}

// --- 3. Impact result is deterministic ---------------------------------------

DOMINUS_TEST(EvaluateCollisionAndApplyImpact_IsDeterministicAcrossTwoIndependentFightPairs) {
    auto dir = FixtureDir();
    auto skel = SkeletonLoader::LoadFromFile(dir / "brooklyn.skel.json");
    auto hurtboxes = HurtboxLoader::LoadFromFile(dir / "brooklyn_hurtboxes.json");
    auto attackerPose = skel.value->ComputeBindPoseWorld();
    auto defenderPose = Translated(skel.value->ComputeBindPoseWorld(), 20.0f, 10.0f);

    auto RunOnce = [&]() {
        auto dir2 = FixtureDir();
        auto attacker = LoadFighter(dir2, "attacker");
        auto defender = LoadFighter(dir2, "defender");
        attacker->controller->StartMove("jab");
        attacker->controller->Update(FramesToSeconds(6));
        ImpactGenomeInputs a, d;
        return attacker->controller->EvaluateCollisionAndApplyImpact(*defender->controller, *skel.value, attackerPose,
                                                                       *skel.value, defenderPose, *hurtboxes.value, a,
                                                                       d, 5.0f);
    };

    auto reactionA = RunOnce();
    auto reactionB = RunOnce();
    DOMINUS_EXPECT(reactionA.has_value());
    DOMINUS_EXPECT(reactionB.has_value());
    DOMINUS_EXPECT(reactionA->type == reactionB->type);
    DOMINUS_EXPECT(reactionA->motion_trigger == reactionB->motion_trigger);
    DOMINUS_EXPECT(reactionA->force_x == reactionB->force_x);
    DOMINUS_EXPECT(reactionA->force_y == reactionB->force_y);
}

// --- 4. Reaction is actually applied -----------------------------------------

DOMINUS_TEST(EvaluateCollisionAndApplyImpact_ReactionAppliesToDefenderNotAttacker) {
    auto dir = FixtureDir();
    auto attacker = LoadFighter(dir, "attacker");
    auto defender = LoadFighter(dir, "defender");

    auto skel = SkeletonLoader::LoadFromFile(dir / "brooklyn.skel.json");
    auto hurtboxes = HurtboxLoader::LoadFromFile(dir / "brooklyn_hurtboxes.json");
    auto attackerPose = skel.value->ComputeBindPoseWorld();
    auto defenderPose = Translated(skel.value->ComputeBindPoseWorld(), 20.0f, 10.0f);

    attacker->controller->StartMove("jab");
    attacker->controller->Update(FramesToSeconds(6));
    DOMINUS_EXPECT(attacker->controller->CurrentMove() != nullptr);
    DOMINUS_EXPECT(defender->controller->CurrentMove() == nullptr);  // defender was never attacking

    ImpactGenomeInputs a, d;
    auto reaction = attacker->controller->EvaluateCollisionAndApplyImpact(
        *defender->controller, *skel.value, attackerPose, *skel.value, defenderPose, *hurtboxes.value, a, d, 5.0f);
    DOMINUS_EXPECT(reaction.has_value());

    // The real, observable state change lands on the DEFENDER -- the
    // attacker's own swing keeps going, it does not self-interrupt.
    DOMINUS_EXPECT(attacker->controller->CurrentMove() != nullptr);
    DOMINUS_EXPECT(attacker->controller->Phase() == CombatPhase::kActive);
    DOMINUS_EXPECT(defender->controller->Phase() == CombatPhase::kHitstun ||
                   defender->controller->Phase() == CombatPhase::kKnockdown ||
                   defender->controller->Phase() == CombatPhase::kBlockstun);
}

// --- 5. Repeated evaluation doesn't double-apply -----------------------------

DOMINUS_TEST(EvaluateCollisionAndApplyImpact_RepeatedEvaluationInSameActivationDoesNotDoubleApply) {
    auto dir = FixtureDir();
    auto attacker = LoadFighter(dir, "attacker");
    auto defender = LoadFighter(dir, "defender");

    auto skel = SkeletonLoader::LoadFromFile(dir / "brooklyn.skel.json");
    auto hurtboxes = HurtboxLoader::LoadFromFile(dir / "brooklyn_hurtboxes.json");
    auto attackerPose = skel.value->ComputeBindPoseWorld();
    auto defenderPose = Translated(skel.value->ComputeBindPoseWorld(), 20.0f, 10.0f);

    attacker->controller->StartMove("jab");
    attacker->controller->Update(FramesToSeconds(6));  // into active

    ImpactGenomeInputs a, d;
    // Same defender stays overlapped across three consecutive frame
    // evaluations within the SAME activation -- exactly what a real
    // multi-frame active window looks like.
    auto first = attacker->controller->EvaluateCollisionAndApplyImpact(
        *defender->controller, *skel.value, attackerPose, *skel.value, defenderPose, *hurtboxes.value, a, d, 5.0f,
        "attacker", 1);
    auto second = attacker->controller->EvaluateCollisionAndApplyImpact(
        *defender->controller, *skel.value, attackerPose, *skel.value, defenderPose, *hurtboxes.value, a, d, 5.0f,
        "attacker", 2);
    auto third = attacker->controller->EvaluateCollisionAndApplyImpact(
        *defender->controller, *skel.value, attackerPose, *skel.value, defenderPose, *hurtboxes.value, a, d, 5.0f,
        "attacker", 3);

    DOMINUS_EXPECT(first.has_value());    // the one real impact
    DOMINUS_EXPECT(!second.has_value());  // same activation -- already resolved
    DOMINUS_EXPECT(!third.has_value());   // still the same activation
}

DOMINUS_TEST(EvaluateCollisionAndApplyImpact_ANewActivationCanHitAgain) {
    auto dir = FixtureDir();
    auto attacker = LoadFighter(dir, "attacker");
    auto defender = LoadFighter(dir, "defender");

    auto skel = SkeletonLoader::LoadFromFile(dir / "brooklyn.skel.json");
    auto hurtboxes = HurtboxLoader::LoadFromFile(dir / "brooklyn_hurtboxes.json");
    auto attackerPose = skel.value->ComputeBindPoseWorld();
    auto defenderPose = Translated(skel.value->ComputeBindPoseWorld(), 20.0f, 10.0f);
    ImpactGenomeInputs a, d;

    attacker->controller->StartMove("jab");
    attacker->controller->Update(FramesToSeconds(6));
    auto firstActivation = attacker->controller->EvaluateCollisionAndApplyImpact(
        *defender->controller, *skel.value, attackerPose, *skel.value, defenderPose, *hurtboxes.value, a, d, 5.0f);
    DOMINUS_EXPECT(firstActivation.has_value());

    // A brand new StartMove is a fresh activation -- resets the gate,
    // same "new move = new opportunity" rule ResetForNewActivation
    // documents. Brooklyn's real, authored motion graph only allows
    // "attack" from "idle" (there is no attack->attack self-transition
    // -- see brooklyn_motion_graph.json), so a real caller lets the
    // first move complete before starting the next one, same as every
    // other test in this codebase that advances through a full move.
    // The underlying attack CLIP is 0.3s/non-looping with a 0.15s blend
    // back to idle (brooklyn_attack.clip.json) -- MotionGraphEvaluator
    // only begins that auto-transition once clipTime_ passes 0.3s, and
    // only COMMITS it on a later Update() call once transitionElapsed_
    // passes the 0.15s blend -- one big Update() starts the transition,
    // a second one is required to finish it.
    attacker->controller->Update(FramesToSeconds(20));  // past clip duration -- transition begins
    attacker->controller->Update(FramesToSeconds(20));  // past blend_duration -- transition commits
    DOMINUS_EXPECT(attacker->controller->CurrentMove() == nullptr);

    bool started2 = attacker->controller->StartMove("jab");
    DOMINUS_EXPECT(started2);
    attacker->controller->Update(FramesToSeconds(6));
    auto secondActivation = attacker->controller->EvaluateCollisionAndApplyImpact(
        *defender->controller, *skel.value, attackerPose, *skel.value, defenderPose, *hurtboxes.value, a, d, 5.0f);
    DOMINUS_EXPECT(secondActivation.has_value());  // real, new hit -- not blocked by the old gate
}

// --- 6. Serialized/deserialized impact remains equivalent -------------------

DOMINUS_TEST(EvaluateCollisionAndApplyImpact_ProvenanceRoundTripsThroughTheRealCollisionPath) {
    auto dir = FixtureDir();
    auto attacker = LoadFighter(dir, "attacker_001");
    auto defender = LoadFighter(dir, "brooklyn");

    ImpactEventLog log;
    defender->controller->SetProvenanceLog(&log);  // the DEFENDER's controller records the hit against it

    auto skel = SkeletonLoader::LoadFromFile(dir / "brooklyn.skel.json");
    auto hurtboxes = HurtboxLoader::LoadFromFile(dir / "brooklyn_hurtboxes.json");
    auto attackerPose = skel.value->ComputeBindPoseWorld();
    auto defenderPose = Translated(skel.value->ComputeBindPoseWorld(), 20.0f, 10.0f);

    attacker->controller->StartMove("jab");
    attacker->controller->Update(FramesToSeconds(6));

    ImpactGenomeInputs a, d;
    auto reaction = attacker->controller->EvaluateCollisionAndApplyImpact(
        *defender->controller, *skel.value, attackerPose, *skel.value, defenderPose, *hurtboxes.value, a, d, 5.0f,
        "attacker_001", 999);
    DOMINUS_EXPECT(reaction.has_value());
    DOMINUS_EXPECT(log.Count() == 1);  // the collision genuinely produced a provenance record, not just a reaction

    const auto& captured = log.Events()[0];
    DOMINUS_EXPECT(captured.attacker == "attacker_001");
    DOMINUS_EXPECT(captured.target == "brooklyn");
    DOMINUS_EXPECT(captured.tick == 999);

    // Serialize -> reload -> replay: a brand-new log with zero shared
    // state, then rebuild the exact same context from the exact same
    // real collision inputs and confirm the hashes still agree.
    auto reloadedLog = ImpactEventLog::Deserialize(log.Serialize());
    DOMINUS_EXPECT(reloadedLog.Count() == 1);
    const auto& reloadedEvent = reloadedLog.Events()[0];
    DOMINUS_EXPECT(reloadedEvent.context_hash == captured.context_hash);
    DOMINUS_EXPECT(reloadedEvent.result_hash == captured.result_hash);

    // Real collision geometry here: attacker root at world x=0,
    // defender root at world x=20 -> dx=-20 -> impactDirX=+1.0
    // ("push defender away from attacker"); the only overlapping
    // hurtbox is torso (see EvaluateCollisionAndApplyImpact's own
    // geometry comment in the sibling tests above).
    auto replayCtx = dominus::combat::BuildImpactContext(a, d, 5.0f, "torso", 1.0f, false, false);
    auto replayResult = dominus::combat::ImpactSolver::Solve(replayCtx);
    DOMINUS_EXPECT(ImpactEventCompiler::VerifyMatches(reloadedEvent, replayCtx, replayResult));
}
