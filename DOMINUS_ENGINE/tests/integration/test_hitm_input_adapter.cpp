// tests/integration/test_hitm_input_adapter.cpp
// Track H Phase 5C -- the input adapter, proven end to end:
//
//   Keyboard / Gamepad -> Input Adapter -> HitmInputCommand
//     -> HitmMatch::AdvanceFrame() -> existing proven combat simulation
//
// Two layers tested separately (see HitmInputAdapter.h's own header
// comment): the pure priority-resolution logic (TranslateRawInput), and
// the real key/button binding tables (ReadRawInput). The final test
// proves the whole chain by feeding a translated command into a real
// HitmMatch and confirming the already-proven simulation actually
// responds -- not a mock, the real HitmMatch::AdvanceFrame this track
// closed in Phase 3.
#include "CHARACTER/HitmBridge/HitmCombatGenome.h"
#include "CHARACTER/HitmBridge/HitmGameRules.h"
#include "CHARACTER/HitmBridge/HitmIdentityImporter.h"
#include "CHARACTER/HitmBridge/HitmInputAdapter.h"
#include "CHARACTER/HitmBridge/HitmMatch.h"
#include "tests/TestFramework.h"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <vector>

using dominus::character::hitm::HitmCombatGenome;
using dominus::character::hitm::HitmGameRules;
using dominus::character::hitm::HitmIdentityImporter;
using dominus::character::hitm::HitmIdentityRecord;
using dominus::character::hitm::HitmInputCommand;
using dominus::character::hitm::HitmMatch;
using dominus::character::hitm::HitmRawInputState;
using dominus::character::hitm::ReadRawInput;
using dominus::character::hitm::TranslateRawInput;
using dominus::character::hitm::kGamepadButtons;
using dominus::character::hitm::kPlayerOneKeyboard;
using dominus::character::hitm::kPlayerTwoKeyboard;

namespace {

std::filesystem::path FindDir(const std::filesystem::path& rel) {
    std::vector<std::filesystem::path> candidates = {rel, std::filesystem::path("..") / rel,
                                                       std::filesystem::path("../..") / rel};
    for (auto& c : candidates)
        if (std::filesystem::exists(c)) return c;
    throw std::runtime_error("Fixture dir not found: " + rel.string());
}
HitmIdentityRecord RealIdentity(const std::string& fighter) {
    auto r = HitmIdentityImporter::Import(FindDir(std::filesystem::path("tests/fixtures/hitm_identity") / fighter));
    if (!r.ok) throw std::runtime_error("test setup: " + r.error);
    return *r.value;
}
HitmCombatGenome RealGenome(const HitmIdentityRecord& record) {
    auto r = HitmCombatGenome::FromRecord(record);
    if (!r.ok) throw std::runtime_error("test setup: " + r.error);
    return std::move(*r.value);
}
HitmGameRules RealRules() {
    auto r = HitmGameRules::Import(FindDir("tests/fixtures/hitm_game_rules/game.json"));
    if (!r.ok) throw std::runtime_error("test setup: " + r.error);
    return std::move(*r.value);
}
HitmMatch MakeBrooklynVsRocketMatch() {
    auto brooklynIdentity = RealIdentity("brooklyn");
    auto brooklynGenome = RealGenome(brooklynIdentity);
    auto rocketIdentity = RealIdentity("rocket");
    auto rocketGenome = RealGenome(rocketIdentity);
    auto rules = RealRules();
    auto result = HitmMatch::Create(brooklynIdentity, brooklynGenome, rocketIdentity, rocketGenome, rules);
    if (!result.ok) throw std::runtime_error("test setup: " + result.error);
    return std::move(*result.value);
}

// A real, in-memory "which keys are down" fake -- stands in for a real
// GLFWwindow's key state without depending on GLFW at all. Exactly the
// shape `ReadRawInput`'s own `isPressed` callable expects.
std::function<bool(int)> FakeKeysDown(std::vector<int> down) {
    return [down](int code) { return std::find(down.begin(), down.end(), code) != down.end(); };
}

}  // namespace

// --- 1. TranslateRawInput: the real, disclosed priority order ------------

DOMINUS_TEST(HitmInputAdapter_Translate_AllFalse_IsNeutral) {
    DOMINUS_EXPECT(TranslateRawInput(HitmRawInputState{}) == HitmInputCommand::kNeutral);
}

DOMINUS_TEST(HitmInputAdapter_Translate_LeftAlone_IsLeft) {
    HitmRawInputState raw;
    raw.left = true;
    DOMINUS_EXPECT(TranslateRawInput(raw) == HitmInputCommand::kLeft);
}

DOMINUS_TEST(HitmInputAdapter_Translate_RightAlone_IsRight) {
    HitmRawInputState raw;
    raw.right = true;
    DOMINUS_EXPECT(TranslateRawInput(raw) == HitmInputCommand::kRight);
}

