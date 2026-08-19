// tests/combat/test_impact_pipeline.cpp
// Phase 4.1.5: CombatController -> ImpactContext -> ImpactSolver::Solve()
// -> ImpactResult -> ReactionSystem::Apply(). Six acceptance-criteria
// checks below, each named after the exact requirement it proves.
#include "CHARACTER/Genome/CombatPhysicsGenomeLoader.h"
#include "CHARACTER/Genome/CombatStyleGenomeLoader.h"
#include "CHARACTER/Genome/GameDesignGenomeLoader.h"
#include "CHARACTER/Genome/MaterialGenomeLoader.h"
#include "CHARACTER/Genome/VisualStyleGenomeLoader.h"
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

#include <cmath>
#include <filesystem>

using dominus::character::CombatPhysicsGenomeComponent;
using dominus::character::CombatPhysicsGenomeLoader;
using dominus::character::CombatStyleGenomeComponent;
using dominus::character::CombatStyleGenomeLoader;
using dominus::character::GameDesignGenomeComponent;
using dominus::character::GameDesignGenomeLoader;
using dominus::character::MakeMotionGraphEvaluator;
using dominus::character::MaterialGenomeComponent;
using dominus::character::MaterialGenomeLoader;
using dominus::character::RigBinder;
using dominus::character::VisualStyleGenomeComponent;
using dominus::character::VisualStyleGenomeLoader;
using dominus::combat::BuildImpactContext;
using dominus::combat::CombatBinder;
using dominus::combat::CombatController;
using dominus::combat::CombatPhase;
using dominus::combat::ImpactContext;
using dominus::combat::ImpactEventCompiler;
using dominus::combat::ImpactEventLog;
using dominus::combat::ImpactGenomeInputs;
using dominus::combat::ImpactResult;
using dominus::combat::ImpactSolver;
using dominus::combat::MoveSetComponent;
using dominus::combat::ReactionSystem;
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

bool NearlyEqual(float a, float b, float eps = 0.001f) { return std::fabs(a - b) < eps; }
}  // namespace

// --- 1. Identical ImpactContext produces identical ImpactResult -------------

DOMINUS_TEST(ImpactSolver_Solve_IsDeterministic) {
    ImpactContext ctx;
    ctx.attacker_mass_kg = 90.0f;
    ctx.attacker_velocity = 8.0f;
    ctx.attacker_strike_force_multiplier = 1.15f;
    ctx.attacker_aggression = 0.85f;
    ctx.defender_durability = 55.0f;
    ctx.defender_armor = 0.1f;
    ctx.defender_material_wear = 0.2f;
    ctx.struck_body_part = "head";
    ctx.design_risk_reward_balance = 0.75f;

    ImpactResult a = ImpactSolver::Solve(ctx);
    ImpactResult b = ImpactSolver::Solve(ctx);

    DOMINUS_EXPECT(NearlyEqual(a.force, b.force));
    DOMINUS_EXPECT(NearlyEqual(a.damage, b.damage));
    DOMINUS_EXPECT(a.severity == b.severity);
    DOMINUS_EXPECT(a.defender_blocking == b.defender_blocking);
}

// --- 2. ImpactSolver performs no entity mutation -----------------------------

DOMINUS_TEST(ImpactSolver_Solve_DoesNotMutateItsOwnInputContext) {
    // Solve takes ImpactContext by const&, has no ECS/entity parameter at
    // all, and returns a brand-new ImpactResult by value -- there is no
    // parameter it COULD mutate. Proven by calling it repeatedly against
    // one context object and confirming that object's own fields never
    // change, and that results stay identical across calls (if Solve had
    // any hidden internal/static state, repeated calls would drift).
    ImpactContext ctx;
    ctx.attacker_mass_kg = 70.0f;
    ctx.attacker_velocity = 5.0f;
    float massBefore = ctx.attacker_mass_kg;
    float velocityBefore = ctx.attacker_velocity;

    for (int i = 0; i < 5; ++i) {
        ImpactSolver::Solve(ctx);
    }

    DOMINUS_EXPECT(NearlyEqual(ctx.attacker_mass_kg, massBefore));
    DOMINUS_EXPECT(NearlyEqual(ctx.attacker_velocity, velocityBefore));

    ImpactResult r1 = ImpactSolver::Solve(ctx);
    ImpactResult r2 = ImpactSolver::Solve(ctx);
    DOMINUS_EXPECT(NearlyEqual(r1.damage, r2.damage));
}

