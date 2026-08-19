// REALITY/BrooklynDomainCompilers.cpp
#include "REALITY/BrooklynDomainCompilers.h"

#include "CHARACTER/Rig/RigBinder.h"
#include "CORE/Serialization/DominusSerializer.h"
#include "REGISTRY/Hash/Sha256.h"
#include "RIG/CharacterAcceptanceHarness.h"
#include "VISUALFORGE/AssetSpecification.h"
#include "VISUALFORGE/CharacterBlueprint.h"
#include "VISUALFORGE/RendererPackage.h"
#include "VISUALFORGE/RendererPackageAcceptanceHarness.h"

namespace dominus::reality::internal {

namespace {

rig::BoneMap BrooklynBoneMap() {
    return {{"root", "root"},    {"torso", "chest"}, {"head", "head"},   {"arm_r", "hand_R"},
            {"arm_l", "hand_L"}, {"leg_r", "foot_R"}, {"leg_l", "foot_L"}};
}

rig::ClipPairs BrooklynClipPairs(const std::filesystem::path& dir) {
    std::vector<std::string> names = {"idle",          "attack",     "air_combo",          "block_impact",
                                       "combo_starter", "counter",    "dodge",              "knockback",
                                       "knockdown",     "knockdown_recovery", "launcher",   "stagger",
                                       "transformation"};
    rig::ClipPairs pairs;
    for (const auto& n : names) {
        pairs.push_back({dir / ("brooklyn_" + n + ".clip.json"), dir / ("canonical_brooklyn_" + n + ".clip.json")});
    }
    return pairs;
}

}  // namespace

rig::AcceptanceCertificate CompileRig(const std::filesystem::path& dir) {
    std::vector<rig::AcceptanceSection> sections = {
        rig::CharacterAcceptanceHarness::CheckSkeleton(dir / "brooklyn.skel.json",
                                                         dir / "brooklyn_canonical.skel.json", BrooklynBoneMap()),
        rig::CharacterAcceptanceHarness::CheckAnimation(dir / "brooklyn.skel.json",
                                                          dir / "brooklyn_canonical.skel.json", BrooklynBoneMap(),
                                                          BrooklynClipPairs(dir)),
        rig::CharacterAcceptanceHarness::CheckCombat(dir / "brooklyn_canonical.skel.json",
                                                       dir / "brooklyn_canonical_hurtboxes.json",
                                                       dir / "canonical_brooklyn_move_jab.json"),
        rig::CharacterAcceptanceHarness::CheckRuntime(dir / "brooklyn_canonical.dominus", dir,
                                                        dir / "brooklyn_canonical_hurtboxes.json", "jab"),
        rig::CharacterAcceptanceHarness::CheckDeterminism(dir / "brooklyn_canonical.dominus", dir,
                                                            dir / "brooklyn_canonical_hurtboxes.json", "jab"),
    };
    return rig::AcceptanceCertificateForge::Generate("brooklyn", "brooklyn_canonical_identity_v1", sections);
}

std::optional<VisualForgeCompileResult> CompileVisualForge(const std::filesystem::path& dominusPath) {
    std::filesystem::path baseDir = dominusPath.parent_path();

    auto loadResult = core::DominusSerializer::Load(dominusPath);
    if (!loadResult.ok) return std::nullopt;
    auto& obj = *loadResult.value;
    character::RigBinder::Bind(obj, baseDir);

    auto* visual = obj.GetComponent<character::VisualGenomeComponent>();
    if (!visual) return std::nullopt;
    auto* material = obj.GetComponent<character::MaterialGenomeComponent>();
    auto* visualStyle = obj.GetComponent<character::VisualStyleGenomeComponent>();

    auto blueprint = visualforge::CharacterBlueprintForge::Build(obj.Id(), visual->genome,
                                                                   material ? &material->genome : nullptr,
                                                                   visualStyle ? &visualStyle->genome : nullptr);
    auto assets = visualforge::AssetSpecificationForge::Build(blueprint);
    auto deps = visualforge::DependencyGraphForge::Build(obj.Id(), visual->genome,
                                                           material ? &material->genome : nullptr,
                                                           visualStyle ? &visualStyle->genome : nullptr);
    auto packageResult = visualforge::RendererPackageForge::Build(obj.Id(), "t", blueprint, assets, deps);
    if (!packageResult.ok) return std::nullopt;

    std::vector<visualforge::AcceptanceSection> sections = {
        visualforge::RendererPackageAcceptanceHarness::CheckBlueprintValidity(blueprint),
        visualforge::RendererPackageAcceptanceHarness::CheckDependencyIntegrity(deps),
        visualforge::RendererPackageAcceptanceHarness::CheckPackageDeterminism(obj.Id(), "t", blueprint, assets,
                                                                                 deps, std::nullopt),
        visualforge::RendererPackageAcceptanceHarness::CheckProvenanceCompleteness(*packageResult.package, deps),
        visualforge::RendererPackageAcceptanceHarness::CheckRenderable(blueprint),
        visualforge::RendererPackageAcceptanceHarness::CheckRenderDeterminism(blueprint),
        visualforge::RendererPackageAcceptanceHarness::RenderedOutputNotDeclared(),
    };
    VisualForgeCompileResult out;
    out.certificate = visualforge::VisualAcceptanceCertificateForge::Generate(
        obj.Id(), packageResult.package->package_hash, sections);
    out.dependencies = deps;
    return out;
}

const rig::AcceptanceSection* FindRigSection(const rig::AcceptanceCertificate& cert, const std::string& name) {
    for (const auto& s : cert.sections) {
        if (s.name == name) return &s;
    }
    return nullptr;
}

std::string ComputeRealityArtifactHash(const std::string& entity, const std::string& compilerIdentity,
                                        const std::string& rigCertificateHash,
                                        const std::string& visualforgeCertificateHash) {
    return registry::Sha256::Hash("entity=" + entity + ";compiler=" + compilerIdentity +
                                   ";rig_certificate_hash=" + rigCertificateHash +
                                   ";visualforge_certificate_hash=" + visualforgeCertificateHash);
}

}  // namespace dominus::reality::internal
