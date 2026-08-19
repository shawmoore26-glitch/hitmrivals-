// ANIMATION/AnimationGraph/AnimationLayerStack.h
// Layers composite on top of a base pose: each non-base layer carries a
// bone mask (nullopt = affects every bone) and a weight, and is blended
// onto the running result only for its masked bones. This is what lets an
// "upper body attack" play over a looping "lower body idle" without either
// one owning the whole skeleton -- the requirement this file exists for.
#pragma once

#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "ANIMATION/AnimationGraph/MotionGraphEvaluator.h"
#include "ANIMATION/ProceduralMotion/AnimationPlayer.h"
#include "ANIMATION/SkeletonSystem/Skeleton.h"

namespace dominus::animation {

struct LayerDef {
    std::string name;
    float weight = 1.0f;
    std::optional<std::unordered_set<std::string>> bone_mask;  // nullopt = all bones
};

class AnimationLayerStack {
public:
    // Layer 0 is treated as the base layer regardless of its own mask/weight
    // -- it always contributes its full pose before any other layer blends
    // in. Evaluators are owned externally (they're constructed against a
    // specific Skeleton + IClipSource + MotionGraph) and referenced here.
    void AddLayer(LayerDef def, MotionGraphEvaluator* evaluator) {
        layers_.push_back(Layer{std::move(def), evaluator});
    }

    size_t LayerCount() const { return layers_.size(); }

    Pose Composite(const Skeleton& skeleton, float dt) {
        if (layers_.empty()) return {};

        Pose result = layers_[0].evaluator->Update(dt);

        for (size_t i = 1; i < layers_.size(); ++i) {
            Layer& layer = layers_[i];
            Pose layerPose = layer.evaluator->Update(dt);

            const auto& bones = skeleton.Bones();
            for (size_t b = 0; b < bones.size(); ++b) {
                if (layer.def.bone_mask && layer.def.bone_mask->count(bones[b].name) == 0) {
                    continue;  // this bone isn't affected by this layer
                }
                result[b] = Lerp(result[b], layerPose[b], layer.def.weight);
            }
        }
        return result;
    }

private:
    struct Layer {
        LayerDef def;
        MotionGraphEvaluator* evaluator;
    };
    std::vector<Layer> layers_;
};

}  // namespace dominus::animation