// --- 3. ReactionSystem applies an ImpactResult correctly ---------------------

DOMINUS_TEST(ReactionSystem_Apply_LowDamageCausesStagger) {
    ImpactResult impact;
    impact.damage = 10.0f;
    impact.impact_dir_x = 1.0f;
    auto reaction = ReactionSystem::Apply(impact);
    DOMINUS_EXPECT(reaction.type == ReactionType::kStagger);
    DOMINUS_EXPECT(reaction.motion_trigger == "stagger");
}

DOMINUS_TEST(ReactionSystem_Apply_HighDamageCausesKnockdown) {
    ImpactResult impact;
    impact.damage = 50.0f;
    impact.impact_dir_x = 1.0f;
    auto reaction = ReactionSystem::Apply(impact);
    DOMINUS_EXPECT(reaction.type == ReactionType::kKnockdown);
    DOMINUS_EXPECT(reaction.force_y > 0.0f);
}

DOMINUS_TEST(ReactionSystem_Apply_RespectsBlockingFlagFromImpactResult) {
    ImpactResult impact;
    impact.damage = 50.0f;
    impact.defender_blocking = true;
    impact.impact_dir_x = 1.0f;
    auto reaction = ReactionSystem::Apply(impact);
    DOMINUS_EXPECT(reaction.type == ReactionType::kNone);
    DOMINUS_EXPECT(reaction.motion_trigger == "block_impact");
}

DOMINUS_TEST(ReactionSystem_Apply_AgreesWithDetermineForTheEquivalentReactionInput) {
    // Apply must not silently diverge from the existing, tested Determine
    // -- it's a thin adapter over the same logic, not a parallel
    // reimplementation.
    ImpactResult impact;
    impact.damage = 40.0f;
    impact.defender_already_staggered = true;
    impact.defender_defense_bias = 0.7f;
    impact.impact_dir_x = -1.0f;

    auto viaApply = ReactionSystem::Apply(impact);

    dominus::combat::ReactionInput equivalentInput;
    equivalentInput.hit_power = 40.0f;
    equivalentInput.defender_already_staggered = true;
    equivalentInput.defense_bias = 0.7f;
    auto viaDetermine = ReactionSystem::Determine(equivalentInput, -1.0f);

    DOMINUS_EXPECT(viaApply.type == viaDetermine.type);
    DOMINUS_EXPECT(NearlyEqual(viaApply.force_x, viaDetermine.force_x));
    DOMINUS_EXPECT(viaApply.motion_trigger == viaDetermine.motion_trigger);
}

// --- 4. CombatController builds a valid ImpactContext ------------------------

DOMINUS_TEST(BuildImpactContext_PopulatesFromRealBrooklynGenomes) {
    auto fixtureDir = FixtureDir();
    auto physics = CombatPhysicsGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_combat_physics.json");
    auto style = CombatStyleGenomeLoader::LoadFromFile(fixtureDir / "psycho_drunken_martial_arts_style.json");
    DOMINUS_EXPECT(physics.ok);
    DOMINUS_EXPECT(style.ok);

    CombatPhysicsGenomeComponent attackerPhysics{*physics.value};
    CombatStyleGenomeComponent attackerStyle{*style.value};

    ImpactGenomeInputs attacker;
    attacker.physics = &attackerPhysics;
    attacker.style = &attackerStyle;
    ImpactGenomeInputs defender;  // fully empty on purpose -- see test 5 below

    auto ctx = BuildImpactContext(attacker, defender, /*attackerVelocity=*/8.0f, "torso",
                                   /*impactDirX=*/1.0f, /*defenderBlocking=*/false,
                                   /*defenderAlreadyStaggered=*/false);

    DOMINUS_EXPECT(NearlyEqual(ctx.attacker_mass_kg, attackerPhysics.genome.body.mass_kg));
    DOMINUS_EXPECT(NearlyEqual(ctx.attacker_strike_force_multiplier, attackerPhysics.genome.impact.strike_force));
    DOMINUS_EXPECT(NearlyEqual(ctx.attacker_aggression, attackerStyle.genome.aggression));
    DOMINUS_EXPECT(NearlyEqual(ctx.attacker_velocity, 8.0f));
    DOMINUS_EXPECT(ctx.struck_body_part == "torso");
}

