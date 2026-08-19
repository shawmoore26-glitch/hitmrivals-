// COMBAT/HitSystem/CombatBinder.cpp
#include "COMBAT/HitSystem/CombatBinder.h"

#include "CHARACTER/Genome/CombatIdentityLoader.h"
#include "COMBAT/HitSystem/CombatComponents.h"
#include "COMBAT/HitSystem/HurtboxLoader.h"
#include "COMBAT/HitSystem/MoveLoader.h"

namespace dominus::combat {

using core::VoidResult;

VoidResult CombatBinder::Bind(core::MetaBinObject& obj, const std::filesystem::path& baseDir) {
    auto* dnaRef = obj.GetComponent<core::CombatDnaRefComponent>();
    if (dnaRef) {
        auto idResult = character::CombatIdentityLoader::LoadFromFile(baseDir / dnaRef->ref_path);
        if (!idResult.ok) {
            return VoidResult::Fail("failed to load combat identity for '" + obj.Id() + "': " + idResult.error);
        }
        obj.AddComponent<CombatIdentityComponent>(CombatIdentityComponent{std::move(*idResult.value)});
    }

    auto* moveRefs = obj.GetComponent<core::MoveRefListComponent>();
    if (moveRefs) {
        MoveSetComponent moveSet;
        for (const auto& ref : moveRefs->moves) {
            auto moveResult = MoveLoader::LoadFromFile(baseDir / ref.ref_path);
            if (!moveResult.ok) {
                return VoidResult::Fail("failed to load move '" + ref.name + "' for '" + obj.Id() +
                                         "': " + moveResult.error);
            }
            moveSet.moves.emplace(ref.name, std::move(*moveResult.value));
        }
        obj.AddComponent<MoveSetComponent>(std::move(moveSet));
    }

    auto* hurtboxRef = obj.GetComponent<core::HurtboxRefComponent>();
    if (hurtboxRef) {
        auto hbResult = HurtboxLoader::LoadFromFile(baseDir / hurtboxRef->ref_path);
        if (!hbResult.ok) {
            return VoidResult::Fail("failed to load hurtboxes for '" + obj.Id() + "': " + hbResult.error);
        }
        obj.AddComponent<HurtboxSetComponent>(HurtboxSetComponent{std::move(*hbResult.value)});
    }

    return VoidResult::Ok();
}

}  // namespace dominus::combat
