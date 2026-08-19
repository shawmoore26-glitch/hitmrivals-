// tests/character/test_hitm_game_rules.cpp
// ROADMAP.md Track H Module 4. Fixtures under
// tests/fixtures/hitm_game_rules/ are a real, byte-identical copy of
// HITM's authored data/system/game.json (see that directory's README.md
// for provenance) plus deliberate single-field mutations of it.
#include "CHARACTER/HitmBridge/HitmGameRules.h"
#include "tests/TestFramework.h"

#include <filesystem>
#include <fstream>
#include <sstream>

using dominus::character::hitm::HitmGameRules;
using dominus::core::json::Value;

namespace {

std::filesystem::path FixturePath(const std::string& relative) {
    std::vector<std::filesystem::path> candidates = {
        std::filesystem::path("tests/fixtures/hitm_game_rules") / relative,
        std::filesystem::path("../tests/fixtures/hitm_game_rules") / relative,
        std::filesystem::path("../../tests/fixtures/hitm_game_rules") / relative,
    };
    for (auto& c : candidates) {
        if (std::filesystem::exists(c)) return c;
    }
    throw std::runtime_error("Fixture not found: " + relative);
}

}  // namespace

// --- Real data ------------------------------------------------------------

DOMINUS_TEST(HitmGameRules_ImportsRealGameJson) {
    auto result = HitmGameRules::Import(FixturePath("game.json"));
    DOMINUS_EXPECT(result.ok);
    auto& g = *result.value;

    DOMINUS_EXPECT(g.Roster().size() == 3);
    DOMINUS_EXPECT(g.Roster()[0] == "brooklyn");
    DOMINUS_EXPECT(g.Roster()[1] == "static");
    DOMINUS_EXPECT(g.Roster()[2] == "rocket");
}

DOMINUS_TEST(HitmGameRules_ViewAndPhysicsMatchRealData) {
    auto g = *HitmGameRules::Import(FixturePath("game.json")).value;

    DOMINUS_EXPECT(g.View().w == 1060.0);
    DOMINUS_EXPECT(g.View().h == 600.0);

    DOMINUS_EXPECT(g.Physics().ground == 538.0);
    DOMINUS_EXPECT(g.Physics().gravity == 0.6);
    DOMINUS_EXPECT(g.Physics().wall_l == 64.0);
    DOMINUS_EXPECT(g.Physics().wall_r == 996.0);
    DOMINUS_EXPECT(g.Physics().walk_speed == 4.4);
    DOMINUS_EXPECT(g.Physics().jump_vel == -13.6);
    DOMINUS_EXPECT(g.Physics().dash_speed == 10.5);
    DOMINUS_EXPECT(g.Physics().dash_frames == 9.0);
}

DOMINUS_TEST(HitmGameRules_MeterAndCombatMatchRealData) {
    auto g = *HitmGameRules::Import(FixturePath("game.json")).value;

    DOMINUS_EXPECT(g.Meter().max == 100.0);
    DOMINUS_EXPECT(g.Meter().break_cost == 50.0);
    DOMINUS_EXPECT(g.Meter().perfect_guard_gain == 12.0);

    DOMINUS_EXPECT(g.Combat().chip_mult == 0.14);
    DOMINUS_EXPECT(g.Combat().counter_dmg_mult == 1.25);
    DOMINUS_EXPECT(g.Combat().hitstop_light == 4.0);
    DOMINUS_EXPECT(g.Combat().hitstop_heavy == 7.0);
    DOMINUS_EXPECT(g.Combat().hitstop_counter == 11.0);
    DOMINUS_EXPECT(g.Combat().input_buffer_frames == 8.0);
}

DOMINUS_TEST(HitmGameRules_RoundsAndSpriteMatchRealData) {
    auto g = *HitmGameRules::Import(FixturePath("game.json")).value;

    DOMINUS_EXPECT(g.Rounds().to_win == 2.0);
    DOMINUS_EXPECT(g.Rounds().timer_seconds == 60.0);
    DOMINUS_EXPECT(g.Sprite().display_height == 225.0);
}

// --- Losslessness: import -> export must reproduce the exact source tree ---

DOMINUS_TEST(HitmGameRules_ExportRoundTripsWithZeroLoss) {
    auto g = *HitmGameRules::Import(FixturePath("game.json")).value;

    std::ifstream in(FixturePath("game.json"), std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    Value sourceParsed = Value::Parse(ss.str());

    DOMINUS_EXPECT(sourceParsed.Dump() == g.ToJson().Dump());

    Value reparsed = Value::Parse(g.ToJson().Dump());
    DOMINUS_EXPECT(reparsed.Dump() == g.ToJson().Dump());
}

// --- Deliberate-break: every real-world corruption mode must fail loud ---

DOMINUS_TEST(HitmGameRules_Break_MissingFile_Fails) {
    auto result = HitmGameRules::Import(FixturePath("broken_missing_file") / "game.json");
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(!result.error.empty());
}

DOMINUS_TEST(HitmGameRules_Break_MalformedJson_Fails) {
    auto result = HitmGameRules::Import(FixturePath("broken_malformed_json/game.json"));
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("parse error") != std::string::npos);
}

DOMINUS_TEST(HitmGameRules_Break_MissingCombatSection_Fails) {
    auto result = HitmGameRules::Import(FixturePath("broken_missing_combat_section/game.json"));
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("combat") != std::string::npos);
}

DOMINUS_TEST(HitmGameRules_Break_RosterWrongType_Fails) {
    auto result = HitmGameRules::Import(FixturePath("broken_roster_wrong_type/game.json"));
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("roster") != std::string::npos);
}

DOMINUS_TEST(HitmGameRules_Break_EmptyRoster_Fails) {
    auto result = HitmGameRules::Import(FixturePath("broken_empty_roster/game.json"));
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("roster") != std::string::npos);
}

DOMINUS_TEST(HitmGameRules_Break_RosterNonStringElement_Fails) {
    auto result = HitmGameRules::Import(FixturePath("broken_roster_non_string_element/game.json"));
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("roster") != std::string::npos);
}

DOMINUS_TEST(HitmGameRules_Break_MissingGravity_Fails) {
    auto result = HitmGameRules::Import(FixturePath("broken_missing_gravity/game.json"));
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("gravity") != std::string::npos);
}

DOMINUS_TEST(HitmGameRules_Break_MeterMaxWrongType_Fails) {
    auto result = HitmGameRules::Import(FixturePath("broken_meter_max_wrong_type/game.json"));
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("max") != std::string::npos);
}

DOMINUS_TEST(HitmGameRules_Break_FailedImportDoesNotThrow) {
    bool threw = false;
    bool ok = true;
    try {
        auto result = HitmGameRules::Import(FixturePath("broken_missing_gravity/game.json"));
        ok = result.ok;
    } catch (...) {
        threw = true;
    }
    DOMINUS_EXPECT(!threw);
    DOMINUS_EXPECT(!ok);
}
