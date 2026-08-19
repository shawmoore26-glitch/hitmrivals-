// tests/integration/test_ai_genome_pipeline.cpp
// Priority 1 proof: Player Action -> CombatAI -> GenomeDecoder ->
// DecisionWeights -> MoveSelector -> Motion Graph -> Skeleton Runtime, all
// connected, verified against Brooklyn's real .dominus data.
#include "AI/Agents/CombatAI.h"
#include "CHARACTER/Genome/GenomeDecoder.h"
#include "CHARACTER/Rig/RigBinder.h"
#include "COMBAT/CombatController.h"
#include "COMBAT/HitSystem/CombatBinder.h"
#include "CORE/Serialization/DominusSerializer.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::ai::CombatAI;
using dominus::ai::OpponentPatternTracker;
using dominus::character::CombatIdentity;
using dominus::character::GenomeDecoder;
using dominus::character::MakeMotionGraphEvaluator;
using dominus::character::RigBinder;
using dominus::combat::CombatBinder;
using dominus::combat::CombatController;
using dominus::combat::MoveSetComponent;
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

DOMINUS_TEST(CombatAI_ExpertCounterGenomeAttemptsCounterAgainstRepeatedPattern) {
    OpponentPatternTracker tracker(8);
    tracker.RecordMove("jab");
    tracker.RecordMove("dodge");
    tracker.RecordMove("jab");

    CombatIdentity expert{"disciplined_style", "mid", "reactive", "expert", "grounded", "low"};
    auto weights = GenomeDecoder::Decode(expert);  // counter_bias ~0.9
    CombatAI ai(tracker, weights);
    DOMINUS_EXPECT(ai.Decide() == "counter");
}

DOMINUS_TEST(CombatAI_PoorCounterGenomeNeverAttemptsCounterEvenWithIdenticalPattern) {
    // SAME opponent pattern as the test above -- only the genome differs.
    OpponentPatternTracker tracker(8);
    tracker.RecordMove("jab");
    tracker.RecordMove("dodge");
    tracker.RecordMove("jab");

    CombatIdentity poor{"brawler_style", "close", "reactive", "poor", "grounded", "low"};
    auto weights = GenomeDecoder::Decode(poor);  // counter_bias ~0.15, below the 0.3 threshold
    CombatAI ai(tracker, weights);
    DOMINUS_EXPECT(ai.Decide() != "counter");
}

DOMINUS_TEST(CombatAI_HighAggressionGenomePressesAttackInsteadOfBlocking) {
    OpponentPatternTracker tracker(8);
    tracker.RecordMove("jab");  // LastMoveWas("jab") is true

    CombatIdentity berserker{"reckless_style", "close", "reckless", "poor", "unpredictable", "high"};
    auto weights = GenomeDecoder::Decode(berserker);  // aggression ~1.0 (> 0.8 threshold)
    CombatAI ai(tracker, weights);
    DOMINUS_EXPECT(ai.Decide() == "attack");  // NOT block, despite opponent's last move being a jab
}

DOMINUS_TEST(CombatAI_CautiousGenomeStillBlocksAfterOpponentAttacks) {
    OpponentPatternTracker tracker(8);
    tracker.RecordMove("jab");

    CombatIdentity knight{"disciplined_style", "mid", "cautious", "average", "grounded", "low"};
    auto weights = GenomeDecoder::Decode(knight);  // aggression ~0.2, well under 0.8
    CombatAI ai(tracker, weights);
    DOMINUS_EXPECT(ai.Decide() == "block");
}

DOMINUS_TEST(CombatAI_DecideMoveName_FullPipelineFromRealBrooklynData) {
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    DOMINUS_EXPECT(loadResult.ok);
    auto& obj = *loadResult.value;
    RigBinder::Bind(obj, fixtureDir);
    CombatBinder::Bind(obj, fixtureDir);

    auto* identity = obj.GetComponent<dominus::combat::CombatIdentityComponent>();
    auto* moves = obj.GetComponent<MoveSetComponent>();
    DOMINUS_EXPECT(identity != nullptr && moves != nullptr);

    auto weights = GenomeDecoder::Decode(identity->identity);  // Brooklyn: relentless/expert/unpredictable

    OpponentPatternTracker tracker(8);  // empty -- no counter/block conditions trigger
    CombatAI ai(tracker, weights);

    std::string chosen = ai.DecideMoveName(*moves, {"jab", "counter", "combo_starter"});
    DOMINUS_EXPECT(!chosen.empty());
    DOMINUS_EXPECT(moves->Find(chosen) != nullptr);  // never invents a move that doesn't exist
}

DOMINUS_TEST(Integration_AIChoiceDrivesMotionGraphThroughCombatController) {
    // The full stated pipeline, end to end: CombatAI picks a move name,
    // CombatController::StartMove requests it through the real
    // MotionGraphEvaluator, which is the skeleton runtime's own driver.
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    auto& obj = *loadResult.value;
    RigBinder::Bind(obj, fixtureDir);
    CombatBinder::Bind(obj, fixtureDir);

    auto* identity = obj.GetComponent<dominus::combat::CombatIdentityComponent>();
    auto* moves = obj.GetComponent<MoveSetComponent>();
    auto weights = GenomeDecoder::Decode(identity->identity);

    OpponentPatternTracker tracker(8);
    CombatAI ai(tracker, weights);
    std::string chosen = ai.DecideMoveName(*moves, {"jab"});  // only jab is wired to a real motion state
    DOMINUS_EXPECT(chosen == "jab");

    auto evaluator = MakeMotionGraphEvaluator(obj);
    CombatController controller(*evaluator, *moves);
    bool started = controller.StartMove(chosen);
    DOMINUS_EXPECT(started);  // proves the AI's choice actually reached the motion graph
    DOMINUS_EXPECT(controller.CurrentMove()->name == "jab");
}
