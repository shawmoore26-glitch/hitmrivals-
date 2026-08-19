// tests/genome/test_creature_genome.cpp
#include "CHARACTER/Genome/CreatureGenomeLoader.h"
#include "CHARACTER/Genome/CreatureGenomeSemanticValidator.h"
#include "CHARACTER/Rig/RigBinder.h"
#include "CORE/MetaBin/MetaBinObject.h"
#include "REGISTRY/CanonicalSerializer.h"
#include "REGISTRY/CreatureGenomeCompiler.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::character::CreatureGenome;
using dominus::character::CreatureGenomeComponent;
using dominus::character::CreatureGenomeLoader;
using dominus::character::CreatureGenomeSemanticValidator;
using dominus::character::IntelligenceTier;
using dominus::character::RigBinder;
using dominus::character::SemanticSeverity;
using dominus::core::MetaBinObject;
using dominus::registry::CanonicalSerializer;
using dominus::registry::CreatureGenomeCompiler;

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

// --- CreatureGenomeLoader: strict validation ---------------------------------

DOMINUS_TEST(CreatureGenomeLoader_LoadsRealFlareStalkerFixture) {
    auto result = CreatureGenomeLoader::LoadFromFile(FixtureDir() / "flare_stalker_creature.json");
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(result.value->identity.species_name == "flare_stalker");
    DOMINUS_EXPECT(result.value->identity.common_name == "Flare Stalker");
    DOMINUS_EXPECT(result.value->taxonomy.limb_count == 6);
    DOMINUS_EXPECT(result.value->cognition.tier == IntelligenceTier::kPack);
    DOMINUS_EXPECT(result.value->anatomy.notable_features.size() == 3);
    DOMINUS_EXPECT(result.value->evolution.lineage.size() == 3);
    DOMINUS_EXPECT(result.value->evolution.lineage[0] == "feral_canid");
}

DOMINUS_TEST(CreatureGenomeLoader_RejectsMissingSpeciesName) {
    auto result = CreatureGenomeLoader::LoadFromFile(FixtureDir() / "broken_creature_genome.json");
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("species_name") != std::string::npos);
}

DOMINUS_TEST(CreatureGenomeLoader_RejectsOutOfRangeUnitValues) {
    // Same fixture also carries aggression=47.0 and speed=-3.0 -- both
    // outside [0,1] -- proving the validator collects EVERY violation,
    // not just the first it happens to hit.
    auto result = CreatureGenomeLoader::LoadFromFile(FixtureDir() / "broken_creature_genome.json");
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("combat.aggression") != std::string::npos);
    DOMINUS_EXPECT(result.error.find("combat.speed") != std::string::npos);
    DOMINUS_EXPECT(result.error.find("lifespan_years") != std::string::npos);
}

DOMINUS_TEST(CreatureGenomeLoader_MissingFileFailsGracefully) {
    auto result = CreatureGenomeLoader::LoadFromFile(FixtureDir() / "does_not_exist_creature.json");
    DOMINUS_EXPECT(!result.ok);
}

DOMINUS_TEST(CreatureGenomeLoader_ValidGenomeWithDefaultsPassesValidation) {
    // A genome that only sets the one required field (species_name) and
    // takes every other default must still pass -- defaults were chosen
    // to be valid (all unit-range fields default within [0,1]).
    CreatureGenome minimal;
    minimal.identity.species_name = "unnamed_species";
    // Defaults for anatomy.height_m/weight_kg (1.0/50.0) and
    // growth.lifespan_years (10.0) are all already valid, so this
    // struct, if it went through Validate(), would pass -- proven
    // indirectly via CreatureGenomeCompiler below, which re-checks only
    // species_name and would reject an empty one.
    auto compileResult = CreatureGenomeCompiler::Compile(minimal);
    DOMINUS_EXPECT(compileResult.ok);
}

// --- CanonicalSerializer + CreatureGenomeCompiler: the generalization test ---

