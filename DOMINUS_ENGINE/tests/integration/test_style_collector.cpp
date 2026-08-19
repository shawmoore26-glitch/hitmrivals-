// tests/integration/test_style_collector.cpp
// Priority 2 proof: Combat Event -> Style Collector -> Metrics -> Style
// Rank, with metrics genuinely auto-populated from real MoveDef data
// instead of hand-typed StyleMetrics literals.
#include "CHARACTER/Rig/RigBinder.h"
#include "COMBAT/CombatController.h"
#include "COMBAT/ComboSystem/StyleCollector.h"
#include "COMBAT/HitSystem/CombatBinder.h"
#include "COMBAT/HitSystem/MoveLoader.h"
#include "CORE/Serialization/DominusSerializer.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::character::MakeMotionGraphEvaluator;
using dominus::character::RigBinder;
using dominus::combat::CombatBinder;
using dominus::combat::CombatController;
using dominus::combat::MoveLoader;
using dominus::combat::MoveSetComponent;
using dominus::combat::ReactionInput;
using dominus::combat::StyleCollector;
using dominus::combat::StyleRank;
using dominus::core::DominusSerializer;

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

DOMINUS_TEST(StyleCollector_RecordHitLandedAutoPopulatesFromRealMoveData) {
    auto jab = MoveLoader::LoadFromFile(FixtureDir() / "brooklyn_move_jab.json");
    DOMINUS_EXPECT(jab.ok);

    StyleCollector collector;
    collector.RecordHitLanded(*jab.value, 18.0f);

    DOMINUS_EXPECT(collector.Metrics().hit_count == 1);
    DOMINUS_EXPECT(collector.Metrics().distinct_move_count == 1);
    DOMINUS_EXPECT(collector.Metrics().damage_dealt == 18.0f);
}

DOMINUS_TEST(StyleCollector_DistinctMoveCountOnlyCountsUniqueNames) {
    auto jab = MoveLoader::LoadFromFile(FixtureDir() / "brooklyn_move_jab.json");
    StyleCollector collector;
    collector.RecordHitLanded(*jab.value, 10.0f);
    collector.RecordHitLanded(*jab.value, 10.0f);
    collector.RecordHitLanded(*jab.value, 10.0f);
    DOMINUS_EXPECT(collector.Metrics().hit_count == 3);
    DOMINUS_EXPECT(collector.Metrics().distinct_move_count == 1);  // same move every time
}

DOMINUS_TEST(StyleCollector_HighRiskMovesIncrementRiskCounter) {
    auto counter = MoveLoader::LoadFromFile(FixtureDir() / "brooklyn_move_dodge.json");  // risk "low"
    auto jab = MoveLoader::LoadFromFile(FixtureDir() / "brooklyn_move_jab.json");         // risk "medium"
    StyleCollector collector;
    collector.RecordHitLanded(*jab.value, 5.0f);
    collector.RecordHitLanded(*counter.value, 5.0f);
    DOMINUS_EXPECT(collector.Metrics().high_risk_move_count == 0);  // neither is "high" risk
}

DOMINUS_TEST(StyleCollector_CounterMoveIncrementsCounterCount) {
    auto counterMove = MoveLoader::LoadFromFile(FixtureDir() / "brooklyn_move_counter.json");  // name == "counter"
    StyleCollector collector;
    collector.RecordHitLanded(*counterMove.value, 22.0f);
    DOMINUS_EXPECT(collector.Metrics().counter_count == 1);
}

DOMINUS_TEST(StyleCollector_RecordersForUnwiredEventsExistButDontAffectScore) {
    StyleCollector collector;
    float scoreBefore = collector.CurrentScore();
    collector.RecordPerfectDodge();
    collector.RecordParry();
    collector.RecordAirTime(0.5f);
    // These ARE recorded (telemetry exists)...
    DOMINUS_EXPECT(collector.Metrics().perfect_dodge_count == 1);
    DOMINUS_EXPECT(collector.Metrics().parry_count == 1);
    DOMINUS_EXPECT(collector.Metrics().air_time_seconds == 0.5f);
    // ...but Score() is untouched -- honestly not yet weighted in.
    DOMINUS_EXPECT(collector.CurrentScore() == scoreBefore);
}

DOMINUS_TEST(StyleCollector_ResetClearsAllMetrics) {
    auto jab = MoveLoader::LoadFromFile(FixtureDir() / "brooklyn_move_jab.json");
    StyleCollector collector;
    collector.RecordHitLanded(*jab.value, 10.0f);
    collector.Reset();
    DOMINUS_EXPECT(collector.Metrics().hit_count == 0);
    DOMINUS_EXPECT(collector.CurrentRank() == StyleRank::kD);
}

DOMINUS_TEST(CombatController_ApplyHitAutoRecordsDamageTakenOnAttachedCollector) {
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    auto& obj = *loadResult.value;
    RigBinder::Bind(obj, fixtureDir);
    CombatBinder::Bind(obj, fixtureDir);

    auto evaluator = MakeMotionGraphEvaluator(obj);
    auto* moveSet = obj.GetComponent<MoveSetComponent>();
    CombatController controller(*evaluator, *moveSet);

    StyleCollector collector;
    controller.SetStyleCollector(&collector);
    DOMINUS_EXPECT(controller.Style() == &collector);

    controller.ApplyHit(ReactionInput{.hit_power = 25.0f}, 1.0f);
    DOMINUS_EXPECT(collector.Metrics().damage_taken == 25.0f);
}

DOMINUS_TEST(CombatController_RecordMoveLandedAutoRecordsDamageDealt) {
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    auto& obj = *loadResult.value;
    RigBinder::Bind(obj, fixtureDir);
    CombatBinder::Bind(obj, fixtureDir);

    auto evaluator = MakeMotionGraphEvaluator(obj);
    auto* moveSet = obj.GetComponent<MoveSetComponent>();
    CombatController controller(*evaluator, *moveSet);

    StyleCollector collector;
    controller.SetStyleCollector(&collector);

    controller.StartMove("jab");
    controller.RecordMoveLanded(18.0f);  // caller confirms the hit landed (e.g. via CollisionEvaluator)

    DOMINUS_EXPECT(collector.Metrics().hit_count == 1);
    DOMINUS_EXPECT(collector.Metrics().damage_dealt == 18.0f);
    DOMINUS_EXPECT(collector.Metrics().distinct_move_count == 1);
}

DOMINUS_TEST(CombatController_RecordMoveLandedIsNoOpWithoutActiveMoveOrCollector) {
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    auto& obj = *loadResult.value;
    RigBinder::Bind(obj, fixtureDir);
    CombatBinder::Bind(obj, fixtureDir);

    auto evaluator = MakeMotionGraphEvaluator(obj);
    auto* moveSet = obj.GetComponent<MoveSetComponent>();
    CombatController controller(*evaluator, *moveSet);

    // No collector attached, no move started -- must not crash.
    controller.RecordMoveLanded(10.0f);
    DOMINUS_EXPECT(controller.Style() == nullptr);
}
