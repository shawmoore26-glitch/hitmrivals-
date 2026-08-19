// tests/genome/test_combat_style_genome.cpp
#include "CHARACTER/Genome/CombatIdentityLoader.h"
#include "CHARACTER/Genome/CombatStyleGenomeLoader.h"
#include "CHARACTER/Rig/RigBinder.h"
#include "CORE/MetaBin/MetaBinObject.h"
#include "CORE/Serialization/DominusSerializer.h"
#include "REGISTRY/CanonicalSerializer.h"
#include "REGISTRY/CombatStyleGenomeCompiler.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::character::CombatIdentityLoader;
using dominus::character::CombatStyleGenome;
using dominus::character::CombatStyleGenomeLoader;
using dominus::registry::CanonicalSerializer;
using dominus::registry::CombatStyleGenomeCompiler;

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

// --- CombatStyleGenomeLoader: strict validation ------------------------------

DOMINUS_TEST(CombatStyleGenomeLoader_LoadsRealPsychoDrunkenStyle) {
    auto result = CombatStyleGenomeLoader::LoadFromFile(FixtureDir() / "psycho_drunken_martial_arts_style.json");
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(result.value->style_name == "psycho_drunken_martial_arts");
    DOMINUS_EXPECT(result.value->ancestry.size() == 4);
    DOMINUS_EXPECT(result.value->ancestry[0] == "drunken_boxing");
    DOMINUS_EXPECT(result.value->weaknesses.size() == 3);
}

DOMINUS_TEST(CombatStyleGenomeLoader_RejectsMissingStyleName) {
    auto result = CombatStyleGenomeLoader::LoadFromFile(FixtureDir() / "broken_combat_style.json");
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("style_name") != std::string::npos);
}

DOMINUS_TEST(CombatStyleGenomeLoader_RejectsOutOfRangeValues) {
    // broken_combat_style.json also has aggression=42.0 and
    // defense=-1.5 -- both out of range -- proving every violation gets
    // collected, not just the first.
    auto result = CombatStyleGenomeLoader::LoadFromFile(FixtureDir() / "broken_combat_style.json");
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("aggression") != std::string::npos);
    DOMINUS_EXPECT(result.error.find("defense") != std::string::npos);
}

DOMINUS_TEST(CombatStyleGenomeLoader_MissingFileFailsGracefully) {
    auto result = CombatStyleGenomeLoader::LoadFromFile(FixtureDir() / "does_not_exist_style.json");
    DOMINUS_EXPECT(!result.ok);
}

// --- CanonicalSerializer + CombatStyleGenomeCompiler: same discipline as CreatureGenome --

DOMINUS_TEST(CanonicalSerializer_CombatStyleGenome_SameValuesProduceSameBytes) {
    auto a = CombatStyleGenomeLoader::LoadFromFile(FixtureDir() / "psycho_drunken_martial_arts_style.json");
    auto b = CombatStyleGenomeLoader::LoadFromFile(FixtureDir() / "psycho_drunken_martial_arts_style.json");
    DOMINUS_EXPECT(CanonicalSerializer::SerializeCombatStyleGenome(*a.value) ==
                    CanonicalSerializer::SerializeCombatStyleGenome(*b.value));
}

DOMINUS_TEST(CombatStyleGenomeCompiler_CompilesDeterministically) {
    auto a = CombatStyleGenomeLoader::LoadFromFile(FixtureDir() / "psycho_drunken_martial_arts_style.json");
    auto b = CombatStyleGenomeLoader::LoadFromFile(FixtureDir() / "psycho_drunken_martial_arts_style.json");

    auto compileA = CombatStyleGenomeCompiler::Compile(*a.value);
    auto compileB = CombatStyleGenomeCompiler::Compile(*b.value);

    DOMINUS_EXPECT(compileA.ok);
    DOMINUS_EXPECT(compileA.hash.size() == 64);
    DOMINUS_EXPECT(compileA.hash == compileB.hash);
}

DOMINUS_TEST(CombatStyleGenomeCompiler_RejectsEmptyStyleName) {
    CombatStyleGenome invalid;  // default-constructed, style_name empty
    auto result = CombatStyleGenomeCompiler::Compile(invalid);
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(!result.errors.empty());
}

// --- The real connection: Brooklyn's CombatIdentity.style vs the style genome ---

DOMINUS_TEST(CombatStyleGenomeLoader_BrokenFixtureAlsoMissesWeaknesses) {
    // The document's own literal example: "Combat Style -> No Weakness ->
    // FAIL." broken_combat_style.json has no weaknesses field, so its
    // rejection must now cite that specifically, not just the other
    // pre-existing violations.
    auto result = CombatStyleGenomeLoader::LoadFromFile(FixtureDir() / "broken_combat_style.json");
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("weaknesses") != std::string::npos);
}

DOMINUS_TEST(CombatStyleGenome_StyleNameMatchesBrooklynsRealCombatIdentity) {
    // Not a coincidence of naming -- proving the two systems actually
    // agree: Brooklyn's real combat_dna file's `style` field and this
    // new style genome's `style_name` are the same string, loaded from
    // two completely independent files through two completely
    // independent loaders.
    auto identityResult = CombatIdentityLoader::LoadFromFile(FixtureDir() / "brooklyn_combat.json");
    auto styleResult = CombatStyleGenomeLoader::LoadFromFile(FixtureDir() / "psycho_drunken_martial_arts_style.json");
    DOMINUS_EXPECT(identityResult.ok);
    DOMINUS_EXPECT(styleResult.ok);
    DOMINUS_EXPECT(identityResult.value->style == styleResult.value->style_name);
}

// --- RigBinder integration: CombatStyleGenome now binds to a real entity ----

DOMINUS_TEST(RigBinder_ResolvesCombatStyleGenomeRefIntoBoundComponent) {
    auto fixtureDir = FixtureDir();
    auto loadResult = dominus::core::DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    DOMINUS_EXPECT(loadResult.ok);
    auto bindResult = dominus::character::RigBinder::Bind(*loadResult.value, fixtureDir);
    DOMINUS_EXPECT(bindResult.ok);

    auto* styleComponent = loadResult.value->GetComponent<dominus::character::CombatStyleGenomeComponent>();
    DOMINUS_EXPECT(styleComponent != nullptr);
    DOMINUS_EXPECT(styleComponent->genome.style_name == "psycho_drunken_martial_arts");
    DOMINUS_EXPECT(styleComponent->genome.ancestry.size() == 4);
}

DOMINUS_TEST(RigBinder_FailsCleanlyWhenCombatStyleGenomeRefIsBroken) {
    auto fixtureDir = FixtureDir();
    dominus::core::MetaBinObject obj("broken_combat_style_test_entity", "0.1.0");
    obj.AddComponent<dominus::core::SkeletonRefComponent>(dominus::core::SkeletonRefComponent{"brooklyn.skel.json"});
    obj.AddComponent<dominus::core::CombatStyleGenomeRefComponent>(
        dominus::core::CombatStyleGenomeRefComponent{"broken_combat_style.json"});

    auto bindResult = dominus::character::RigBinder::Bind(obj, fixtureDir);
    DOMINUS_EXPECT(!bindResult.ok);
    DOMINUS_EXPECT(bindResult.error.find("combat style genome") != std::string::npos);
}
