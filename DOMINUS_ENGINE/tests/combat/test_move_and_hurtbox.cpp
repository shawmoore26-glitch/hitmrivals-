// tests/combat/test_move_and_hurtbox.cpp
#include "CHARACTER/Genome/CombatIdentityLoader.h"
#include "COMBAT/HitSystem/HurtboxLoader.h"
#include "COMBAT/HitSystem/MoveLoader.h"
#include "tests/TestFramework.h"

#include <filesystem>
#include <fstream>

using dominus::character::CombatIdentityLoader;
using dominus::combat::HurtboxLoader;
using dominus::combat::MoveLoader;

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

DOMINUS_TEST(CombatIdentityLoader_LoadsBrooklynFixture) {
    auto result = CombatIdentityLoader::LoadFromFile(FixtureDir() / "brooklyn_combat.json");
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(result.value->style == "psycho_drunken_martial_arts");
    DOMINUS_EXPECT(result.value->range == "close");
    DOMINUS_EXPECT(result.value->counter == "expert");
}

DOMINUS_TEST(MoveLoader_LoadsJabWithFullFrameData) {
    auto result = MoveLoader::LoadFromFile(FixtureDir() / "brooklyn_move_jab.json");
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(result.value->name == "jab");
    DOMINUS_EXPECT(result.value->motion_trigger == "attack");
    DOMINUS_EXPECT(result.value->frames.startup == 5);
    DOMINUS_EXPECT(result.value->frames.active == 4);
    DOMINUS_EXPECT(result.value->frames.recovery == 15);
    DOMINUS_EXPECT(result.value->frames.TotalFrames() == 24);
    DOMINUS_EXPECT(result.value->intent.purpose == "close-range pressure strike");
    DOMINUS_EXPECT(result.value->intent.followups.size() == 2);
    DOMINUS_EXPECT(result.value->hitboxes.size() == 1);
    DOMINUS_EXPECT(result.value->hitboxes[0].bone == "arm_r");
}

DOMINUS_TEST(MoveLoader_DodgeHasNoHitboxes) {
    auto result = MoveLoader::LoadFromFile(FixtureDir() / "brooklyn_move_dodge.json");
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(result.value->hitboxes.empty());
    DOMINUS_EXPECT(result.value->power == 0.0f);
}

DOMINUS_TEST(MoveLoader_RejectsFileMissingName) {
    std::filesystem::path tmp = std::filesystem::temp_directory_path() / "dominus_bad_move.json";
    {
        std::ofstream out(tmp);
        out << R"({"frames":{"startup":1}})";
    }
    auto result = MoveLoader::LoadFromFile(tmp);
    DOMINUS_EXPECT(!result.ok);
    std::filesystem::remove(tmp);
}

DOMINUS_TEST(HurtboxLoader_LoadsBrooklynThreeBoxes) {
    auto result = HurtboxLoader::LoadFromFile(FixtureDir() / "brooklyn_hurtboxes.json");
    DOMINUS_EXPECT(result.ok);
    // 6 boxes since the bone-rig fix -- head/torso/arm_r (original 3)
    // plus arm_l/leg_r/leg_l, so Brooklyn is fully hittable on every
    // real limb, not just his right arm.
    DOMINUS_EXPECT(result.value->boxes.size() == 6);
    bool foundTorso = false;
    for (auto& b : result.value->boxes) {
        if (b.bone == "torso") {
            foundTorso = true;
            DOMINUS_EXPECT(b.radius == 12.0f);
        }
    }
    DOMINUS_EXPECT(foundTorso);
}
