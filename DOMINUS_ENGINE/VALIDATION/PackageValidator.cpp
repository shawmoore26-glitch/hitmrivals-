// VALIDATION/PackageValidator.cpp
#include "VALIDATION/PackageValidator.h"

#include <cctype>

#include "CHARACTER/Rig/RigBinder.h"
#include "COMBAT/HitSystem/AssetValidation.h"
#include "COMBAT/HitSystem/CombatBinder.h"
#include "COMBAT/HitSystem/CombatComponents.h"
#include "CORE/Serialization/DominusSerializer.h"

namespace dominus::validation {

void PackageValidator::CheckIdentityCompleteness(core::MetaBinObject& obj, ValidationReport& report) {
    auto* identity = obj.GetComponent<core::IdentityComponent>();
    if (!identity) {
        report.Add(Severity::kError, "identity",
                    "object '" + obj.Id() + "' has no IdentityComponent (missing 'identity' block in .dominus)");
    } else if (identity->display_name.empty()) {
        report.Add(Severity::kError, "identity", "object '" + obj.Id() + "' has an empty display_name");
    }

    auto* combatIdentity = obj.GetComponent<combat::CombatIdentityComponent>();
    auto* moves = obj.GetComponent<combat::MoveSetComponent>();
    if (!combatIdentity) {
        if (moves && !moves->moves.empty()) {
            // LAW C002: combat identity and abilities exist together.
            // Moves with no genome behind them is a real inconsistency.
            report.Add(Severity::kError, "identity",
                        "object '" + obj.Id() + "' has moves but no combat_dna -- abilities without a genome");
        } else {
            report.Add(Severity::kInfo, "identity", "object '" + obj.Id() + "' has no combat genome (fine for non-combat entities)");
        }
    } else if (combatIdentity->identity.style.empty()) {
        report.Add(Severity::kError, "identity", "object '" + obj.Id() + "' has an empty combat identity style");
    }
}

void PackageValidator::CheckAssetOwnership(core::MetaBinObject& obj, const std::filesystem::path& baseDir,
                                            ValidationReport& report) {
    auto checkPath = [&](const std::string& ownerField, const std::string& refPath) {
        if (!std::filesystem::exists(baseDir / refPath)) {
            report.Add(Severity::kError, "asset_ownership",
                        "object '" + obj.Id() + "' field '" + ownerField + "' references a missing file: " + refPath);
        }
    };

    if (auto* r = obj.GetComponent<core::SkeletonRefComponent>()) checkPath("skeleton", r->ref_path);
    if (auto* r = obj.GetComponent<core::RawRefComponent>()) checkPath("mesh", r->ref_path);
    if (auto* r = obj.GetComponent<core::MotionGraphRefComponent>()) checkPath("motion_graph", r->ref_path);
    if (auto* r = obj.GetComponent<core::CombatDnaRefComponent>()) checkPath("combat_dna", r->ref_path);
    if (auto* r = obj.GetComponent<core::HurtboxRefComponent>()) checkPath("physics_rules.hurtbox_ref", r->ref_path);
    if (auto* r = obj.GetComponent<core::RetargetMapRefComponent>()) checkPath("retarget_map", r->ref_path);
    if (auto* r = obj.GetComponent<core::SocialGenomeRefComponent>()) checkPath("social_genome", r->ref_path);
    if (auto* r = obj.GetComponent<core::CreatureGenomeRefComponent>()) checkPath("creature_genome", r->ref_path);
    if (auto* r = obj.GetComponent<core::VisualGenomeRefComponent>()) checkPath("visual_genome", r->ref_path);
    if (auto* r = obj.GetComponent<core::CombatStyleGenomeRefComponent>()) checkPath("combat_style_genome", r->ref_path);
    if (auto* r = obj.GetComponent<core::CombatPhysicsGenomeRefComponent>()) checkPath("combat_physics_genome", r->ref_path);
    if (auto* r = obj.GetComponent<core::GameDesignGenomeRefComponent>()) checkPath("game_design_genome", r->ref_path);
    if (auto* r = obj.GetComponent<core::MaterialGenomeRefComponent>()) checkPath("material_genome", r->ref_path);
    if (auto* r = obj.GetComponent<core::VisualStyleGenomeRefComponent>()) checkPath("visual_style_genome", r->ref_path);

    if (auto* list = obj.GetComponent<core::AnimationRefListComponent>()) {
        for (auto& entry : list->clips) checkPath("animations['" + entry.name + "']", entry.ref_path);
    }
    if (auto* list = obj.GetComponent<core::IKChainRefListComponent>()) {
        for (auto& entry : list->chains) checkPath("ik_chains['" + entry.name + "']", entry.ref_path);
    }
    if (auto* list = obj.GetComponent<core::MoveRefListComponent>()) {
        for (auto& entry : list->moves) checkPath("moves['" + entry.name + "']", entry.ref_path);
    }
    if (auto* list = obj.GetComponent<core::TransformationRefListComponent>()) {
        for (auto& entry : list->transformations) checkPath("transformations['" + entry.name + "']", entry.ref_path);
    }
}

void PackageValidator::CheckNamingConsistency(core::MetaBinObject& obj, ValidationReport& report) {
    // object_id must be lowercase snake_case, matching the convention
    // documented in schemas/dominus_object.schema.json itself.
    const std::string& id = obj.Id();
    bool validId = !id.empty();
    for (char c : id) {
        bool ok = std::islower(static_cast<unsigned char>(c)) || std::isdigit(static_cast<unsigned char>(c)) || c == '_';
        if (!ok) {
            validId = false;
            break;
        }
    }
    if (!validId) {
        report.Add(Severity::kError, "naming",
                    "object_id '" + id + "' does not match the schema's snake_case convention (^[a-z0-9_]+$)");
    }

    // A move's declared ref-list name is used as the MoveSetComponent map
    // key, but nothing before this check ever confirmed it matches the
    // move file's OWN internal "name" field -- a real latent-bug class
    // (rename a move file's internal name, forget to update the .dominus
    // ref list, and nothing catches the mismatch until something looks
    // up the move by the wrong name at runtime).
    if (auto* moves = obj.GetComponent<combat::MoveSetComponent>()) {
        for (auto& [key, move] : moves->moves) {
            if (key != move.name) {
                report.Add(Severity::kError, "naming",
                            "move ref name '" + key + "' does not match its file's internal name field '" +
                                move.name + "'");
            }
        }
    }

    if (auto* animSet = obj.GetComponent<character::AnimationSetComponent>()) {
        for (auto& [key, clip] : animSet->clips) {
            if (key != clip.name) {
                report.Add(Severity::kError, "naming",
                            "animation ref name '" + key + "' does not match its file's internal name field '" +
                                clip.name + "'");
            }
        }
    }
}

void PackageValidator::CheckDeterministicRebuild(const std::filesystem::path& dominusPath, ValidationReport& report) {
    auto first = core::DominusSerializer::Load(dominusPath);
    auto second = core::DominusSerializer::Load(dominusPath);
    if (!first.ok || !second.ok) {
        report.Add(Severity::kError, "integrity", "deterministic rebuild check failed to load: " + dominusPath.string());
        return;
    }
    if (first.value->Id() != second.value->Id() || first.value->Version() != second.value->Version() ||
        first.value->ComponentCount() != second.value->ComponentCount()) {
        report.Add(Severity::kError, "integrity",
                    "two loads of the same file produced different results -- non-deterministic parse");
    }

    // dominus_version must look like semver, per the schema's own
    // documented requirement.
    const std::string& version = first.value->Version();
    int dotCount = 0;
    bool validVersion = !version.empty();
    for (char c : version) {
        if (c == '.') {
            ++dotCount;
            continue;
        }
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            validVersion = false;
            break;
        }
    }
    if (!validVersion || dotCount != 2) {
        report.Add(Severity::kError, "integrity", "dominus_version '" + version + "' is not valid semver (expected N.N.N)");
    }
}

