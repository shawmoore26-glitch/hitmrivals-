// tests/genome/test_game_design_genome.cpp
#include "CHARACTER/Genome/CombatPhysicsGenomeLoader.h"
#include "CHARACTER/Genome/CombatStyleGenomeLoader.h"
#include "CHARACTER/Genome/GameDesignCoherenceChecker.h"
#include "CHARACTER/Genome/GameDesignGenomeLoader.h"
#include "CHARACTER/Rig/RigBinder.h"
#include "CORE/MetaBin/MetaBinObject.h"
#include "CORE/Serialization/DominusSerializer.h"
#include "REGISTRY/CanonicalSerializer.h"
#include "REGISTRY/GameDesignGenomeCompiler.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::character::CombatPhysicsGenome;
using dominus::character::CombatPhysicsGenomeLoader;
using dominus::character::CombatStyleGenome;
using dominus::character::CombatStyleGenomeLoader;
using dominus::character::GameDesignCoherenceChecker;
using dominus::character::GameDesignGenome;
using dominus::character::GameDesignGenomeLoader;
using dominus::registry::CanonicalSerializer;
using dominus::registry::GameDesignGenomeCompiler;

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

// --- GameDesignGenomeLoader: strict validation -------------------------------

DOMINUS_TEST(GameDesignGenomeLoader_LoadsRealSoulslikeDesign) {
    auto result = GameDesignGenomeLoader::LoadFromFile(FixtureDir() / "design_soulslike.json");
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(result.value->genre == "soulslike");
}

DOMINUS_TEST(GameDesignGenomeLoader_LoadsRealArcadeDesign) {
    auto result = GameDesignGenomeLoader::LoadFromFile(FixtureDir() / "design_arcade.json");
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(result.value->genre == "arcade");
}

DOMINUS_TEST(GameDesignGenomeLoader_RejectsMissingGenreAndOutOfRangeValues) {
    auto result = GameDesignGenomeLoader::LoadFromFile(FixtureDir() / "broken_game_design.json");
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("genre") != std::string::npos);
    DOMINUS_EXPECT(result.error.find("difficulty") != std::string::npos);
    DOMINUS_EXPECT(result.error.find("risk_reward_balance") != std::string::npos);
}

DOMINUS_TEST(GameDesignGenomeLoader_MissingFileFailsGracefully) {
    auto result = GameDesignGenomeLoader::LoadFromFile(FixtureDir() / "does_not_exist_design.json");
    DOMINUS_EXPECT(!result.ok);
}

// --- CanonicalSerializer + Compiler: fifth genome type through the pipeline --

DOMINUS_TEST(CanonicalSerializer_GameDesignGenome_SameValuesProduceSameBytes) {
    auto a = GameDesignGenomeLoader::LoadFromFile(FixtureDir() / "design_soulslike.json");
    auto b = GameDesignGenomeLoader::LoadFromFile(FixtureDir() / "design_soulslike.json");
    DOMINUS_EXPECT(CanonicalSerializer::SerializeGameDesignGenome(*a.value) ==
                    CanonicalSerializer::SerializeGameDesignGenome(*b.value));
}

DOMINUS_TEST(GameDesignGenomeCompiler_CompilesDeterministically) {
    auto a = GameDesignGenomeLoader::LoadFromFile(FixtureDir() / "design_soulslike.json");
    auto b = GameDesignGenomeLoader::LoadFromFile(FixtureDir() / "design_soulslike.json");
    auto compileA = GameDesignGenomeCompiler::Compile(*a.value);
    auto compileB = GameDesignGenomeCompiler::Compile(*b.value);
    DOMINUS_EXPECT(compileA.ok);
    DOMINUS_EXPECT(compileA.hash.size() == 64);
    DOMINUS_EXPECT(compileA.hash == compileB.hash);
}

DOMINUS_TEST(GameDesignGenomeCompiler_RejectsEmptyGenre) {
    GameDesignGenome invalid;
    auto result = GameDesignGenomeCompiler::Compile(invalid);
    DOMINUS_EXPECT(!result.ok);
}

// --- GameDesignCoherenceChecker: advisory notes, computed not assumed -------

DOMINUS_TEST(CoherenceChecker_UnrecognizedGenreProducesZeroNotes) {
    // "roguelike" was never given concrete criteria by the source
    // document -- the checker must not guess.
    GameDesignGenome design;
    design.genre = "roguelike";
    CombatStyleGenome style;
    CombatPhysicsGenome physics;
    auto notes = GameDesignCoherenceChecker::Evaluate(design, style, physics);
    DOMINUS_EXPECT(notes.empty());
}

DOMINUS_TEST(CoherenceChecker_LowFatigueRateFlaggedForSoulslike) {
    GameDesignGenome design;
    design.genre = "soulslike";
    CombatStyleGenome style;
    style.precision = 0.9f;  // high precision, avoids the second check
    CombatPhysicsGenome physics;
    physics.energy.fatigue_rate = 0.5f;  // below 1.0
    auto notes = GameDesignCoherenceChecker::Evaluate(design, style, physics);
    bool found = false;
    for (auto& n : notes) {
        if (n.field == "physics.energy.fatigue_rate") found = true;
    }
    DOMINUS_EXPECT(found);
}