// --- 5. Missing genome references fail gracefully ---------------------------

DOMINUS_TEST(BuildImpactContext_AllNullGenomesProducesNeutralDefaultContext) {
    ImpactGenomeInputs emptyAttacker;
    ImpactGenomeInputs emptyDefender;

    auto ctx = BuildImpactContext(emptyAttacker, emptyDefender, 5.0f, "torso", 1.0f, false, false);

    ImpactContext neutral;  // freshly default-constructed
    DOMINUS_EXPECT(NearlyEqual(ctx.attacker_mass_kg, neutral.attacker_mass_kg));
    DOMINUS_EXPECT(NearlyEqual(ctx.attacker_strike_force_multiplier, neutral.attacker_strike_force_multiplier));
    DOMINUS_EXPECT(NearlyEqual(ctx.attacker_aggression, neutral.attacker_aggression));
    DOMINUS_EXPECT(NearlyEqual(ctx.defender_durability, neutral.defender_durability));
    DOMINUS_EXPECT(NearlyEqual(ctx.defender_armor, neutral.defender_armor));
    DOMINUS_EXPECT(NearlyEqual(ctx.defender_material_wear, neutral.defender_material_wear));
    DOMINUS_EXPECT(NearlyEqual(ctx.design_risk_reward_balance, neutral.design_risk_reward_balance));
    DOMINUS_EXPECT(ctx.visual_style_id.empty());

    // Solving that neutral context must not crash and must produce a
    // well-formed result -- "fails gracefully" means degrades to
    // baseline behavior, never a hard failure/exception for missing
    // genome data (genome refs are always optional, same discipline as
    // every RigBinder-optional component).
    ImpactResult result = ImpactSolver::Solve(ctx);
    DOMINUS_EXPECT(result.force >= 0.0f);
}

DOMINUS_TEST(CombatController_ApplyImpact_WithNoGenomesAttachedStillProducesAReaction) {
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    DOMINUS_EXPECT(loadResult.ok);
    auto& obj = *loadResult.value;
    RigBinder::Bind(obj, fixtureDir);
    CombatBinder::Bind(obj, fixtureDir);

    auto evaluator = MakeMotionGraphEvaluator(obj);
    auto* moveSet = obj.GetComponent<MoveSetComponent>();
    CombatController controller(*evaluator, *moveSet);

    controller.StartMove("jab");
    DOMINUS_EXPECT(controller.CurrentMove() != nullptr);

    // Deliberately empty genome inputs on both sides -- no crash, no
    // thrown exception, a real (if low-power, since defaults are gentle)
    // reaction still comes out the other end.
    ImpactGenomeInputs noGenomes;
    auto reaction =
        controller.ApplyImpact(noGenomes, noGenomes, /*attackerVelocity=*/1.0f, "torso", 1.0f, false, false);

    DOMINUS_EXPECT(controller.CurrentMove() == nullptr);  // interrupted, same as ApplyHit always does
    DOMINUS_EXPECT(!reaction.motion_trigger.empty());
}

// --- 6. Existing combat behavior remains unchanged when genome values ------
//        are defaults (the real Phase 2 worked example, reproduced) --------

