// tests/character/test_hitm_identity_importer.cpp
// ROADMAP.md Track H Module 1. Fixtures under tests/fixtures/hitm_identity/
// are real, byte-identical copies of the authored HITM Rivals data (see
// that directory's README.md for provenance) plus deliberate-break
// variants built from them.
#include "CHARACTER/HitmBridge/HitmIdentityImporter.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::character::hitm::HitmIdentityImporter;
using dominus::character::hitm::HitmIdentityRecord;

namespace {

std::filesystem::path FixtureDir(const std::string& name) {
    std::vector<std::filesystem::path> candidates = {
        std::filesystem::path("tests/fixtures/hitm_identity") / name,
        std::filesystem::path("../tests/fixtures/hitm_identity") / name,
        std::filesystem::path("../../tests/fixtures/hitm_identity") / name,
    };
    for (auto& c : candidates) {
        if (std::filesystem::exists(c)) return c;
    }
    throw std::runtime_error("Fixture dir not found: " + name);
}

}  // namespace

DOMINUS_TEST(HitmIdentityImporter_ImportsRealBrooklyn) {
    auto result = HitmIdentityImporter::Import(FixtureDir("brooklyn"));
    DOMINUS_EXPECT(result.ok);
    auto& rec = *result.value;
    DOMINUS_EXPECT(rec.fighter_id == "brooklyn");

    // Real, authored values -- not derived, not invented. Cross-checked
    // against HITM_INTEGRATION_AUDIT.md's quoted excerpts of the same file.
    DOMINUS_EXPECT(rec.identity.Get("name")->AsString() == "Brooklyn");
    DOMINUS_EXPECT(rec.identity.Get("faction")->AsString() == "RENEGADES");
    DOMINUS_EXPECT(rec.combat_genome.Get("archetype")->AsString() == "psycho_drunken_martial_artist");
    DOMINUS_EXPECT(rec.design.Get("character")->AsString() == "brooklyn");

    // Nested access -- proves the record is a real, navigable tree, not a
    // flattened summary. dna.confidence == 92 straight from character_dna.json.
    const auto* dna = rec.character_dna.Get("dna");
    DOMINUS_EXPECT(dna != nullptr);
    DOMINUS_EXPECT(dna->Get("confidence")->AsNumber() == 92.0);

    // The read_engine mechanic -- the exact thing CombatIdentity cannot
    // represent (HITM_INTEGRATION_AUDIT.md section 2). Proves it survived
    // the import losslessly.
    const auto* readEngine = rec.combat_genome.Get("read_engine");
    DOMINUS_EXPECT(readEngine != nullptr);
    DOMINUS_EXPECT(readEngine->Get("max_reads")->AsNumber() == 5.0);
    const auto* tiers = readEngine->Get("tiers");
    DOMINUS_EXPECT(tiers != nullptr && tiers->IsArray());
    DOMINUS_EXPECT(tiers->AsArray().size() == 6);

    // The em-dash-bearing _law field -- proves MiniJson's \u fix (Module 0)
    // and this importer compose correctly on real authored text.
    const auto* defense = rec.combat_genome.Get("defense_profile");
    DOMINUS_EXPECT(defense != nullptr);
    const std::string& law = defense->Get("_law")->AsString();
    DOMINUS_EXPECT(law.find("\xE2\x80\x94") != std::string::npos);
}

DOMINUS_TEST(HitmIdentityImporter_ImportsRealRocket) {
    // A second real fighter -- proves this path is general, not
    // Brooklyn-special-cased.
    auto result = HitmIdentityImporter::Import(FixtureDir("rocket"));
    DOMINUS_EXPECT(result.ok);
    auto& rec = *result.value;
    DOMINUS_EXPECT(rec.fighter_id == "rocket");
    DOMINUS_EXPECT(rec.identity.Get("name")->AsString() == "Rocket");
    DOMINUS_EXPECT(rec.identity.Get("accent")->AsString() == "#3ecf6e");
    // Rocket's real combat_genome has no read_engine (Brooklyn-only
    // mechanic) -- the importer must not require it.
    DOMINUS_EXPECT(rec.combat_genome.Get("read_engine") == nullptr);
    DOMINUS_EXPECT(rec.combat_genome.Get("archetype") != nullptr);
}

DOMINUS_TEST(HitmIdentityImporter_ImportsRealStatic) {
    // A third real fighter -- full roster coverage, not two-out-of-three.
    auto result = HitmIdentityImporter::Import(FixtureDir("static"));
    DOMINUS_EXPECT(result.ok);
    auto& rec = *result.value;
    DOMINUS_EXPECT(rec.fighter_id == "static");
    DOMINUS_EXPECT(rec.identity.Get("name")->AsString() == "Static");
}

// --- Deliberate-break: every real-world corruption mode must fail loud ---

DOMINUS_TEST(HitmIdentityImporter_Break_NonexistentDirectory_Fails) {
    auto result = HitmIdentityImporter::Import("tests/fixtures/hitm_identity/does_not_exist");
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(!result.error.empty());
}

DOMINUS_TEST(HitmIdentityImporter_Break_MissingFile_Fails) {
    auto result = HitmIdentityImporter::Import(FixtureDir("broken_missing_file"));
    DOMINUS_EXPECT(!result.ok);
    // Must name the actual missing file, not a generic failure.
    DOMINUS_EXPECT(result.error.find("combat_genome.json") != std::string::npos);
}

DOMINUS_TEST(HitmIdentityImporter_Break_MalformedJson_Fails) {
    auto result = HitmIdentityImporter::Import(FixtureDir("broken_malformed_json"));
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("identity.json") != std::string::npos);
}

DOMINUS_TEST(HitmIdentityImporter_Break_MissingRequiredKey_Fails) {
    auto result = HitmIdentityImporter::Import(FixtureDir("broken_missing_key"));
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("sprite") != std::string::npos);
}

DOMINUS_TEST(HitmIdentityImporter_Break_IdMismatch_Fails) {
    // identity.json says "rocket" while the directory is
    // broken_id_mismatch -- must be caught, not silently imported as
    // either fighter.
    auto result = HitmIdentityImporter::Import(FixtureDir("broken_id_mismatch"));
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("rocket") != std::string::npos);
}
