// CHARACTER/Rig/RigBinder.cpp
#include "CHARACTER/Rig/RigBinder.h"

#include "ANIMATION/AnimationGraph/MotionGraphLoader.h"
#include "ANIMATION/IK/IKChainLoader.h"
#include "ANIMATION/Retargeting/RetargetMapLoader.h"
#include "ANIMATION/SkeletonSystem/AnimationClipLoader.h"
#include "ANIMATION/SkeletonSystem/SkeletonLoader.h"
#include "CHARACTER/Genome/CombatPhysicsGenomeLoader.h"
#include "CHARACTER/Genome/CombatStyleGenomeLoader.h"
#include "CHARACTER/Genome/CreatureGenomeLoader.h"
#include "CHARACTER/Genome/GameDesignGenomeLoader.h"
#include "CHARACTER/Genome/MaterialGenomeLoader.h"
#include "CHARACTER/Genome/VisualGenomeLoader.h"
#include "CHARACTER/Genome/VisualStyleGenomeLoader.h"
#include "CHARACTER/Genome/SocialGenomeLoader.h"

namespace dominus::character {

using core::VoidResult;

VoidResult RigBinder::Bind(core::MetaBinObject& obj, const std::filesystem::path& baseDir) {
    auto* skelRef = obj.GetComponent<core::SkeletonRefComponent>();
    if (!skelRef) {
        return VoidResult::Fail("object '" + obj.Id() + "' has no SkeletonRefComponent -- nothing to bind");
    }

    auto skeletonResult = animation::SkeletonLoader::LoadFromFile(baseDir / skelRef->ref_path);
    if (!skeletonResult.ok) {
        return VoidResult::Fail("failed to load skeleton for '" + obj.Id() + "': " + skeletonResult.error);
    }
    obj.AddComponent<SkeletonComponent>(SkeletonComponent{std::move(*skeletonResult.value)});

    auto* animRefs = obj.GetComponent<core::AnimationRefListComponent>();
    if (animRefs) {
        AnimationSetComponent animSet;
        for (const auto& ref : animRefs->clips) {
            auto clipResult = animation::AnimationClipLoader::LoadFromFile(baseDir / ref.ref_path);
            if (!clipResult.ok) {
                return VoidResult::Fail("failed to load animation '" + ref.name + "' for '" + obj.Id() +
                                         "': " + clipResult.error);
            }
            animSet.clips.emplace(ref.name, std::move(*clipResult.value));
        }
        obj.AddComponent<AnimationSetComponent>(std::move(animSet));
    }

    auto* motionGraphRef = obj.GetComponent<core::MotionGraphRefComponent>();
    if (motionGraphRef) {
        auto graphResult = animation::MotionGraphLoader::LoadFromFile(baseDir / motionGraphRef->ref_path);
        if (!graphResult.ok) {
            return VoidResult::Fail("failed to load motion graph for '" + obj.Id() + "': " + graphResult.error);
        }
        obj.AddComponent<MotionGraphComponent>(MotionGraphComponent{std::move(*graphResult.value)});
    }

    auto* ikRefs = obj.GetComponent<core::IKChainRefListComponent>();
    if (ikRefs) {
        IKChainSetComponent ikSet;
        for (const auto& ref : ikRefs->chains) {
            auto chainResult = animation::IKChainLoader::LoadFromFile(baseDir / ref.ref_path);
            if (!chainResult.ok) {
                return VoidResult::Fail("failed to load IK chain '" + ref.name + "' for '" + obj.Id() +
                                         "': " + chainResult.error);
            }
            animation::IKChainDef chainDef = std::move(*chainResult.value);
            chainDef.name = ref.name;
            ikSet.chains.emplace(ref.name, std::move(chainDef));
        }
        obj.AddComponent<IKChainSetComponent>(std::move(ikSet));
    }

    auto* retargetRef = obj.GetComponent<core::RetargetMapRefComponent>();
    if (retargetRef) {
        auto mapResult = animation::RetargetMapLoader::LoadFromFile(baseDir / retargetRef->ref_path);
        if (!mapResult.ok) {
            return VoidResult::Fail("failed to load retarget map for '" + obj.Id() + "': " + mapResult.error);
        }
        obj.AddComponent<RetargetMapComponent>(RetargetMapComponent{std::move(*mapResult.value)});
    }

    auto* socialGenomeRef = obj.GetComponent<core::SocialGenomeRefComponent>();
    if (socialGenomeRef) {
        auto genomeResult = SocialGenomeLoader::LoadFromFile(baseDir / socialGenomeRef->ref_path);
        if (!genomeResult.ok) {
            return VoidResult::Fail("failed to load social genome for '" + obj.Id() + "': " + genomeResult.error);
        }
        obj.AddComponent<SocialGenomeComponent>(SocialGenomeComponent{std::move(*genomeResult.value)});
    }

    auto* creatureGenomeRef = obj.GetComponent<core::CreatureGenomeRefComponent>();
    if (creatureGenomeRef) {
        auto genomeResult = CreatureGenomeLoader::LoadFromFile(baseDir / creatureGenomeRef->ref_path);
        if (!genomeResult.ok) {
            return VoidResult::Fail("failed to load creature genome for '" + obj.Id() + "': " + genomeResult.error);
        }
        obj.AddComponent<CreatureGenomeComponent>(CreatureGenomeComponent{std::move(*genomeResult.value)});
    }

    auto* visualGenomeRef = obj.GetComponent<core::VisualGenomeRefComponent>();
    if (visualGenomeRef) {
        auto genomeResult = VisualGenomeLoader::LoadFromFile(baseDir / visualGenomeRef->ref_path);
        if (!genomeResult.ok) {
            return VoidResult::Fail("failed to load visual genome for '" + obj.Id() + "': " + genomeResult.error);
        }
        obj.AddComponent<VisualGenomeComponent>(VisualGenomeComponent{std::move(*genomeResult.value)});
    }

    auto* combatStyleGenomeRef = obj.GetComponent<core::CombatStyleGenomeRefComponent>();
    if (combatStyleGenomeRef) {
        auto genomeResult = CombatStyleGenomeLoader::LoadFromFile(baseDir / combatStyleGenomeRef->ref_path);
        if (!genomeResult.ok) {
            return VoidResult::Fail("failed to load combat style genome for '" + obj.Id() + "': " + genomeResult.error);
        }
        obj.AddComponent<CombatStyleGenomeComponent>(CombatStyleGenomeComponent{std::move(*genomeResult.value)});
    }

    auto* combatPhysicsGenomeRef = obj.GetComponent<core::CombatPhysicsGenomeRefComponent>();
    if (combatPhysicsGenomeRef) {
        auto genomeResult = CombatPhysicsGenomeLoader::LoadFromFile(baseDir / combatPhysicsGenomeRef->ref_path);
        if (!genomeResult.ok) {
            return VoidResult::Fail("failed to load combat physics genome for '" + obj.Id() + "': " + genomeResult.error);
        }
        obj.AddComponent<CombatPhysicsGenomeComponent>(CombatPhysicsGenomeComponent{std::move(*genomeResult.value)});
    }

    auto* gameDesignGenomeRef = obj.GetComponent<core::GameDesignGenomeRefComponent>();
    if (gameDesignGenomeRef) {
        auto genomeResult = GameDesignGenomeLoader::LoadFromFile(baseDir / gameDesignGenomeRef->ref_path);
        if (!genomeResult.ok) {
            return VoidResult::Fail("failed to load game design genome for '" + obj.Id() + "': " + genomeResult.error);
        }
        obj.AddComponent<GameDesignGenomeComponent>(GameDesignGenomeComponent{std::move(*genomeResult.value)});
    }

    auto* materialGenomeRef = obj.GetComponent<core::MaterialGenomeRefComponent>();
    if (materialGenomeRef) {
        auto genomeResult = MaterialGenomeLoader::LoadFromFile(baseDir / materialGenomeRef->ref_path);
        if (!genomeResult.ok) {
            return VoidResult::Fail("failed to load material genome for '" + obj.Id() + "': " + genomeResult.error);
        }
        obj.AddComponent<MaterialGenomeComponent>(MaterialGenomeComponent{std::move(*genomeResult.value)});
    }

    auto* visualStyleGenomeRef = obj.GetComponent<core::VisualStyleGenomeRefComponent>();
    if (visualStyleGenomeRef) {
        auto genomeResult = VisualStyleGenomeLoader::LoadFromFile(baseDir / visualStyleGenomeRef->ref_path);
        if (!genomeResult.ok) {
            return VoidResult::Fail("failed to load visual style genome for '" + obj.Id() + "': " + genomeResult.error);
        }
        obj.AddComponent<VisualStyleGenomeComponent>(VisualStyleGenomeComponent{std::move(*genomeResult.value)});
    }

    return VoidResult::Ok();
}

}  // namespace dominus::character
