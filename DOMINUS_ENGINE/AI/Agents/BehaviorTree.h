// AI/Agents/BehaviorTree.h
// LAW C011: enemies must think, not run a fixed wait/attack/repeat loop.
// A behavior tree is the minimum real mechanism for that -- Selector tries
// children until one succeeds (an "or"), Sequence runs children until one
// fails (an "and"), Condition/Action are leaves. Foundation scope: no
// blackboard/memory nodes, no parallel nodes -- add them when a real AI
// needs them, not speculatively.
#pragma once

#include <functional>
#include <memory>
#include <vector>

namespace dominus::ai {

enum class NodeStatus { kSuccess, kFailure, kRunning };

class BehaviorNode {
public:
    virtual ~BehaviorNode() = default;
    virtual NodeStatus Tick() = 0;
};

class ConditionNode : public BehaviorNode {
public:
    explicit ConditionNode(std::function<bool()> predicate) : predicate_(std::move(predicate)) {}
    NodeStatus Tick() override { return predicate_() ? NodeStatus::kSuccess : NodeStatus::kFailure; }

private:
    std::function<bool()> predicate_;
};

class ActionNode : public BehaviorNode {
public:
    explicit ActionNode(std::function<NodeStatus()> action) : action_(std::move(action)) {}
    NodeStatus Tick() override { return action_(); }

private:
    std::function<NodeStatus()> action_;
};

class SelectorNode : public BehaviorNode {
public:
    void AddChild(std::unique_ptr<BehaviorNode> child) { children_.push_back(std::move(child)); }
    NodeStatus Tick() override {
        for (auto& child : children_) {
            NodeStatus status = child->Tick();
            if (status != NodeStatus::kFailure) return status;
        }
        return NodeStatus::kFailure;
    }

private:
    std::vector<std::unique_ptr<BehaviorNode>> children_;
};

class SequenceNode : public BehaviorNode {
public:
    void AddChild(std::unique_ptr<BehaviorNode> child) { children_.push_back(std::move(child)); }
    NodeStatus Tick() override {
        for (auto& child : children_) {
            NodeStatus status = child->Tick();
            if (status != NodeStatus::kSuccess) return status;
        }
        return NodeStatus::kSuccess;
    }

private:
    std::vector<std::unique_ptr<BehaviorNode>> children_;
};

}  // namespace dominus::ai