DOMINUS_TEST(CoherenceChecker_HighMobilityLowPrecisionFlaggedForSoulslike) {
    GameDesignGenome design;
    design.genre = "soulslike";
    CombatStyleGenome style;
    style.mobility = 0.95f;
    style.precision = 0.2f;
    CombatPhysicsGenome physics;
    physics.energy.fatigue_rate = 1.5f;  // avoids the first check
    auto notes = GameDesignCoherenceChecker::Evaluate(design, style, physics);
    bool found = false;
    for (auto& n : notes) {
        if (n.field == "style.precision / style.mobility") found = true;
    }
    DOMINUS_EXPECT(found);
}

DOMINUS_TEST(CoherenceChecker_HighFatigueRateFlaggedForArcade) {
    GameDesignGenome design;
    design.genre = "arcade";
    CombatStyleGenome style;
    style.mobility = 0.9f;  // avoids the second check
    CombatPhysicsGenome physics;
    physics.energy.fatigue_rate = 1.5f;  // above 1.0
    auto notes = GameDesignCoherenceChecker::Evaluate(design, style, physics);
    bool found = false;
    for (auto& n : notes) {
        if (n.field == "physics.energy.fatigue_rate") found = true;
    }
    DOMINUS_EXPECT(found);
}

DOMINUS_TEST(CoherenceChecker_LowMobilityFlaggedForArcade) {
    GameDesignGenome design;
    design.genre = "arcade";
    CombatStyleGenome style;
    style.mobility = 0.3f;
    CombatPhysicsGenome physics;
    physics.energy.fatigue_rate = 0.8f;  // avoids the first check
    auto notes = GameDesignCoherenceChecker::Evaluate(design, style, physics);
    bool found = false;
    for (auto& n : notes) {
        if (n.field == "style.mobility") found = true;
    }
    DOMINUS_EXPECT(found);
}

// --- The real proof: one genre value changes the notes, on REAL data --------

DOMINUS_TEST(CoherenceChecker_RealBrooklynDataProducesDifferentNotesPerGenre) {
    // Brooklyn's actual style (aggression=0.85, mobility=0.9,
    // precision=0.5) and physics (fatigue_rate=1.3) run against both
    // real fixture genres -- this is the actual "change one value,
    // every forge's output changes" claim, proven, not narrated.
    auto styleResult = CombatStyleGenomeLoader::LoadFromFile(FixtureDir() / "psycho_drunken_martial_arts_style.json");
    auto physicsResult = CombatPhysicsGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_combat_physics.json");
    DOMINUS_EXPECT(styleResult.ok);
    DOMINUS_EXPECT(physicsResult.ok);

    auto soulslikeResult = GameDesignGenomeLoader::LoadFromFile(FixtureDir() / "design_soulslike.json");
    auto arcadeResult = GameDesignGenomeLoader::LoadFromFile(FixtureDir() / "design_arcade.json");
    DOMINUS_EXPECT(soulslikeResult.ok);
    DOMINUS_EXPECT(arcadeResult.ok);

    auto soulslikeNotes = GameDesignCoherenceChecker::Evaluate(*soulslikeResult.value, *styleResult.value,
                                                                 *physicsResult.value);
    auto arcadeNotes =
        GameDesignCoherenceChecker::Evaluate(*arcadeResult.value, *styleResult.value, *physicsResult.value);

    // The same fighter data produces genuinely different results
    // depending purely on the declared genre -- proving the cascade is
    // real, not the specific counts (which depend on the exact
    // fixture values and could legitimately change if either fixture
    // is edited).
    DOMINUS_EXPECT(soulslikeNotes.size() != arcadeNotes.size());
}

// --- RigBinder integration: GameDesignGenome now binds to a real entity -----

DOMINUS_TEST(RigBinder_ResolvesGameDesignGenomeRefIntoBoundComponent) {
    // Brooklyn's real fixture references design_soulslike.json -- the
    // same fixture the coherence-checker proof above used, and the
    // genre his real style/physics data reads as coherent against.
    auto fixtureDir = FixtureDir();
    auto loadResult = dominus::core::DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    DOMINUS_EXPECT(loadResult.ok);
    auto bindResult = dominus::character::RigBinder::Bind(*loadResult.value, fixtureDir);
    DOMINUS_EXPECT(bindResult.ok);

    auto* designComponent = loadResult.value->GetComponent<dominus::character::GameDesignGenomeComponent>();
    DOMINUS_EXPECT(designComponent != nullptr);
    DOMINUS_EXPECT(designComponent->genome.genre == "soulslike");
}

DOMINUS_TEST(RigBinder_FailsCleanlyWhenGameDesignGenomeRefIsBroken) {
    auto fixtureDir = FixtureDir();
    dominus::core::MetaBinObject obj("broken_game_design_test_entity", "0.1.0");
    obj.AddComponent<dominus::core::SkeletonRefComponent>(dominus::core::SkeletonRefComponent{"brooklyn.skel.json"});
    obj.AddComponent<dominus::core::GameDesignGenomeRefComponent>(
        dominus::core::GameDesignGenomeRefComponent{"broken_game_design.json"});

    auto bindResult = dominus::character::RigBinder::Bind(obj, fixtureDir);
    DOMINUS_EXPECT(!bindResult.ok);
    DOMINUS_EXPECT(bindResult.error.find("game design genome") != std::string::npos);
}
