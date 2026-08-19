// tests/combat/test_combo_and_clash.cpp
#include "COMBAT/ComboSystem/ComboEngine.h"
#include "COMBAT/HitSystem/MoveLoader.h"
#include "COMBAT/PhysicsCombat/ClashSystem.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::combat::ClashOutcome;
using dominus::combat::ClashSystem;
using dominus::combat::ComboEngine;
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

DOMINUS_TEST(ComboEngine_CanCancelIntoListedFollowupWithinWindow) {
    auto jab = MoveLoader::LoadFromFile(FixtureDir() / "brooklyn_move_jab.json");
    DOMINUS_EXPECT(jab.ok);
    // jab: startup=5, active=4 -> cancel window opens at frame 5, closes at
    // 5+4+10=19. combo_starter is a listed followup.
    DOMINUS_EXPECT(ComboEngine::CanCancelInto(*jab.value, 10, "combo_starter"));
}

DOMINUS_TEST(ComboEngine_CannotCancelBeforeWindowOpens) {
    auto jab = MoveLoader::LoadFromFile(FixtureDir() / "brooklyn_move_jab.json");
    DOMINUS_EXPECT(!ComboEngine::CanCancelInto(*jab.value, 2, "combo_starter"));  // still in startup
}

DOMINUS_TEST(ComboEngine_CannotCancelAfterWindowCloses) {
    auto jab = MoveLoader::LoadFromFile(FixtureDir() / "brooklyn_move_jab.json");
    DOMINUS_EXPECT(!ComboEngine::CanCancelInto(*jab.value, 25, "combo_starter"));  // past frame 19
}

DOMINUS_TEST(ComboEngine_CannotCancelIntoUnlistedMove) {
    auto jab = MoveLoader::LoadFromFile(FixtureDir() / "brooklyn_move_jab.json");
    DOMINUS_EXPECT(!ComboEngine::CanCancelInto(*jab.value, 10, "dodge"));  // not in jab's followups
}

DOMINUS_TEST(ComboEngine_ChainTracksRecordedHits) {
    ComboEngine engine;
    DOMINUS_EXPECT(engine.ComboCount() == 0);
    engine.RecordHit("jab");
    engine.RecordHit("combo_starter");
    DOMINUS_EXPECT(engine.ComboCount() == 2);
    DOMINUS_EXPECT(engine.Chain()[0] == "jab");
    engine.Reset();
    DOMINUS_EXPECT(engine.ComboCount() == 0);
}

DOMINUS_TEST(ClashSystem_HigherPowerOverpowers) {
    auto result = ClashSystem::Resolve({.power = 50, .speed = 10, .skill = 1.0f}, {.power = 10, .speed = 10, .skill = 1.0f});
    DOMINUS_EXPECT(result.outcome == ClashOutcome::kOverpowerA);
}

DOMINUS_TEST(ClashSystem_EqualScoresCancel) {
    auto result = ClashSystem::Resolve({.power = 20, .speed = 20, .skill = 1.0f}, {.power = 20, .speed = 20, .skill = 1.0f});
    DOMINUS_EXPECT(result.outcome == ClashOutcome::kCancel);
}

DOMINUS_TEST(ClashSystem_CloseScoresRebound) {
    // ~5% apart -- inside the rebound band, outside the cancel band.
    auto result = ClashSystem::Resolve({.power = 21, .speed = 10, .skill = 1.0f}, {.power = 20, .speed = 10, .skill = 1.0f});
    DOMINUS_EXPECT(result.outcome == ClashOutcome::kRebound);
}

DOMINUS_TEST(ClashSystem_SkillMultiplierCanFlipOutcome) {
    // Lower power but much higher skill overtakes a raw-power attacker.
    auto result = ClashSystem::Resolve({.power = 10, .speed = 10, .skill = 3.0f}, {.power = 20, .speed = 10, .skill = 1.0f});
    DOMINUS_EXPECT(result.outcome == ClashOutcome::kOverpowerA);
}