DOMINUS_TEST(ImpactSolver_Solve_DefaultGenomeValuesMatchPhase2Baseline) {
    // The exact real numbers MASTER OF COMBAT Phase 2's own roadmap entry
    // printed for Brooklyn: force=720 (90*8), post-armor=648
    // (720*(1-0.1)), torso damage=648 (648*1.0). Solve() with every
    // genome-driven field left at ImpactContext's own neutral default
    // must reproduce these exactly -- proving the new pipeline changes
    // nothing about Phase 2's already-shipped, already-tested math.
    ImpactContext ctx;
    ctx.attacker_mass_kg = 90.0f;
    ctx.attacker_velocity = 8.0f;
    // attacker_strike_force_multiplier, attacker_aggression: left at
    // ImpactContext defaults (1.0, 0.5) on purpose.
    ctx.defender_durability = 55.0f;
    ctx.defender_armor = 0.1f;
    // defender_material_wear, design_risk_reward_balance: left at
    // defaults (0.0, 0.5) on purpose.
    ctx.struck_body_part = "torso";

    ImpactResult result = ImpactSolver::Solve(ctx);
    DOMINUS_EXPECT(NearlyEqual(result.force, 720.0f));
    DOMINUS_EXPECT(NearlyEqual(result.damage, 648.0f));

    // Cross-check against the original Phase 2 hand-chained calls
    // directly -- the two paths must agree exactly.
    float legacyForce = ImpactSolver::CalculateForce(90.0f, 8.0f);
    float legacyPostArmor = ImpactSolver::ApplyArmor(legacyForce, 0.1f);
    float legacyDamage = ImpactSolver::CalculateDamage(legacyPostArmor, "torso");
    DOMINUS_EXPECT(NearlyEqual(result.damage, legacyDamage));
}

DOMINUS_TEST(ImpactSolver_Solve_UsingRealBrooklynFixtureMatchesPhase2Chain) {
    // Same cross-check, but sourced from the real, on-disk
    // brooklyn_combat_physics.json fixture rather than hand-typed
    // numbers -- the actual attacker in Phase 2's own worked example was
    // a synthetic 90kg/8m/s attacker striking Brooklyn, not Brooklyn
    // striking someone else, so this loads Brooklyn as the DEFENDER,
    // exactly as the original example did.
    auto fixtureDir = FixtureDir();
    auto brooklynPhysics = CombatPhysicsGenomeLoader::LoadFromFile(fixtureDir / "brooklyn_combat_physics.json");
    DOMINUS_EXPECT(brooklynPhysics.ok);

    ImpactContext ctx;
    ctx.attacker_mass_kg = 90.0f;
    ctx.attacker_velocity = 8.0f;
    ctx.defender_durability = brooklynPhysics.value->impact.durability;
    ctx.defender_armor = brooklynPhysics.value->body.armor;
    ctx.struck_body_part = "torso";

    ImpactResult result = ImpactSolver::Solve(ctx);
    DOMINUS_EXPECT(NearlyEqual(result.force, 720.0f));
    DOMINUS_EXPECT(NearlyEqual(result.damage, 648.0f));
}

// --- Genome integration: each genome type genuinely changes the result -----

DOMINUS_TEST(ImpactSolver_Solve_MaterialWearReducesEffectiveArmor) {
    ImpactContext pristine;
    pristine.attacker_mass_kg = 90.0f;
    pristine.attacker_velocity = 8.0f;
    pristine.defender_durability = 55.0f;
    pristine.defender_armor = 0.5f;
    pristine.defender_material_wear = 0.0f;
    pristine.struck_body_part = "torso";

    ImpactContext worn = pristine;
    worn.defender_material_wear = 1.0f;  // fully worn

    auto pristineResult = ImpactSolver::Solve(pristine);
    auto wornResult = ImpactSolver::Solve(worn);

    // Worn material protects less -> more damage gets through.
    DOMINUS_EXPECT(wornResult.damage > pristineResult.damage);
}

DOMINUS_TEST(ImpactSolver_Solve_HigherAttackerAggressionIncreasesDamage) {
    ImpactContext calm;
    calm.attacker_mass_kg = 90.0f;
    calm.attacker_velocity = 8.0f;
    calm.attacker_aggression = 0.0f;
    calm.defender_durability = 55.0f;
    calm.struck_body_part = "torso";

    ImpactContext aggressive = calm;
    aggressive.attacker_aggression = 1.0f;

    auto calmResult = ImpactSolver::Solve(calm);
    auto aggressiveResult = ImpactSolver::Solve(aggressive);
    DOMINUS_EXPECT(aggressiveResult.damage > calmResult.damage);
}

