// tests/genome/test_combat_physics_genome.cpp
#include "CHARACTER/Genome/CombatPhysicsGenomeLoader.h"
#include "CHARACTER/Rig/RigBinder.h"
#include "COMBAT/PhysicsCombat/ImpactSolver.h"
#include "CORE/MetaBin/MetaBinObject.h"
#include "CORE/Serialization/DominusSerializer.h"
#include "REGISTRY/CanonicalSerializer.h"
#include "REGISTRY/CombatPhysicsGenomeCompiler.h"
#include "tests/TestFramework.h"

#include <cmath>
#include <filesystem>

using dominus::character::CombatPhysicsGenome;
using dominus::character::CombatPhysicsGenomeLoader;
using dominus::combat::ImpactSeverity;
using dominus::combat::ImpactSolver;
using dominus::registry::CanonicalSerializer;
using dominus::registry::CombatPhysicsGenomeCompiler;

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

// --- CombatPhysicsGenomeLoader: strict validation ----------------------------

DOMINUS_TEST(CombatPhysicsGenomeLoader_LoadsRealBrooklynPhysics) {
    auto result = CombatPhysicsGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_combat_physics.json");
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(NearlyEqual(result.value->body.mass_kg, 78.0f));
    DOMINUS_EXPECT(NearlyEqual(result.value->impact.durability, 55.0f));
}

DOMINUS_TEST(CombatPhysicsGenomeLoader_RejectsInvalidValues) {
    // broken_combat_physics.json has mass=-10, armor=2.5, stamina=0,
    // durability=-5 -- four independent violations, all must be caught.
    auto result = CombatPhysicsGenomeLoader::LoadFromFile(FixtureDir() / "broken_combat_physics.json");
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("mass_kg") != std::string::npos);
    DOMINUS_EXPECT(result.error.find("armor") != std::string::npos);
    DOMINUS_EXPECT(result.error.find("stamina") != std::string::npos);
    DOMINUS_EXPECT(result.error.find("durability") != std::string::npos);
}

DOMINUS_TEST(CombatPhysicsGenomeLoader_MissingFileFailsGracefully) {
    auto result = CombatPhysicsGenomeLoader::LoadFromFile(FixtureDir() / "does_not_exist_physics.json");
    DOMINUS_EXPECT(!result.ok);
}

DOMINUS_TEST(CombatPhysicsGenomeLoader_DefaultsAreAllValid) {
    // An empty file (no fields set) must still pass -- every default is
    // itself a valid value.
    CombatPhysicsGenome g;  // default-constructed
    auto compileResult = CombatPhysicsGenomeCompiler::Compile(g);
    DOMINUS_EXPECT(compileResult.ok);
}

// --- CanonicalSerializer + Compiler: fourth genome type through the pipeline -

DOMINUS_TEST(CanonicalSerializer_CombatPhysicsGenome_SameValuesProduceSameBytes) {
    auto a = CombatPhysicsGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_combat_physics.json");
    auto b = CombatPhysicsGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_combat_physics.json");
    DOMINUS_EXPECT(CanonicalSerializer::SerializeCombatPhysicsGenome(*a.value) ==
                    CanonicalSerializer::SerializeCombatPhysicsGenome(*b.value));
}

DOMINUS_TEST(CombatPhysicsGenomeCompiler_CompilesDeterministically) {
    auto a = CombatPhysicsGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_combat_physics.json");
    auto b = CombatPhysicsGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_combat_physics.json");
    auto compileA = CombatPhysicsGenomeCompiler::Compile(*a.value);
    auto compileB = CombatPhysicsGenomeCompiler::Compile(*b.value);
    DOMINUS_EXPECT(compileA.ok);
    DOMINUS_EXPECT(compileA.hash.size() == 64);
    DOMINUS_EXPECT(compileA.hash == compileB.hash);
}

DOMINUS_TEST(CombatPhysicsGenomeCompiler_RejectsNonPositiveMass) {
    CombatPhysicsGenome invalid;
    invalid.body.mass_kg = 0.0f;
    auto result = CombatPhysicsGenomeCompiler::Compile(invalid);
    DOMINUS_EXPECT(!result.ok);
}

// --- ImpactSolver: real force/impact/damage math -----------------------------

DOMINUS_TEST(ImpactSolver_ForceIsMassTimesVelocity) {
    float force = ImpactSolver::CalculateForce(10.0f, 5.0f);
    DOMINUS_EXPECT(NearlyEqual(force, 50.0f));
}

DOMINUS_TEST(ImpactSolver_HighForceExceedingDurabilityByOverHundredIsHighSeverity) {
    auto result = ImpactSolver::ResolveImpact(50.0f, 10.0f, 50.0f);  // force=500, resistance=50, diff=450
    DOMINUS_EXPECT(result.severity == ImpactSeverity::kHigh);
}

DOMINUS_TEST(ImpactSolver_SmallPositiveDifferenceIsModerateSeverity) {
    auto result = ImpactSolver::ResolveImpact(10.0f, 6.0f, 50.0f);  // force=60, resistance=50, diff=10
    DOMINUS_EXPECT(result.severity == ImpactSeverity::kModerate);
}

