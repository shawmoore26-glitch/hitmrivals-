// tests/character/test_hitm_combat_genome.cpp
// ROADMAP.md Track H Module 2. Built on top of Module 1's
// HitmIdentityImporter (tests/character/test_hitm_identity_importer.cpp)
// -- every fixture here is either the real byte-identical HITM data or a
// deliberate single-field mutation of it (see
// tests/fixtures/hitm_identity/README.md "Module 2" section for exact
// provenance of each broken_genome_* fixture).
#include "CHARACTER/HitmBridge/HitmCombatGenome.h"
#include "CHARACTER/HitmBridge/HitmIdentityImporter.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::character::hitm::HitmCombatGenome;
using dominus::character::hitm::HitmIdentityImporter;
using dominus::character::hitm::HitmIdentityRecord;
using dominus::core::json::Value;

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

// Imports via the real Module 1 path -- Module 2 never reads a file
// directly, so a test that bypassed this would not be proving what
// Module 2 actually does.
HitmIdentityRecord ImportOrThrow(const std::string& fighter) {
    auto result = HitmIdentityImporter::Import(FixtureDir(fighter));
    if (!result.ok) throw std::runtime_error("test setup: import failed for " + fighter + ": " + result.error);
    return *result.value;
}

}  // namespace

// --- Real data: Brooklyn (has read_engine) -----------------------------

DOMINUS_TEST(HitmCombatGenome_BuildsFromRealBrooklyn) {
    auto record = ImportOrThrow("brooklyn");
    auto result = HitmCombatGenome::FromRecord(record);
    DOMINUS_EXPECT(result.ok);
    auto& g = *result.value;

    DOMINUS_EXPECT(g.FighterId() == "brooklyn");
    DOMINUS_EXPECT(g.Archetype() == "psycho_drunken_martial_artist");
    DOMINUS_EXPECT(g.Philosophy() == "Volume beats weight. He wins by never letting your turn begin.");
    DOMINUS_EXPECT(g.AiIntent().size() == 5);
    DOMINUS_EXPECT(g.AiIntent()[0] == "pressure");
    DOMINUS_EXPECT(g.CombatVerbs().size() == 6);
}

DOMINUS_TEST(HitmCombatGenome_BrooklynProfilesMatchRealData) {
    auto record = ImportOrThrow("brooklyn");
    auto g = *HitmCombatGenome::FromRecord(record).value;

    DOMINUS_EXPECT(g.Rhythm().categorical.value() == "Metronomic");
    DOMINUS_EXPECT(g.Weight().inertia.value() == 0.28);
    DOMINUS_EXPECT(g.Risk().commitment.value() == "low");
    DOMINUS_EXPECT(g.Range().dominant.value() == 76.0);
    DOMINUS_EXPECT(g.Impact().hitstop.value() == "light");

    // The exact numeric field the audit/roadmap called out by name.
    DOMINUS_EXPECT(g.Defense().block_preference.has_value());
    DOMINUS_EXPECT(g.Defense().block_preference.value() == 0.18);
    // Brooklyn-only inner fields on defense_profile -- Rocket/Static lack these.
    DOMINUS_EXPECT(g.Defense().law.has_value());
    DOMINUS_EXPECT(g.Defense().law->find("VOLUME 18") != std::string::npos);
    DOMINUS_EXPECT(g.Defense().from.has_value());
    DOMINUS_EXPECT(g.Defense().from->size() == 3);
    DOMINUS_EXPECT(g.Defense().not_allowed.has_value());
}