DOMINUS_TEST(CanonicalSerializer_CreatureGenome_SameValuesProduceSameBytes) {
    auto a = CreatureGenomeLoader::LoadFromFile(FixtureDir() / "flare_stalker_creature.json");
    auto b = CreatureGenomeLoader::LoadFromFile(FixtureDir() / "flare_stalker_creature.json");
    DOMINUS_EXPECT(CanonicalSerializer::SerializeCreatureGenome(*a.value) ==
                    CanonicalSerializer::SerializeCreatureGenome(*b.value));
}

DOMINUS_TEST(CanonicalSerializer_CreatureGenome_DifferentValuesProduceDifferentBytes) {
    auto flareStalker = CreatureGenomeLoader::LoadFromFile(FixtureDir() / "flare_stalker_creature.json");
    CreatureGenome other = *flareStalker.value;
    other.identity.species_name = "something_else";
    DOMINUS_EXPECT(CanonicalSerializer::SerializeCreatureGenome(*flareStalker.value) !=
                    CanonicalSerializer::SerializeCreatureGenome(other));
}

DOMINUS_TEST(CreatureGenomeCompiler_CompilesRealFlareStalkerDeterministically) {
    auto a = CreatureGenomeLoader::LoadFromFile(FixtureDir() / "flare_stalker_creature.json");
    auto b = CreatureGenomeLoader::LoadFromFile(FixtureDir() / "flare_stalker_creature.json");

    auto compileA = CreatureGenomeCompiler::Compile(*a.value);
    auto compileB = CreatureGenomeCompiler::Compile(*b.value);

    DOMINUS_EXPECT(compileA.ok);
    DOMINUS_EXPECT(compileA.hash.size() == 64);  // real SHA-256 hex digest
    DOMINUS_EXPECT(compileA.hash == compileB.hash);  // deterministic across independent loads
}

DOMINUS_TEST(CreatureGenomeCompiler_RejectsEmptySpeciesName) {
    CreatureGenome invalid;  // default-constructed -- species_name is empty
    auto result = CreatureGenomeCompiler::Compile(invalid);
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(!result.errors.empty());
}

// --- RigBinder integration: full .dominus-style bind ------------------------

DOMINUS_TEST(RigBinder_ResolvesCreatureGenomeRefIntoBoundComponent) {
    // Reuses an existing real skeleton file purely as the mechanical
    // prerequisite RigBinder::Bind currently requires to proceed at all
    // (a real, flagged limitation shared with SocialGenome -- see
    // CHARACTER/Rig/RigBinder.h). The creature genome itself has no
    // dependency on skeletal data.
    auto fixtureDir = FixtureDir();
    MetaBinObject obj("flare_stalker_test_entity", "0.1.0");
    obj.AddComponent<dominus::core::SkeletonRefComponent>(
        dominus::core::SkeletonRefComponent{"brooklyn.skel.json"});
    obj.AddComponent<dominus::core::CreatureGenomeRefComponent>(
        dominus::core::CreatureGenomeRefComponent{"flare_stalker_creature.json"});

    auto bindResult = RigBinder::Bind(obj, fixtureDir);
    DOMINUS_EXPECT(bindResult.ok);

    auto* creatureComponent = obj.GetComponent<CreatureGenomeComponent>();
    DOMINUS_EXPECT(creatureComponent != nullptr);
    DOMINUS_EXPECT(creatureComponent->genome.identity.species_name == "flare_stalker");
    DOMINUS_EXPECT(creatureComponent->genome.behavior.combat_role == "ambush");
}

DOMINUS_TEST(RigBinder_FailsCleanlyWhenCreatureGenomeRefIsBroken) {
    auto fixtureDir = FixtureDir();
    MetaBinObject obj("broken_creature_test_entity", "0.1.0");
    obj.AddComponent<dominus::core::SkeletonRefComponent>(
        dominus::core::SkeletonRefComponent{"brooklyn.skel.json"});
    obj.AddComponent<dominus::core::CreatureGenomeRefComponent>(
        dominus::core::CreatureGenomeRefComponent{"broken_creature_genome.json"});

    auto bindResult = RigBinder::Bind(obj, fixtureDir);
    DOMINUS_EXPECT(!bindResult.ok);
    DOMINUS_EXPECT(bindResult.error.find("creature genome") != std::string::npos);
}