DOMINUS_TEST(ImpactSolver_ForceBelowDurabilityIsLowSeverity) {
    auto result = ImpactSolver::ResolveImpact(5.0f, 2.0f, 50.0f);  // force=10, resistance=50, diff=-40
    DOMINUS_EXPECT(result.severity == ImpactSeverity::kLow);
}

DOMINUS_TEST(ImpactSolver_ArmorReducesForceProportionally) {
    float reduced = ImpactSolver::ApplyArmor(100.0f, 0.3f);  // 30% absorbed
    DOMINUS_EXPECT(NearlyEqual(reduced, 70.0f));
}

DOMINUS_TEST(ImpactSolver_ArmorClampsToValidRange) {
    DOMINUS_EXPECT(NearlyEqual(ImpactSolver::ApplyArmor(100.0f, 1.5f), 0.0f));    // over 1.0 clamps to full absorption
    DOMINUS_EXPECT(NearlyEqual(ImpactSolver::ApplyArmor(100.0f, -0.5f), 100.0f));  // negative clamps to zero absorption
}

DOMINUS_TEST(ImpactSolver_HeadTakesDoubleDamageOfTorso) {
    float headDamage = ImpactSolver::CalculateDamage(100.0f, "head");
    float torsoDamage = ImpactSolver::CalculateDamage(100.0f, "torso");
    DOMINUS_EXPECT(NearlyEqual(headDamage, 200.0f));
    DOMINUS_EXPECT(NearlyEqual(torsoDamage, 100.0f));
    DOMINUS_EXPECT(headDamage > torsoDamage * 1.9f);
}

DOMINUS_TEST(ImpactSolver_LegsTakeMoreDamageThanArms) {
    float legDamage = ImpactSolver::CalculateDamage(100.0f, "leg");
    float armDamage = ImpactSolver::CalculateDamage(100.0f, "arm");
    DOMINUS_EXPECT(legDamage > armDamage);
}

DOMINUS_TEST(ImpactSolver_UnknownBodyPartGetsNeutralMultiplier) {
    float damage = ImpactSolver::CalculateDamage(100.0f, "tail");  // not in the table
    DOMINUS_EXPECT(NearlyEqual(damage, 100.0f));  // 1.0x, not zeroed or inflated
}

DOMINUS_TEST(ImpactSolver_FullPipeline_BrooklynPhysicsIntoARealImpact) {
    // The complete, real chain: load Brooklyn's actual physics genome,
    // simulate an attacker's strike against him, apply his real armor,
    // then real per-body-part damage -- every number traced back to an
    // input, nothing invented.
    auto brooklynResult = CombatPhysicsGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_combat_physics.json");
    DOMINUS_EXPECT(brooklynResult.ok);
    auto& brooklyn = *brooklynResult.value;

    float attackerMass = 90.0f;
    float attackVelocity = 8.0f;
    auto impact = ImpactSolver::ResolveImpact(attackerMass, attackVelocity, brooklyn.impact.durability);
    DOMINUS_EXPECT(NearlyEqual(impact.force, 720.0f));  // 90 * 8

    float postArmor = ImpactSolver::ApplyArmor(impact.force, brooklyn.body.armor);
    DOMINUS_EXPECT(postArmor < impact.force);  // armor genuinely reduced it

    float finalDamage = ImpactSolver::CalculateDamage(postArmor, "torso");
    DOMINUS_EXPECT(NearlyEqual(finalDamage, postArmor));  // torso multiplier is 1.0
}

// --- RigBinder integration: CombatPhysicsGenome now binds to a real entity --

DOMINUS_TEST(RigBinder_ResolvesCombatPhysicsGenomeRefIntoBoundComponent) {
    auto fixtureDir = FixtureDir();
    auto loadResult = dominus::core::DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    DOMINUS_EXPECT(loadResult.ok);
    auto bindResult = dominus::character::RigBinder::Bind(*loadResult.value, fixtureDir);
    DOMINUS_EXPECT(bindResult.ok);

    auto* physicsComponent = loadResult.value->GetComponent<dominus::character::CombatPhysicsGenomeComponent>();
    DOMINUS_EXPECT(physicsComponent != nullptr);
    DOMINUS_EXPECT(NearlyEqual(physicsComponent->genome.body.mass_kg, 78.0f));
    DOMINUS_EXPECT(NearlyEqual(physicsComponent->genome.impact.durability, 55.0f));
}

DOMINUS_TEST(RigBinder_FailsCleanlyWhenCombatPhysicsGenomeRefIsBroken) {
    auto fixtureDir = FixtureDir();
    dominus::core::MetaBinObject obj("broken_combat_physics_test_entity", "0.1.0");
    obj.AddComponent<dominus::core::SkeletonRefComponent>(dominus::core::SkeletonRefComponent{"brooklyn.skel.json"});
    obj.AddComponent<dominus::core::CombatPhysicsGenomeRefComponent>(
        dominus::core::CombatPhysicsGenomeRefComponent{"broken_combat_physics.json"});

    auto bindResult = dominus::character::RigBinder::Bind(obj, fixtureDir);
    DOMINUS_EXPECT(!bindResult.ok);
    DOMINUS_EXPECT(bindResult.error.find("combat physics genome") != std::string::npos);
}
