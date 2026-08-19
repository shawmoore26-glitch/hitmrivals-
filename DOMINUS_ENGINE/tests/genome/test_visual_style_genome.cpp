// tests/genome/test_visual_style_genome.cpp
#include "CHARACTER/Genome/VisualGenomeLoader.h"
#include "CHARACTER/Genome/VisualStyleGenomeLoader.h"
#include "CHARACTER/Rig/RigBinder.h"
#include "CORE/MetaBin/MetaBinObject.h"
#include "CORE/Serialization/DominusSerializer.h"
#include "REGISTRY/CanonicalSerializer.h"
#include "REGISTRY/VisualStyleGenomeCompiler.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::character::VisualGenomeLoader;
using dominus::character::VisualStyleGenome;
using dominus::character::VisualStyleGenomeLoader;
using dominus::registry::CanonicalSerializer;
using dominus::registry::VisualStyleGenomeCompiler;

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

// --- VisualStyleGenomeLoader: strict validation ------------------------------

DOMINUS_TEST(VisualStyleGenomeLoader_LoadsRealUrbanCombatStyle) {
    auto result = VisualStyleGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_visual_style.json");
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(result.value->style_id == "STYLE-URBAN-COMBAT");
    DOMINUS_EXPECT(result.value->name == "Urban Combat");
    DOMINUS_EXPECT(result.value->visual_rules.line_quality == "heavy");
    DOMINUS_EXPECT(result.value->influences.size() == 2);
    DOMINUS_EXPECT(result.value->influences[0] == "boxing");
}

DOMINUS_TEST(VisualStyleGenomeLoader_RejectsMissingStyleIdAndName) {
    auto result = VisualStyleGenomeLoader::LoadFromFile(FixtureDir() / "broken_visual_style.json");
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("style_id") != std::string::npos);
    DOMINUS_EXPECT(result.error.find("name") != std::string::npos);
}

DOMINUS_TEST(VisualStyleGenomeLoader_MissingFileFailsGracefully) {
    auto result = VisualStyleGenomeLoader::LoadFromFile(FixtureDir() / "does_not_exist_visual_style.json");
    DOMINUS_EXPECT(!result.ok);
}

// --- CanonicalSerializer + Compiler: eighth genome type through the pipeline -

DOMINUS_TEST(CanonicalSerializer_VisualStyleGenome_SameValuesProduceSameBytes) {
    auto a = VisualStyleGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_visual_style.json");
    auto b = VisualStyleGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_visual_style.json");
    DOMINUS_EXPECT(CanonicalSerializer::SerializeVisualStyleGenome(*a.value) ==
                    CanonicalSerializer::SerializeVisualStyleGenome(*b.value));
}

DOMINUS_TEST(VisualStyleGenomeCompiler_CompilesDeterministically) {
    auto a = VisualStyleGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_visual_style.json");
    auto b = VisualStyleGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_visual_style.json");
    auto compileA = VisualStyleGenomeCompiler::Compile(*a.value);
    auto compileB = VisualStyleGenomeCompiler::Compile(*b.value);
    DOMINUS_EXPECT(compileA.ok);
    DOMINUS_EXPECT(compileA.hash.size() == 64);
    DOMINUS_EXPECT(compileA.hash == compileB.hash);
}

DOMINUS_TEST(VisualStyleGenomeCompiler_RejectsEmptyStyleId) {
    VisualStyleGenome invalid;
    auto result = VisualStyleGenomeCompiler::Compile(invalid);
    DOMINUS_EXPECT(!result.ok);
}

// --- The real connection: VisualGenome.presence.style_id vs the style genome -

DOMINUS_TEST(VisualStyleGenome_StyleIdMatchesBrooklynsRealVisualGenome) {
    // Not a coincidence of naming -- proving the two systems actually
    // agree: Brooklyn's real visual genome's style_id field and this
    // style genome's own style_id are the same string, loaded from two
    // completely independent files through two completely independent
    // loaders. Same pattern as the CombatStyleGenome<->CombatIdentity
    // cross-check.
    auto visualResult = VisualGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_visual.json");
    auto styleResult = VisualStyleGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_visual_style.json");
    DOMINUS_EXPECT(visualResult.ok);
    DOMINUS_EXPECT(styleResult.ok);
    DOMINUS_EXPECT(visualResult.value->presence.style_id == styleResult.value->style_id);
}

// --- RigBinder integration: VisualStyleGenome now binds to a real entity ----

DOMINUS_TEST(RigBinder_ResolvesVisualStyleGenomeRefIntoBoundComponent) {
    auto fixtureDir = FixtureDir();
    auto loadResult = dominus::core::DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    DOMINUS_EXPECT(loadResult.ok);
    auto bindResult = dominus::character::RigBinder::Bind(*loadResult.value, fixtureDir);
    DOMINUS_EXPECT(bindResult.ok);

    auto* visualStyleComponent = loadResult.value->GetComponent<dominus::character::VisualStyleGenomeComponent>();
    DOMINUS_EXPECT(visualStyleComponent != nullptr);
    DOMINUS_EXPECT(visualStyleComponent->genome.style_id == "STYLE-URBAN-COMBAT");
    DOMINUS_EXPECT(visualStyleComponent->genome.name == "Urban Combat");
}

DOMINUS_TEST(RigBinder_FailsCleanlyWhenVisualStyleGenomeRefIsBroken) {
    auto fixtureDir = FixtureDir();
    dominus::core::MetaBinObject obj("broken_visual_style_test_entity", "0.1.0");
    obj.AddComponent<dominus::core::SkeletonRefComponent>(dominus::core::SkeletonRefComponent{"brooklyn.skel.json"});
    obj.AddComponent<dominus::core::VisualStyleGenomeRefComponent>(
        dominus::core::VisualStyleGenomeRefComponent{"broken_visual_style.json"});

    auto bindResult = dominus::character::RigBinder::Bind(obj, fixtureDir);
    DOMINUS_EXPECT(!bindResult.ok);
    DOMINUS_EXPECT(bindResult.error.find("visual style genome") != std::string::npos);
}