DOMINUS_TEST(HitmInputAdapter_Translate_LeftAndRightTogether_CancelsToNeutral) {
    HitmRawInputState raw;
    raw.left = true;
    raw.right = true;
    DOMINUS_EXPECT(TranslateRawInput(raw) == HitmInputCommand::kNeutral);
}

DOMINUS_TEST(HitmInputAdapter_Translate_UpAlone_IsJump) {
    HitmRawInputState raw;
    raw.up = true;
    DOMINUS_EXPECT(TranslateRawInput(raw) == HitmInputCommand::kJump);
}

DOMINUS_TEST(HitmInputAdapter_Translate_UpWithLeft_JumpTakesPriorityOverMovement) {
    HitmRawInputState raw;
    raw.up = true;
    raw.left = true;
    DOMINUS_EXPECT(TranslateRawInput(raw) == HitmInputCommand::kJump);
}

DOMINUS_TEST(HitmInputAdapter_Translate_BlockAlone_IsBlock) {
    HitmRawInputState raw;
    raw.block = true;
    DOMINUS_EXPECT(TranslateRawInput(raw) == HitmInputCommand::kBlock);
}

DOMINUS_TEST(HitmInputAdapter_Translate_BlockWithMovement_BlockWins) {
    HitmRawInputState raw;
    raw.block = true;
    raw.left = true;
    DOMINUS_EXPECT(TranslateRawInput(raw) == HitmInputCommand::kBlock);
}

DOMINUS_TEST(HitmInputAdapter_Translate_BlockWithSpecial_BlockWinsMatchingRealEngineOrder) {
    // Real CombatSystem.js checks input.down FIRST and returns
    // immediately -- block preempts even a special attempt.
    HitmRawInputState raw;
    raw.block = true;
    raw.special = true;
    DOMINUS_EXPECT(TranslateRawInput(raw) == HitmInputCommand::kBlock);
}

DOMINUS_TEST(HitmInputAdapter_Translate_SpecialAlone_IsSpecial) {
    HitmRawInputState raw;
    raw.special = true;
    DOMINUS_EXPECT(TranslateRawInput(raw) == HitmInputCommand::kSpecial);
}

DOMINUS_TEST(HitmInputAdapter_Translate_AttackAlone_AlsoResolvesToSpecial_DisclosedAlias) {
    // See HitmInputAdapter.h's top comment: HitmInputCommand has exactly
    // one real attack-type command today. Real, disclosed, not hidden.
    HitmRawInputState raw;
    raw.attack = true;
    DOMINUS_EXPECT(TranslateRawInput(raw) == HitmInputCommand::kSpecial);
}

DOMINUS_TEST(HitmInputAdapter_Translate_AttackAndSpecialTogether_StillJustOneSpecial) {
    HitmRawInputState raw;
    raw.attack = true;
    raw.special = true;
    DOMINUS_EXPECT(TranslateRawInput(raw) == HitmInputCommand::kSpecial);
}

DOMINUS_TEST(HitmInputAdapter_Translate_SpecialWithMovement_SpecialTakesPriority) {
    HitmRawInputState raw;
    raw.special = true;
    raw.right = true;
    DOMINUS_EXPECT(TranslateRawInput(raw) == HitmInputCommand::kSpecial);
}

DOMINUS_TEST(HitmInputAdapter_Translate_SpecialWithUp_SpecialTakesPriorityOverJump) {
    HitmRawInputState raw;
    raw.special = true;
    raw.up = true;
    DOMINUS_EXPECT(TranslateRawInput(raw) == HitmInputCommand::kSpecial);
}

// --- 2. ReadRawInput: the real key/button binding tables ------------------

DOMINUS_TEST(HitmInputAdapter_ReadRawInput_PlayerOneKeyboard_ReadsRealBoundKeys) {
    // A held (left) + W held (up), nothing else.
    auto isPressed = FakeKeysDown({kPlayerOneKeyboard.left, kPlayerOneKeyboard.up});
    HitmRawInputState raw = ReadRawInput(kPlayerOneKeyboard, isPressed);
    DOMINUS_EXPECT(raw.left == true);
    DOMINUS_EXPECT(raw.up == true);
    DOMINUS_EXPECT(raw.right == false);
    DOMINUS_EXPECT(raw.attack == false);
    DOMINUS_EXPECT(raw.block == false);
    DOMINUS_EXPECT(raw.special == false);
}

DOMINUS_TEST(HitmInputAdapter_ReadRawInput_PlayerTwoKeyboard_ReadsRealBoundKeys) {
    auto isPressed = FakeKeysDown({kPlayerTwoKeyboard.block, kPlayerTwoKeyboard.special});
    HitmRawInputState raw = ReadRawInput(kPlayerTwoKeyboard, isPressed);
    DOMINUS_EXPECT(raw.block == true);
    DOMINUS_EXPECT(raw.special == true);
    DOMINUS_EXPECT(raw.left == false);
    DOMINUS_EXPECT(raw.right == false);
    DOMINUS_EXPECT(raw.up == false);
    DOMINUS_EXPECT(raw.attack == false);
}

