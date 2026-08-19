// tests/combat/test_combat_ai.cpp
#include "AI/Agents/BehaviorTree.h"
#include "AI/Agents/CombatAI.h"
#include "tests/TestFramework.h"

using dominus::ai::ActionNode;
using dominus::ai::CombatAI;
using dominus::ai::ConditionNode;
using dominus::ai::NodeStatus;
using dominus::ai::OpponentPatternTracker;
using dominus::ai::SelectorNode;
using dominus::ai::SequenceNode;

DOMINUS_TEST(BehaviorTree_SelectorReturnsFirstSuccess) {
    SelectorNode root;
    root.AddChild(std::make_unique<ConditionNode>([]() { return false; }));
    root.AddChild(std::make_unique<ActionNode>([]() { return NodeStatus::kSuccess; }));
    DOMINUS_EXPECT(root.Tick() == NodeStatus::kSuccess);
}

DOMINUS_TEST(BehaviorTree_SelectorFailsWhenAllChildrenFail) {
    SelectorNode root;
    root.AddChild(std::make_unique<ConditionNode>([]() { return false; }));
    root.AddChild(std::make_unique<ConditionNode>([]() { return false; }));
    DOMINUS_EXPECT(root.Tick() == NodeStatus::kFailure);
}

DOMINUS_TEST(BehaviorTree_SequenceRequiresAllChildrenToSucceed) {
    SequenceNode root;
    root.AddChild(std::make_unique<ConditionNode>([]() { return true; }));
    root.AddChild(std::make_unique<ConditionNode>([]() { return false; }));
    root.AddChild(std::make_unique<ActionNode>([]() { return NodeStatus::kSuccess; }));  // never reached
    DOMINUS_EXPECT(root.Tick() == NodeStatus::kFailure);
}

DOMINUS_TEST(BehaviorTree_SequenceSucceedsWhenAllChildrenSucceed) {
    SequenceNode root;
    root.AddChild(std::make_unique<ConditionNode>([]() { return true; }));
    root.AddChild(std::make_unique<ActionNode>([]() { return NodeStatus::kSuccess; }));
    DOMINUS_EXPECT(root.Tick() == NodeStatus::kSuccess);
}

DOMINUS_TEST(OpponentPatternTracker_MostFrequentMoveReflectsHistory) {
    OpponentPatternTracker tracker(8);
    tracker.RecordMove("jab");
    tracker.RecordMove("dodge");
    tracker.RecordMove("jab");
    DOMINUS_EXPECT(tracker.MostFrequentMove() == "jab");
}

DOMINUS_TEST(OpponentPatternTracker_BoundedHistoryDropsOldEntries) {
    OpponentPatternTracker tracker(2);
    tracker.RecordMove("jab");
    tracker.RecordMove("dodge");
    tracker.RecordMove("counter");
    DOMINUS_EXPECT(tracker.HistorySize() == 2);
    DOMINUS_EXPECT(!tracker.LastMoveWas("jab"));  // fell out of the window
    DOMINUS_EXPECT(tracker.LastMoveWas("counter"));
}

DOMINUS_TEST(CombatAI_DefaultsToAttackWithNoHistory) {
    OpponentPatternTracker tracker(8);
    CombatAI ai(tracker);
    DOMINUS_EXPECT(ai.Decide() == "attack");
}

DOMINUS_TEST(CombatAI_BlocksImmediatelyAfterOpponentAttacks) {
    OpponentPatternTracker tracker(8);
    tracker.RecordMove("jab");
    CombatAI ai(tracker);
    DOMINUS_EXPECT(ai.Decide() == "block");
}

DOMINUS_TEST(CombatAI_CountersRepeatedJabPattern) {
    OpponentPatternTracker tracker(8);
    tracker.RecordMove("jab");
    tracker.RecordMove("dodge");
    tracker.RecordMove("jab");
    // Most frequent is "jab" (2 of 3) with history size >= 3 -> counter
    // takes priority over the "last move was jab" block rule since it's
    // checked first in the tree.
    CombatAI ai(tracker);
    DOMINUS_EXPECT(ai.Decide() == "counter");
}
