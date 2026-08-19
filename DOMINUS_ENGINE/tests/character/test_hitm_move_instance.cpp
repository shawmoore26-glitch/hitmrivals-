// tests/character/test_hitm_move_instance.cpp
// ROADMAP.md Track H Module 5A. Reuses the real Brooklyn/Rocket fixtures
// from tests/fixtures/hitm_identity/ (Module 1's provenance applies here
// too).
#include "CHARACTER/HitmBridge/HitmIdentityImporter.h"
#include "CHARACTER/HitmBridge/HitmMoveInstance.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::character::hitm::HitmHitstopCategory;
using dominus::character::hitm::HitmIdentityImporter;
using dominus::character::hitm::HitmIdentityRecord;
using dominus::character::hitm::HitmMoveInstance;

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

HitmIdentityRecord ImportOrThrow(const std::string& fighter) {
    auto result = HitmIdentityImporter::Import(FixtureDir(fighter));
    if (!result.ok) throw std::runtime_error("test setup: import failed for " + fighter + ": " + result.error);
    return *result.value;
}

}  // namespace

DOMINUS_TEST(HitmMoveInstance_ExtractsRealBrooklynSpecial) {
    auto record = ImportOrThrow("brooklyn");
    auto result = HitmMoveInstance::Extract(record, "special");
    DOMINUS_EXPECT(result.ok);
    auto& move = *result.value;

    DOMINUS_EXPECT(move.move_def.name == "DRUNKEN LAUNCHER KICK");
    DOMINUS_EXPECT(move.input_token == "S");
    DOMINUS_EXPECT(move.move_def.frames.startup == 14);
    DOMINUS_EXPECT(move.move_def.frames.active == 4);
    DOMINUS_EXPECT(move.move_def.frames.recovery == 18);
    DOMINUS_EXPECT(move.move_def.power == 62.0f);
    DOMINUS_EXPECT(move.hitstun_frames == 34);
    DOMINUS_EXPECT(move.blockstun_frames == 13);
    DOMINUS_EXPECT(move.meter_gain == 14);
    DOMINUS_EXPECT(move.hitstop_category == HitmHitstopCategory::kHeavy);
    DOMINUS_EXPECT(move.range == 88.0);
    DOMINUS_EXPECT(move.height == 132.0);
    DOMINUS_EXPECT(move.cooldown_frames == 70);
}

DOMINUS_TEST(HitmMoveInstance_Break_RocketSpecialHasNoBlockstunOrRange_Fails) {
    // A real, evidenced finding (see HitmMoveInstance.h's top comment):
    // Rocket's real "special" ("Ghost Dash") is a rush-type move with no
    // "blockstun" and no flat "range"/"height" at all -- it has a "rush"
    // sub-object instead. This extractor correctly refuses it rather
    // than silently defaulting the missing fields.
    auto record = ImportOrThrow("rocket");
    auto result = HitmMoveInstance::Extract(record, "special");
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("blockstun") != std::string::npos);
}

DOMINUS_TEST(HitmMoveInstance_Break_StaticSpecialHasNoBlockstunOrHitstop_Fails) {
    // A second, differently-shaped real gap: Static's real "special"
    // ("Live Wire") has range/height but no "blockstun" and no
    // "hitstop" at all.
    auto record = ImportOrThrow("static");
    auto result = HitmMoveInstance::Extract(record, "special");
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("blockstun") != std::string::npos);
}

DOMINUS_TEST(HitmMoveInstance_Break_UnknownMoveKey_Fails) {
    auto record = ImportOrThrow("brooklyn");
    auto result = HitmMoveInstance::Extract(record, "does_not_exist");
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("does_not_exist") != std::string::npos);
}

DOMINUS_TEST(HitmMoveInstance_Break_UnrecognizedHitstopCategory_Fails) {
    // Brooklyn's real "blockbuster" move -- confirm whatever its real
    // hitstop string is either parses or, if this test ever needs to
    // change because HITM adds a new category, it fails LOUD rather than
    // silently defaulting. This test targets the parser behavior via a
    // synthetic bad record built from real data with one field swapped,
    // not a real fixture (a truly malformed signature.json isn't part of
    // Module 1's byte-identical corpus).
    auto record = ImportOrThrow("brooklyn");
    // Directly mutate the in-memory record's signature tree -- still real
    // Brooklyn data everywhere else, one field deliberately corrupted.
    record.signature["moves"]["special"]["hitstop"] = dominus::core::json::Value(std::string("earthquake"));
    auto result = HitmMoveInstance::Extract(record, "special");
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("earthquake") != std::string::npos);
}