DOMINUS_TEST(HitmInputAdapter_ReadRawInput_UnboundKeyCode_AffectsNothing) {
    auto isPressed = FakeKeysDown({999});  // a real key neither binding uses
    HitmRawInputState raw = ReadRawInput(kPlayerOneKeyboard, isPressed);
    DOMINUS_EXPECT(raw.left == false);
    DOMINUS_EXPECT(raw.right == false);
    DOMINUS_EXPECT(raw.up == false);
    DOMINUS_EXPECT(raw.attack == false);
    DOMINUS_EXPECT(raw.block == false);
    DOMINUS_EXPECT(raw.special == false);
}

DOMINUS_TEST(HitmInputAdapter_PlayerOneAndPlayerTwoKeyboardBindings_ShareNoRealKeyCode) {
    // A real, load-bearing correctness property for local two-player:
    // pressing a Player 1 key must never also register as Player 2
    // input, and vice versa.
    std::vector<int> p1 = {kPlayerOneKeyboard.left,  kPlayerOneKeyboard.right,  kPlayerOneKeyboard.up,
                            kPlayerOneKeyboard.attack, kPlayerOneKeyboard.block, kPlayerOneKeyboard.special};
    std::vector<int> p2 = {kPlayerTwoKeyboard.left,  kPlayerTwoKeyboard.right,  kPlayerTwoKeyboard.up,
                            kPlayerTwoKeyboard.attack, kPlayerTwoKeyboard.block, kPlayerTwoKeyboard.special};
    for (int a : p1) {
        for (int b : p2) {
            DOMINUS_EXPECT(a != b);
        }
    }
}

DOMINUS_TEST(HitmInputAdapter_ReadRawInput_GamepadBinding_UsesTheSameGenericMachinery) {
    // "If the existing architecture supports it cleanly" -- it does:
    // the exact same ReadRawInput reads a gamepad binding identically to
    // a keyboard one, no separate gamepad code path anywhere.
    auto isPressed = FakeKeysDown({kGamepadButtons.attack});
    HitmRawInputState raw = ReadRawInput(kGamepadButtons, isPressed);
    DOMINUS_EXPECT(raw.attack == true);
    DOMINUS_EXPECT(raw.left == false);
}

// --- 3. The whole chain, proven against the real, proven simulation ------

DOMINUS_TEST(HitmInputAdapter_FullChain_PlayerOneRightKeyHeld_RealFighterActuallyWalksRight) {
    HitmMatch match = MakeBrooklynVsRocketMatch();

    // Real 120-frame round intro must elapse before input has any real
    // effect -- Track H Phase 3's own, unmodified phase machine.
    for (int i = 0; i < 121; ++i) match.AdvanceFrame(HitmInputCommand::kNeutral, HitmInputCommand::kNeutral);
    float xBefore = match.Snapshot().fighter_a.x;

    // Real Player 1 key state: only D (right) held.
    auto isPressed = FakeKeysDown({kPlayerOneKeyboard.right});
    for (int i = 0; i < 10; ++i) {
        HitmRawInputState raw = ReadRawInput(kPlayerOneKeyboard, isPressed);
        HitmInputCommand command = TranslateRawInput(raw);
        DOMINUS_EXPECT(command == HitmInputCommand::kRight);
        match.AdvanceFrame(command, HitmInputCommand::kNeutral);
    }

    // The real, already-proven simulation actually moved -- not a mock,
    // the same HitmMatch::AdvanceFrame Track H Phase 3 closed.
    DOMINUS_EXPECT(match.Snapshot().fighter_a.x > xBefore);
}

DOMINUS_TEST(HitmInputAdapter_FullChain_PlayerTwoBlockKeyHeld_RealFighterEntersRealBlockingStance) {
    HitmMatch match = MakeBrooklynVsRocketMatch();
    for (int i = 0; i < 121; ++i) match.AdvanceFrame(HitmInputCommand::kNeutral, HitmInputCommand::kNeutral);

    auto isPressed = FakeKeysDown({kPlayerTwoKeyboard.block});
    HitmRawInputState raw = ReadRawInput(kPlayerTwoKeyboard, isPressed);
    HitmInputCommand command = TranslateRawInput(raw);
    DOMINUS_EXPECT(command == HitmInputCommand::kBlock);
    match.AdvanceFrame(HitmInputCommand::kNeutral, command);

    DOMINUS_EXPECT(match.Snapshot().fighter_b.state == dominus::character::hitm::HitmFighterState::kBlockingStance);
}