DOMINUS_TEST(HitmCombatGenome_BrooklynReadEngineIsExplicitAndComplete) {
    auto record = ImportOrThrow("brooklyn");
    auto g = *HitmCombatGenome::FromRecord(record).value;

    DOMINUS_EXPECT(g.HasReadEngine());
    const auto* re = g.GetReadEngine();
    DOMINUS_EXPECT(re != nullptr);
    DOMINUS_EXPECT(re->max_reads == 5.0);
    DOMINUS_EXPECT(re->gain_on.size() == 4);
    DOMINUS_EXPECT(re->lose_on.size() == 3);
    DOMINUS_EXPECT(re->lose_law.has_value());
    DOMINUS_EXPECT(re->tiers.size() == 6);

    // Tier 0: "This Is Fun", damage_mult 1.0 -- his real balance point.
    DOMINUS_EXPECT(re->tiers[0].reads == 0.0);
    DOMINUS_EXPECT(re->tiers[0].name == "This Is Fun");
    DOMINUS_EXPECT(re->tiers[0].damage_mult == 1.0);

    // Tier 5: "Perfect Hunter", damage_mult 1.42 -- the ceiling.
    DOMINUS_EXPECT(re->tiers[5].reads == 5.0);
    DOMINUS_EXPECT(re->tiers[5].name == "Perfect Hunter");
    DOMINUS_EXPECT(re->tiers[5].damage_mult == 1.42);
    DOMINUS_EXPECT(re->tiers[5].note.has_value());

    // Tier 1 genuinely has no "note" in the source -- must be absent, not "".
    DOMINUS_EXPECT(!re->tiers[1].note.has_value());

    DOMINUS_EXPECT(re->decay.frames == 420.0);
    DOMINUS_EXPECT(re->decay.amount == 1.0);
    DOMINUS_EXPECT(re->decay.note.has_value());
}

// --- Real data: Rocket and Static (no read_engine) ----------------------

DOMINUS_TEST(HitmCombatGenome_BuildsFromRealRocket_NoReadEngine) {
    auto record = ImportOrThrow("rocket");
    auto result = HitmCombatGenome::FromRecord(record);
    DOMINUS_EXPECT(result.ok);
    auto& g = *result.value;

    DOMINUS_EXPECT(g.FighterId() == "rocket");
    DOMINUS_EXPECT(g.Archetype() == "heavy_bruiser");
    DOMINUS_EXPECT(g.Weight().categorical.value() == "Heavy");
    DOMINUS_EXPECT(g.Weight().inertia.value() == 0.78);
    DOMINUS_EXPECT(g.Defense().block_preference.value() == 0.4);

    // Rocket genuinely has no read_engine -- must be nullptr, not a
    // default-constructed empty one. A default-filled "empty read engine"
    // would be exactly the invented-value failure mode this module must
    // not have.
    DOMINUS_EXPECT(!g.HasReadEngine());
    DOMINUS_EXPECT(g.GetReadEngine() == nullptr);

    // Rocket's defense_profile also genuinely has no "_law"/"from"/"not" --
    // Brooklyn-only fields, must not leak a value across fighters.
    DOMINUS_EXPECT(!g.Defense().law.has_value());
    DOMINUS_EXPECT(!g.Defense().from.has_value());
}

DOMINUS_TEST(HitmCombatGenome_BuildsFromRealStatic_NoReadEngine) {
    auto record = ImportOrThrow("static");
    auto result = HitmCombatGenome::FromRecord(record);
    DOMINUS_EXPECT(result.ok);
    auto& g = *result.value;

    DOMINUS_EXPECT(g.FighterId() == "static");
    DOMINUS_EXPECT(g.Archetype() == "zoner_controller");
    DOMINUS_EXPECT(g.Range().band.value() == "long");
    DOMINUS_EXPECT(g.Range().dominant.value() == 105.0);
    DOMINUS_EXPECT(!g.HasReadEngine());
}

// --- Losslessness: import -> export must reproduce the exact source tree ---