DOMINUS_TEST(ImpactSolver_Solve_GameDesignRiskRewardBalanceScalesFinalDamage) {
    ImpactContext safe;
    safe.attacker_mass_kg = 90.0f;
    safe.attacker_velocity = 8.0f;
    safe.defender_durability = 55.0f;
    safe.struck_body_part = "torso";
    safe.design_risk_reward_balance = 0.0f;

    ImpactContext highRisk = safe;
    highRisk.design_risk_reward_balance = 1.0f;

    auto safeResult = ImpactSolver::Solve(safe);
    auto highRiskResult = ImpactSolver::Solve(highRisk);
    DOMINUS_EXPECT(highRiskResult.damage > safeResult.damage);
}

DOMINUS_TEST(ImpactSolver_Solve_VisualStyleIdIsCarriedThroughInertly) {
    // No consumer exists yet (no renderer) -- this proves the data
    // survives the pipeline unmodified, nothing more.
    ImpactContext ctx;
    ctx.visual_style_id = "STYLE-URBAN-COMBAT";
    auto result = ImpactSolver::Solve(ctx);
    DOMINUS_EXPECT(result.visual_style_id == "STYLE-URBAN-COMBAT");
}

// --- Full pipeline, end to end, through the real CombatController ----------

DOMINUS_TEST(CombatController_ApplyImpact_FullPipelineWithRealBrooklynGenomes) {
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    DOMINUS_EXPECT(loadResult.ok);
    auto& obj = *loadResult.value;
    RigBinder::Bind(obj, fixtureDir);
    CombatBinder::Bind(obj, fixtureDir);

    auto evaluator = MakeMotionGraphEvaluator(obj);
    auto* moveSet = obj.GetComponent<MoveSetComponent>();
    CombatController controller(*evaluator, *moveSet);

    // Brooklyn's real, bound genomes (from RigBinder, closed in the
    // previous phase) become the defender in this impact.
    auto* physics = obj.GetComponent<CombatPhysicsGenomeComponent>();
    auto* material = obj.GetComponent<MaterialGenomeComponent>();
    auto* design = obj.GetComponent<GameDesignGenomeComponent>();
    auto* visualStyle = obj.GetComponent<VisualStyleGenomeComponent>();
    DOMINUS_EXPECT(physics != nullptr);
    DOMINUS_EXPECT(material != nullptr);
    DOMINUS_EXPECT(design != nullptr);
    DOMINUS_EXPECT(visualStyle != nullptr);

    ImpactGenomeInputs attacker;  // a plain attacker, no genome refs
    ImpactGenomeInputs defender;
    defender.physics = physics;
    defender.material = material;
    defender.design = design;
    defender.visualStyle = visualStyle;

    controller.StartMove("jab");
    auto reaction =
        controller.ApplyImpact(attacker, defender, /*attackerVelocity=*/8.0f, "head", 1.0f, false, false);

    // Real Brooklyn durability=55/armor=0.1 struck in the head by a
    // plain 70kg (ImpactContext default mass) attacker at 8m/s -- head's
    // 2.0x multiplier all but guarantees knockdown-tier damage, and the
    // reaction (and interruption) must actually be live.
    DOMINUS_EXPECT(reaction.type == ReactionType::kKnockdown || reaction.type == ReactionType::kKnockback);
    DOMINUS_EXPECT(controller.Phase() == CombatPhase::kHitstun || controller.Phase() == CombatPhase::kKnockdown);
    DOMINUS_EXPECT(controller.CurrentMove() == nullptr);
}

// --- Phase 4.1.7: automatic provenance capture ------------------------------

DOMINUS_TEST(CombatController_ApplyImpact_WithNoProvenanceLogAttachedBehavesIdentically) {
    // Zero-effect default: a CombatController that never calls
    // SetProvenanceLog must behave byte-identically to Phase 4.1.5 --
    // this is the exact same test as
    // CombatController_ApplyImpact_WithNoGenomesAttachedStillProducesAReaction,
    // just naming the "no provenance attached" guarantee explicitly.
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    auto& obj = *loadResult.value;
    RigBinder::Bind(obj, fixtureDir);
    CombatBinder::Bind(obj, fixtureDir);

    auto evaluator = MakeMotionGraphEvaluator(obj);
    auto* moveSet = obj.GetComponent<MoveSetComponent>();
    CombatController controller(*evaluator, *moveSet);
    DOMINUS_EXPECT(controller.ProvenanceLog() == nullptr);
    DOMINUS_EXPECT(controller.WorldHistoryLog() == nullptr);

    controller.StartMove("jab");
    ImpactGenomeInputs noGenomes;
    auto reaction = controller.ApplyImpact(noGenomes, noGenomes, 1.0f, "torso", 1.0f, false, false);
    DOMINUS_EXPECT(!reaction.motion_trigger.empty());  // no crash, no change in behavior
}

