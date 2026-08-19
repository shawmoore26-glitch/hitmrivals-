// tests/soul/test_style_rank.cpp
#include "COMBAT/ComboSystem/StyleRankSystem.h"
#include "tests/TestFramework.h"

#include <string>

using dominus::combat::StyleMetrics;
using dominus::combat::StyleRank;
using dominus::combat::StyleRankSystem;

DOMINUS_TEST(StyleRank_ZeroActivityIsD) {
    StyleMetrics m{};
    DOMINUS_EXPECT(StyleRankSystem::Evaluate(m) == StyleRank::kD);
}

DOMINUS_TEST(StyleRank_SingleBasicHitIsC) {
    StyleMetrics m{.hit_count = 1, .distinct_move_count = 1};
    DOMINUS_EXPECT(StyleRankSystem::Evaluate(m) == StyleRank::kC);
}

DOMINUS_TEST(StyleRank_VariedAggressiveComboIsS) {
    StyleMetrics m{.hit_count = 5, .distinct_move_count = 4, .high_risk_move_count = 2};
    DOMINUS_EXPECT(StyleRankSystem::Evaluate(m) == StyleRank::kS);
}

DOMINUS_TEST(StyleRank_DominantFlawlessPerformanceIsSSS) {
    StyleMetrics m{.hit_count = 10, .distinct_move_count = 6, .high_risk_move_count = 4, .damage_dealt = 50.0f};
    DOMINUS_EXPECT(StyleRankSystem::Evaluate(m) == StyleRank::kSSS);
}

DOMINUS_TEST(StyleRank_TakingMoreDamageThanDealingLowersScore) {
    StyleMetrics clean{.hit_count = 3, .distinct_move_count = 2, .damage_dealt = 30.0f, .damage_taken = 0.0f};
    StyleMetrics sloppy{.hit_count = 3, .distinct_move_count = 2, .damage_dealt = 5.0f, .damage_taken = 40.0f};
    DOMINUS_EXPECT(StyleRankSystem::Score(clean) > StyleRankSystem::Score(sloppy));
}

DOMINUS_TEST(StyleRank_ScoreNeverGoesNegative) {
    StyleMetrics disaster{.hit_count = 0, .distinct_move_count = 0, .damage_dealt = 0.0f, .damage_taken = 1000.0f};
    DOMINUS_EXPECT(StyleRankSystem::Score(disaster) == 0.0f);
}

DOMINUS_TEST(StyleRank_NameMappingCoversAllRanks) {
    DOMINUS_EXPECT(std::string(StyleRankSystem::RankName(StyleRank::kD)) == "D");
    DOMINUS_EXPECT(std::string(StyleRankSystem::RankName(StyleRank::kSSS)) == "SSS");
}

DOMINUS_TEST(StyleRank_ThreeOrMoreDistinctMovesEarnsCreativityBonus) {
    StyleMetrics two{.hit_count = 2, .distinct_move_count = 2};
    StyleMetrics three{.hit_count = 2, .distinct_move_count = 3};
    // Same hit_count, but three distinct moves crosses the creativity bonus
    // threshold -- score jump should exceed just the +10 from variety alone.
    float diff = StyleRankSystem::Score(three) - StyleRankSystem::Score(two);
    DOMINUS_EXPECT(diff > 10.0f);  // 10 (variety) + 10 (creativity bonus)
}
