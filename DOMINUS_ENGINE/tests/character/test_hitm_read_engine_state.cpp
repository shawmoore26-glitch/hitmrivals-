// tests/character/test_hitm_read_engine_state.cpp
// ROADMAP.md Track H Module 5A. Exercises the real Brooklyn read-engine
// data (Module 2) through its own transition rules.
#include "CHARACTER/HitmBridge/HitmCombatGenome.h"
#include "CHARACTER/HitmBridge/HitmIdentityImporter.h"
#include "CHARACTER/HitmBridge/HitmReadEngineState.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::character::hitm::HitmCombatGenome;
using dominus::character::hitm::HitmIdentityImporter;
using dominus::character::hitm::HitmReadEngineState;

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

HitmReadEngineState MakeBrooklynReadEngine() {
    auto identity = HitmIdentityImporter::Import(FixtureDir("brooklyn"));
    if (!identity.ok) throw std::runtime_error("test setup: " + identity.error);
    auto genome = HitmCombatGenome::FromRecord(*identity.value);
    if (!genome.ok) throw std::runtime_error("test setup: " + genome.error);
    const auto* engine = genome.value->GetReadEngine();
    if (!engine) throw std::runtime_error("test setup: brooklyn has no read_engine");
    return HitmReadEngineState(*engine);
}

}  // namespace

DOMINUS_TEST(HitmReadEngineState_StartsAtZeroWithRealTierZeroData) {
    auto state = MakeBrooklynReadEngine();
    DOMINUS_EXPECT(state.CurrentReads() == 0);
    DOMINUS_EXPECT(state.MaxReads() == 5);
    // Real tier 0: "This Is Fun", damage_mult 1.0 -- his real, intended
    // balance point, not a placeholder zero state.
    DOMINUS_EXPECT(state.CurrentDamageMultiplier() == 1.0);
    DOMINUS_EXPECT(state.CurrentTierName() == "This Is Fun");
}

DOMINUS_TEST(HitmReadEngineState_GainReadAdvancesThroughRealTiers) {
    auto state = MakeBrooklynReadEngine();
    state.GainRead();
    DOMINUS_EXPECT(state.CurrentReads() == 1);
    DOMINUS_EXPECT(state.CurrentDamageMultiplier() == 1.06);
    DOMINUS_EXPECT(state.CurrentTierName() == "Noticing");

    state.GainRead();
    state.GainRead();
    DOMINUS_EXPECT(state.CurrentReads() == 3);
    DOMINUS_EXPECT(state.CurrentDamageMultiplier() == 1.21);
    DOMINUS_EXPECT(state.CurrentTierName() == "I See Everything");
}

DOMINUS_TEST(HitmReadEngineState_GainReadCapsAtRealMaxReads) {
    auto state = MakeBrooklynReadEngine();
    for (int i = 0; i < 10; ++i) state.GainRead();  // far past max_reads=5
    DOMINUS_EXPECT(state.CurrentReads() == 5);
    DOMINUS_EXPECT(state.CurrentDamageMultiplier() == 1.42);
    DOMINUS_EXPECT(state.CurrentTierName() == "Perfect Hunter");
}

DOMINUS_TEST(HitmReadEngineState_LoseReadFloorsAtZero) {
    auto state = MakeBrooklynReadEngine();
    state.GainRead();
    state.LoseRead();
    DOMINUS_EXPECT(state.CurrentReads() == 0);
    state.LoseRead();  // already at 0
    DOMINUS_EXPECT(state.CurrentReads() == 0);
}

DOMINUS_TEST(HitmReadEngineState_DecaysAfterRealFrameThreshold) {
    auto state = MakeBrooklynReadEngine();
    state.GainRead();
    state.GainRead();
    DOMINUS_EXPECT(state.CurrentReads() == 2);
    DOMINUS_EXPECT(state.DecayFrames() == 420);  // the real authored value

    for (int i = 0; i < 419; ++i) state.TickFrame();
    DOMINUS_EXPECT(state.CurrentReads() == 2);  // not yet -- one frame short

    state.TickFrame();  // frame 420 -- decay fires
    DOMINUS_EXPECT(state.CurrentReads() == 1);  // real decay.amount == 1
    DOMINUS_EXPECT(state.FramesSinceLastGain() == 0);  // countdown restarted
}

DOMINUS_TEST(HitmReadEngineState_RepeatedDecayDrainsToZero) {
    auto state = MakeBrooklynReadEngine();
    state.GainRead();
    state.GainRead();
    for (int cycle = 0; cycle < 2; ++cycle) {
        for (int i = 0; i < 420; ++i) state.TickFrame();
    }
    DOMINUS_EXPECT(state.CurrentReads() == 0);
}

DOMINUS_TEST(HitmReadEngineState_GainResetsDecayClock) {
    auto state = MakeBrooklynReadEngine();
    state.GainRead();
    for (int i = 0; i < 300; ++i) state.TickFrame();
    DOMINUS_EXPECT(state.FramesSinceLastGain() == 300);
    state.GainRead();
    DOMINUS_EXPECT(state.FramesSinceLastGain() == 0);
    DOMINUS_EXPECT(state.CurrentReads() == 2);
}