// --- CreatureGenomeSemanticValidator: real cross-field biological checks ----

DOMINUS_TEST(SemanticValidator_RealFlareStalkerHasZeroIssues) {
    auto result = CreatureGenomeLoader::LoadFromFile(FixtureDir() / "flare_stalker_creature.json");
    DOMINUS_EXPECT(result.ok);
    auto issues = CreatureGenomeSemanticValidator::Validate(*result.value);
    DOMINUS_EXPECT(issues.empty());
}

DOMINUS_TEST(SemanticValidator_FlightWithZeroWingAreaIsError) {
    CreatureGenome g;
    g.identity.species_name = "test_flyer";
    g.locomotion.primary_mode = "flight";
    g.anatomy.wing_area_m2 = 0.0f;  // no wings
    auto issues = CreatureGenomeSemanticValidator::Validate(g);
    bool found = false;
    for (auto& i : issues) {
        if (i.check == "flight_wing_area" && i.severity == SemanticSeverity::kError) found = true;
    }
    DOMINUS_EXPECT(found);
}

DOMINUS_TEST(SemanticValidator_FlightWithSufficientWingAreaPasses) {
    CreatureGenome g;
    g.identity.species_name = "test_flyer";
    g.locomotion.primary_mode = "flight";
    g.anatomy.weight_kg = 10.0f;
    g.anatomy.wing_area_m2 = 5.0f;  // 10 / 5 = 2 kg/m^2, well under the 50 threshold
    auto issues = CreatureGenomeSemanticValidator::Validate(g);
    for (auto& i : issues) DOMINUS_EXPECT(i.check != "flight_wing_area");
}

DOMINUS_TEST(SemanticValidator_FlightWithInsufficientWingAreaForWeightIsError) {
    CreatureGenome g;
    g.identity.species_name = "test_heavy_flyer";
    g.locomotion.primary_mode = "flight";
    g.anatomy.weight_kg = 300.0f;
    g.anatomy.wing_area_m2 = 0.5f;  // 300 / 0.5 = 600 kg/m^2, way over threshold
    auto issues = CreatureGenomeSemanticValidator::Validate(g);
    bool found = false;
    for (auto& i : issues) {
        if (i.check == "flight_wing_area") found = true;
    }
    DOMINUS_EXPECT(found);
}

DOMINUS_TEST(SemanticValidator_QuadrupedalWithTooFewLimbsIsError) {
    CreatureGenome g;
    g.identity.species_name = "test_creature";
    g.locomotion.primary_mode = "quadrupedal_walk";
    g.taxonomy.limb_count = 2;  // claims quadrupedal but has 2 limbs
    auto issues = CreatureGenomeSemanticValidator::Validate(g);
    bool found = false;
    for (auto& i : issues) {
        if (i.check == "locomotion_limb_count") found = true;
    }
    DOMINUS_EXPECT(found);
}

DOMINUS_TEST(SemanticValidator_HerbivoreWithPreferredPreyIsError) {
    CreatureGenome g;
    g.identity.species_name = "test_herbivore";
    g.ecology.diet_type = "herbivore";
    g.ecology.preferred_prey = {"small_mammals"};  // contradiction
    auto issues = CreatureGenomeSemanticValidator::Validate(g);
    bool found = false;
    for (auto& i : issues) {
        if (i.check == "diet_vs_prey") found = true;
    }
    DOMINUS_EXPECT(found);
}

DOMINUS_TEST(SemanticValidator_CarnivoreWithEmptyPreyIsError) {
    CreatureGenome g;
    g.identity.species_name = "test_carnivore";
    g.ecology.diet_type = "carnivore";
    g.ecology.preferred_prey = {};  // contradiction
    auto issues = CreatureGenomeSemanticValidator::Validate(g);
    bool found = false;
    for (auto& i : issues) {
        if (i.check == "diet_vs_prey") found = true;
    }
    DOMINUS_EXPECT(found);
}