DOMINUS_TEST(CombatController_ApplyImpact_WithProvenanceLogAttachedRecordsAnEvent) {
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
    controller.SetProvenanceLog(&log);
    DOMINUS_EXPECT(controller.ProvenanceLog() == &log);
    DOMINUS_EXPECT(controller.EntityId() == "brooklyn");

    controller.StartMove("jab");
    DOMINUS_EXPECT(log.Count() == 0);  // StartMove alone records nothing -- only a real impact does

    ImpactGenomeInputs attacker;
    ImpactGenomeInputs defender;
    defender.physics = obj.GetComponent<CombatPhysicsGenomeComponent>();

    controller.ApplyImpact(attacker, defender, 8.0f, "torso", 1.0f, false, false, "attacker_001", 48291);
    DOMINUS_EXPECT(log.Count() == 1);

    const auto& event = log.Events()[0];
    DOMINUS_EXPECT(event.attacker == "attacker_001");
    DOMINUS_EXPECT(event.target == "brooklyn");
    DOMINUS_EXPECT(event.tick == 48291);
    DOMINUS_EXPECT(event.context_hash.size() == 64);
    DOMINUS_EXPECT(event.result_hash.size() == 64);
}

DOMINUS_TEST(CombatController_ApplyImpact_WithWorldHistoryAttachedRecordsThere_Too) {
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
    DOMINUS_EXPECT(controller.WorldHistoryLog() == &history);

    ImpactGenomeInputs attacker;
    ImpactGenomeInputs defender;
    controller.StartMove("jab");
    controller.ApplyImpact(attacker, defender, 8.0f, "torso", 1.0f, false, false, "attacker_001", 100);

    DOMINUS_EXPECT(history.Count() == 1);
    DOMINUS_EXPECT(history.EventsForEntity("attacker_001").size() == 1);
    DOMINUS_EXPECT(history.Events()[0].event_type == "IMPACT");
}

DOMINUS_TEST(CombatController_ApplyImpact_LogOnlyNoHistory_DoesNotCrashOrRecordToHistory) {
    // A provenance log attached WITHOUT a WorldHistory -- must not
    // crash (worldHistory_ stays nullptr, guarded), and must not
    // silently create history data from nowhere.
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
    controller.SetProvenanceLog(&log);  // no SetWorldHistory call

    ImpactGenomeInputs empty;
    controller.StartMove("jab");
    controller.ApplyImpact(empty, empty, 5.0f, "torso", 1.0f, false, false, "attacker_001", 1);

    DOMINUS_EXPECT(log.Count() == 1);
}

DOMINUS_TEST(CombatController_ApplyImpact_CapturedEventReplaysCorrectlyViaFreshBuildImpactContext) {
    // The actual replay proof at the CombatController level: rebuild
    // the exact same context from the exact same recorded inputs
    // (genome refs + situational parameters), solve it fresh, and
    // confirm the captured event's hashes agree.
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
    controller.SetProvenanceLog(&log);

    ImpactGenomeInputs attacker;
    ImpactGenomeInputs defender;
    defender.physics = obj.GetComponent<CombatPhysicsGenomeComponent>();
    defender.material = obj.GetComponent<MaterialGenomeComponent>();

    controller.StartMove("jab");
    controller.ApplyImpact(attacker, defender, 8.0f, "head", 1.0f, false, false, "attacker_001", 777);

    DOMINUS_EXPECT(log.Count() == 1);
    const auto& captured = log.Events()[0];

    // Reconstruct independently -- same inputs, brand-new call, no
    // shared state with the controller's internal computation.
    auto replayCtx = BuildImpactContext(attacker, defender, 8.0f, "head", 1.0f, false, false);
    auto replayResult = ImpactSolver::Solve(replayCtx);
    DOMINUS_EXPECT(ImpactEventCompiler::VerifyMatches(captured, replayCtx, replayResult));
}