DOMINUS_TEST(HitmCombatGenome_ExportRoundTripsBrooklynWithZeroLoss) {
    auto record = ImportOrThrow("brooklyn");
    auto g = *HitmCombatGenome::FromRecord(record).value;

    // The real proof: dump the exact combat_genome tree Module 1 parsed,
    // dump what HitmCombatGenome::ToJson() returns, and require the two
    // canonical serializations to be byte-identical. If any code path in
    // FromRecord had mutated, dropped, or reconstructed the tree instead
    // of holding it verbatim, this would catch it -- unlike a test that
    // only checks the typed fields it already knows to expect.
    std::string originalDump = record.combat_genome.Dump();
    std::string exportedDump = g.ToJson().Dump();
    DOMINUS_EXPECT(originalDump == exportedDump);

    // Same proof again through a full parse/dump/reparse/dump cycle --
    // confirms MiniJson's own serialization is stable, not just that we
    // held a reference to the same object.
    Value reparsed = Value::Parse(exportedDump);
    DOMINUS_EXPECT(reparsed.Dump() == exportedDump);

    // Concretely: fields this class does NOT model explicitly (Brooklyn's
    // "identity_statement", "strength", "weakness") must still be present
    // in the exported tree.
    DOMINUS_EXPECT(g.ToJson().Get("identity_statement") != nullptr);
    DOMINUS_EXPECT(g.ToJson().Get("strength") != nullptr);
    DOMINUS_EXPECT(g.ToJson().Get("weakness") != nullptr);
}

DOMINUS_TEST(HitmCombatGenome_ExportRoundTripsRocketUnknownFieldsPreserved) {
    // Rocket has fields Brooklyn's genome doesn't (archetype_line,
    // forbidden, martial_foundation) -- none of them are modeled by any
    // typed accessor on this class. They must still survive export.
    auto record = ImportOrThrow("rocket");
    auto g = *HitmCombatGenome::FromRecord(record).value;

    DOMINUS_EXPECT(record.combat_genome.Dump() == g.ToJson().Dump());
    DOMINUS_EXPECT(g.ToJson().Get("archetype_line") != nullptr);
    DOMINUS_EXPECT(g.ToJson().Get("forbidden") != nullptr);
    DOMINUS_EXPECT(g.ToJson().Get("martial_foundation") != nullptr);
}

// --- Deliberate-break: malformed genome shapes must fail loud -----------

DOMINUS_TEST(HitmCombatGenome_Break_ArchetypeWrongType_Fails) {
    auto record = ImportOrThrow("broken_genome_archetype_wrong_type");
    auto result = HitmCombatGenome::FromRecord(record);
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("archetype") != std::string::npos);
}

DOMINUS_TEST(HitmCombatGenome_Break_AiIntentNotArray_Fails) {
    auto record = ImportOrThrow("broken_genome_ai_intent_not_array");
    auto result = HitmCombatGenome::FromRecord(record);
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("ai_intent") != std::string::npos);
}

DOMINUS_TEST(HitmCombatGenome_Break_BlockPreferenceWrongType_Fails) {
    auto record = ImportOrThrow("broken_genome_block_preference_wrong_type");
    auto result = HitmCombatGenome::FromRecord(record);
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("blockPreference") != std::string::npos);
}

DOMINUS_TEST(HitmCombatGenome_Break_ReadEngineMissingTiers_Fails) {
    auto record = ImportOrThrow("broken_genome_read_engine_missing_tiers");
    auto result = HitmCombatGenome::FromRecord(record);
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("tiers") != std::string::npos);
}

DOMINUS_TEST(HitmCombatGenome_Break_ReadEngineTierMissingField_Fails) {
    auto record = ImportOrThrow("broken_genome_read_engine_tier_missing_field");
    auto result = HitmCombatGenome::FromRecord(record);
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("damage_mult") != std::string::npos);
}

DOMINUS_TEST(HitmCombatGenome_Break_ReadEngineBadDecay_Fails) {
    auto record = ImportOrThrow("broken_genome_read_engine_bad_decay");
    auto result = HitmCombatGenome::FromRecord(record);
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("decay") != std::string::npos);
}

DOMINUS_TEST(HitmCombatGenome_Break_FailedFromRecordDoesNotThrow) {
    // Result-based failure, never an uncaught exception escaping
    // FromRecord -- confirmed directly rather than assumed from the other
    // Break_ tests all happening to catch cleanly.
    auto record = ImportOrThrow("broken_genome_archetype_wrong_type");
    bool threw = false;
    bool ok = true;
    try {
        auto result = HitmCombatGenome::FromRecord(record);
        ok = result.ok;
    } catch (...) {
        threw = true;
    }
    DOMINUS_EXPECT(!threw);
    DOMINUS_EXPECT(!ok);
}