ValidationReport PackageValidator::ValidateFile(const std::filesystem::path& dominusPath,
                                                 const std::filesystem::path& baseDir) {
    ValidationReport report;

    CheckDeterministicRebuild(dominusPath, report);

    auto loadResult = core::DominusSerializer::Load(dominusPath);
    if (!loadResult.ok) {
        report.Add(Severity::kError, "integrity", "failed to load: " + loadResult.error);
        return report;
    }
    auto& obj = *loadResult.value;

    // Asset ownership checks the raw refs -- works whether or not binding
    // succeeds, and a missing file is exactly what would make binding
    // fail anyway, so checking it first gives a clearer error message
    // than whatever RigBinder/CombatBinder would surface.
    CheckAssetOwnership(obj, baseDir, report);

    auto rigResult = character::RigBinder::Bind(obj, baseDir);
    if (!rigResult.ok) {
        report.Add(Severity::kError, "integrity", "RigBinder::Bind failed: " + rigResult.error);
    }
    auto combatResult = combat::CombatBinder::Bind(obj, baseDir);
    if (!combatResult.ok) {
        report.Add(Severity::kError, "integrity", "CombatBinder::Bind failed: " + combatResult.error);
    }

    CheckIdentityCompleteness(obj, report);
    CheckNamingConsistency(obj, report);  // object_id check + post-bind move/clip name checks, one pass

    if (auto* moves = obj.GetComponent<combat::MoveSetComponent>()) {
        if (auto* graphComp = obj.GetComponent<character::MotionGraphComponent>()) {
            auto coverage = combat::AssetValidation::CheckMotionCoverage(*moves, graphComp->graph);
            for (auto& name : coverage.unreachable_moves) {
                report.Add(Severity::kError, "motion_coverage", "move '" + name + "' has no matching motion graph transition");
            }
        }
    }

    return report;
}

}  // namespace dominus::validation