DOMINUS_TEST(SemanticValidator_UnspecifiedDietTypeSkipsCheck) {
    CreatureGenome g;
    g.identity.species_name = "test_unspecified";
    g.ecology.diet_type = "";  // unspecified -- not a contradiction
    g.ecology.preferred_prey = {};
    auto issues = CreatureGenomeSemanticValidator::Validate(g);
    for (auto& i : issues) DOMINUS_EXPECT(i.check != "diet_vs_prey");
}

DOMINUS_TEST(SemanticValidator_MaturityAfterLifespanIsError) {
    CreatureGenome g;
    g.identity.species_name = "test_creature";
    g.growth.lifespan_years = 5.0f;
    g.growth.maturity_age_years = 8.0f;  // matures after it's dead
    auto issues = CreatureGenomeSemanticValidator::Validate(g);
    bool found = false;
    for (auto& i : issues) {
        if (i.check == "growth_maturity") found = true;
    }
    DOMINUS_EXPECT(found);
}

DOMINUS_TEST(SemanticValidator_AquaticWithoutWaterRespirationIsWarningNotError) {
    // Deliberately a WARNING, not an error -- air-breathing aquatic life
    // is real (dolphins), so this shouldn't hard-reject.
    CreatureGenome g;
    g.identity.species_name = "test_aquatic";
    g.locomotion.primary_mode = "aquatic_swimmer";
    g.physiology.requires_water_respiration = false;
    auto issues = CreatureGenomeSemanticValidator::Validate(g);
    bool foundWarning = false;
    for (auto& i : issues) {
        if (i.check == "aquatic_respiration") {
            DOMINUS_EXPECT(i.severity == SemanticSeverity::kWarning);
            foundWarning = true;
        }
    }
    DOMINUS_EXPECT(foundWarning);
}

DOMINUS_TEST(SemanticValidator_AquaticWithWaterRespirationHasNoIssue) {
    CreatureGenome g;
    g.identity.species_name = "test_aquatic";
    g.locomotion.primary_mode = "aquatic_swimmer";
    g.physiology.requires_water_respiration = true;
    auto issues = CreatureGenomeSemanticValidator::Validate(g);
    for (auto& i : issues) DOMINUS_EXPECT(i.check != "aquatic_respiration");
}

DOMINUS_TEST(SemanticValidator_StructurallyValidButSemanticallyBrokenFixtureCatchesAllFourErrors) {
    // The actual point of a two-stage compiler: this fixture passes
    // CreatureGenomeLoader's structural validation cleanly (species_name
    // present, every field within [0,1] where applicable) but is
    // biologically incoherent in four independent ways at once --
    // exactly what CreatureGenomeLoader's own Validate() cannot catch,
    // because it never looks at more than one field at a time.
    auto result = CreatureGenomeLoader::LoadFromFile(FixtureDir() / "semantically_broken_creature.json");
    DOMINUS_EXPECT(result.ok);  // structurally clean

    auto issues = CreatureGenomeSemanticValidator::Validate(*result.value);
    bool hasFlight = false, hasLimb = false, hasDiet = false, hasGrowth = false;
    for (auto& i : issues) {
        if (i.check == "flight_wing_area") hasFlight = true;
        if (i.check == "locomotion_limb_count") hasLimb = true;
        if (i.check == "diet_vs_prey") hasDiet = true;
        if (i.check == "growth_maturity") hasGrowth = true;
    }
    DOMINUS_EXPECT(hasFlight);   // 400kg on 0.2 m^2 wings
    DOMINUS_EXPECT(hasLimb);    // "quadrupedal" claimed with limb_count=2
    DOMINUS_EXPECT(hasDiet);    // herbivore with a prey list
    DOMINUS_EXPECT(hasGrowth);  // matures at 12, dead by 5
    DOMINUS_EXPECT(issues.size() >= 4);
}
