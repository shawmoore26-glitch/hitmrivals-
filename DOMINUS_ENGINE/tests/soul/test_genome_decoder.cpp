// tests/soul/test_genome_decoder.cpp
#include "CHARACTER/Genome/GenomeDecoder.h"
#include "COMBAT/HitSystem/MoveLoader.h"
#include "COMBAT/MoveSelector.h"
#include "COMBAT/PhysicsCombat/ClashSystem.h"
#include "COMBAT/ReactionSystem/ReactionSystem.h"
#include "tests/TestFramework.h"

#include <cmath>
#include <filesystem>

using dominus::character::CombatIdentity;
using dominus::character::DecisionWeights;
using dominus::character::GenomeDecoder;
using dominus::combat::ClashSystem;
using dominus::combat::MoveDef;
using dominus::combat::MoveLoader;
using dominus::combat::MoveSelector;
using dominus::combat::ReactionInput;
using dominus::combat::ReactionSystem;
using dominus::combat::ReactionType;

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
bool NearlyEqual(float a, float b, float eps = 0.01f) { return std::fabs(a - b) < eps; }
}  // namespace

DOMINUS_TEST(GenomeDecoder_RelentlessPressureGivesHighAggression) {
    CombatIdentity id{"drunken_kung_fu", "close", "relentless", "expert", "unpredictable", "medium"};
    auto weights = GenomeDecoder::Decode(id);
    DOMINUS_EXPECT(weights.aggression > 0.8f);
    DOMINUS_EXPECT(weights.counter_bias > 0.8f);
    DOMINUS_EXPECT(weights.unpredictability > 0.8f);
}

DOMINUS_TEST(GenomeDecoder_UnknownStringsFallBackToNeutralDefault) {
    CombatIdentity id{"whatever", "unknown_range", "unknown_pressure", "unknown_counter", "unknown_mobility",
                       "unknown_risk"};
    auto weights = GenomeDecoder::Decode(id);
    DOMINUS_EXPECT(NearlyEqual(weights.aggression, 0.5f));
    DOMINUS_EXPECT(NearlyEqual(weights.risk_tolerance, 0.5f));
    DOMINUS_EXPECT(NearlyEqual(weights.counter_bias, 0.5f));
}

DOMINUS_TEST(GenomeDecoder_DifferentGenomesProduceDifferentWeights) {
    // LAW C003's actual claim: two identities must decode differently.
    CombatIdentity knight{"disciplined_swordsman", "mid", "cautious", "average", "grounded", "low"};
    CombatIdentity berserker{"reckless_axeman", "close", "reckless", "poor", "unpredictable", "high"};
    auto knightWeights = GenomeDecoder::Decode(knight);
    auto berserkerWeights = GenomeDecoder::Decode(berserker);
    DOMINUS_EXPECT(berserkerWeights.aggression > knightWeights.aggression);
    DOMINUS_EXPECT(berserkerWeights.risk_tolerance > knightWeights.risk_tolerance);
}

DOMINUS_TEST(ClashSystem_SkillFromWeightsScalesWithCounterBias) {
    DecisionWeights expert{0.5f, 0.5f, 0.5f, 0.9f, 0.5f};
    DecisionWeights poor{0.5f, 0.5f, 0.5f, 0.15f, 0.5f};
    DOMINUS_EXPECT(ClashSystem::SkillFromWeights(expert) > ClashSystem::SkillFromWeights(poor));
}

DOMINUS_TEST(ReactionSystem_NeutralDefenseBiasMatchesOriginalPhase3Thresholds) {
    // defense_bias defaults to 0.5 (neutral) -- must reproduce Phase 3's
    // exact 15/35 power thresholds for backward compatibility.
    auto low = ReactionSystem::Determine(ReactionInput{.hit_power = 14.9f}, 1.0f);
    auto mid = ReactionSystem::Determine(ReactionInput{.hit_power = 20.0f}, 1.0f);
    auto high = ReactionSystem::Determine(ReactionInput{.hit_power = 40.0f}, 1.0f);
    DOMINUS_EXPECT(low.type == ReactionType::kStagger);
    DOMINUS_EXPECT(mid.type == ReactionType::kKnockback);
    DOMINUS_EXPECT(high.type == ReactionType::kKnockdown);
}

DOMINUS_TEST(ReactionSystem_HighDefenseBiasRaisesThresholds) {
    // At defense_bias=1.0, thresholds shift up by +10 -> 25/45. A hit of 20
    // power that would knockback a neutral fighter only staggers a tanky one.
    auto tanky = ReactionSystem::Determine(ReactionInput{.hit_power = 20.0f, .defense_bias = 1.0f}, 1.0f);
    DOMINUS_EXPECT(tanky.type == ReactionType::kStagger);
}

DOMINUS_TEST(ReactionSystem_LowDefenseBiasLowersThresholds) {
    // At defense_bias=0.0, thresholds shift down by -10 -> 5/25. A hit of 10
    // power that would only stagger a neutral fighter knocks back a fragile one.
    auto fragile = ReactionSystem::Determine(ReactionInput{.hit_power = 10.0f, .defense_bias = 0.0f}, 1.0f);
    DOMINUS_EXPECT(fragile.type == ReactionType::kKnockback);
}

DOMINUS_TEST(MoveSelector_AggressiveWeightsPreferHigherPowerMove) {
    auto dir = FixtureDir();
    auto jab = MoveLoader::LoadFromFile(dir / "brooklyn_move_jab.json");        // power 18
    auto counter = MoveLoader::LoadFromFile(dir / "brooklyn_move_counter.json"); // power 22, risk low
    DOMINUS_EXPECT(jab.ok && counter.ok);

    dominus::combat::MoveSetComponent moves;
    moves.moves.emplace("jab", *jab.value);
    moves.moves.emplace("counter", *counter.value);

    DecisionWeights aggressive{1.0f, 0.9f, 0.1f, 0.5f, 0.5f};  // max aggression, high risk tolerance
    auto best = MoveSelector::SelectBest(moves, {"jab", "counter"}, aggressive);
    // counter has higher power (22 vs 18) AND low risk cost -> should win
    // under aggressive+risk-tolerant weights.
    DOMINUS_EXPECT(best == "counter");
}

DOMINUS_TEST(MoveSelector_CautiousWeightsAvoidHighRiskMoves) {
    auto dir = FixtureDir();
    auto jab = MoveLoader::LoadFromFile(dir / "brooklyn_move_jab.json");  // risk medium
    auto counter = MoveLoader::LoadFromFile(dir / "brooklyn_move_counter.json");  // risk low, lower power impact

    dominus::combat::MoveSetComponent moves;
    moves.moves.emplace("jab", *jab.value);
    moves.moves.emplace("counter", *counter.value);

    DecisionWeights cautious{0.3f, 0.05f, 0.1f, 0.5f, 0.5f};  // low aggression, near-zero risk tolerance
    auto best = MoveSelector::SelectBest(moves, {"jab", "counter"}, cautious);
    // counter's lower risk cost (low vs medium) matters much more when risk
    // tolerance is near zero, even though jab has lower raw power weighted in.
    DOMINUS_EXPECT(best == "counter");
}

DOMINUS_TEST(MoveSelector_EmptyCandidatesReturnsEmptyString) {
    dominus::combat::MoveSetComponent moves;
    DecisionWeights w{};
    DOMINUS_EXPECT(MoveSelector::SelectBest(moves, {}, w).empty());
}
