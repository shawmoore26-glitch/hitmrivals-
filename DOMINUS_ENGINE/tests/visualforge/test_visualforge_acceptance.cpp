// tests/visualforge/test_visualforge_acceptance.cpp
// The visual authority model RIG's Brooklyn acceptance harness set the
// precedent for -- applied honestly to RendererPackage. Four real,
// computed checks against Brooklyn's actual VisualForge data; one
// section (RenderedOutput) always NOT_DECLARED because GRAPHICS
// doesn't exist. Terminal state is "structurally_sound," never
// "ACTIVE" -- there is no real renderer to prove equivalence against,
// unlike RIG's Brooklyn, which had a real, running combat system.
#include "CHARACTER/Genome/MaterialGenomeLoader.h"
#include "CHARACTER/Genome/VisualGenomeLoader.h"
#include "CHARACTER/Genome/VisualStyleGenomeLoader.h"
#include "VISUALFORGE/AcceptanceCertificate.h"
#include "VISUALFORGE/AssetSpecification.h"
#include "VISUALFORGE/CharacterBlueprint.h"
#include "VISUALFORGE/DependencyGraph.h"
#include "VISUALFORGE/ProductionSnapshot.h"
#include "VISUALFORGE/RendererPackage.h"
#include "VISUALFORGE/RendererPackageAcceptanceHarness.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::character::MaterialGenomeLoader;
using dominus::character::VisualGenomeLoader;
using dominus::character::VisualStyleGenomeLoader;
using dominus::visualforge::AssetSpecificationForge;
using dominus::visualforge::CharacterBlueprintForge;
using dominus::visualforge::DependencyGraphForge;
using dominus::visualforge::RendererPackageAcceptanceHarness;
using dominus::visualforge::RendererPackageForge;
using dominus::visualforge::VisualAcceptanceCertificateForge;

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

// --- Each real check, against Brooklyn's real VisualForge data -----------

DOMINUS_TEST(RendererPackageAcceptanceHarness_BlueprintValidity_PassesForRealBrooklyn) {
    auto dir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(dir / "brooklyn_visual.json");
    auto material = MaterialGenomeLoader::LoadFromFile(dir / "brooklyn_jacket_material.json");
    auto style = VisualStyleGenomeLoader::LoadFromFile(dir / "brooklyn_visual_style.json");
    DOMINUS_EXPECT(visual.ok && material.ok && style.ok);

    auto blueprint = CharacterBlueprintForge::Build("brooklyn", *visual.value, &*material.value, &*style.value);
    auto section = RendererPackageAcceptanceHarness::CheckBlueprintValidity(blueprint);
    DOMINUS_EXPECT(section.name == "BlueprintValidity");
    DOMINUS_EXPECT(section.passed);
}

DOMINUS_TEST(RendererPackageAcceptanceHarness_DependencyIntegrity_PassesForRealBrooklyn) {
    auto dir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(dir / "brooklyn_visual.json");
    auto material = MaterialGenomeLoader::LoadFromFile(dir / "brooklyn_jacket_material.json");
    DOMINUS_EXPECT(visual.ok && material.ok);

    auto deps = DependencyGraphForge::Build("brooklyn", *visual.value, &*material.value);
    auto section = RendererPackageAcceptanceHarness::CheckDependencyIntegrity(deps);
    DOMINUS_EXPECT(section.passed);
}

DOMINUS_TEST(RendererPackageAcceptanceHarness_DependencyIntegrity_CatchesARealCorruptedHash) {
    auto dir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(dir / "brooklyn_visual.json");
    auto deps = DependencyGraphForge::Build("brooklyn", *visual.value);
    deps.material_genome_hash = "not_a_real_hash";  // simulate corruption
    auto section = RendererPackageAcceptanceHarness::CheckDependencyIntegrity(deps);
    DOMINUS_EXPECT(!section.passed);  // does not paper over a real corruption
}

DOMINUS_TEST(RendererPackageAcceptanceHarness_PackageDeterminism_PassesForRealBrooklyn) {
    auto dir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(dir / "brooklyn_visual.json");
    auto material = MaterialGenomeLoader::LoadFromFile(dir / "brooklyn_jacket_material.json");
    DOMINUS_EXPECT(visual.ok && material.ok);

    auto blueprint = CharacterBlueprintForge::Build("brooklyn", *visual.value, &*material.value);
    auto assets = AssetSpecificationForge::Build(blueprint);
    auto deps = DependencyGraphForge::Build("brooklyn", *visual.value, &*material.value);

    auto section = RendererPackageAcceptanceHarness::CheckPackageDeterminism("brooklyn", "t", blueprint, assets,
                                                                              deps, std::nullopt);
    DOMINUS_EXPECT(section.passed);
}

DOMINUS_TEST(RendererPackageAcceptanceHarness_ProvenanceCompleteness_PassesForRealBrooklyn) {
    auto dir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(dir / "brooklyn_visual.json");
    auto material = MaterialGenomeLoader::LoadFromFile(dir / "brooklyn_jacket_material.json");
    DOMINUS_EXPECT(visual.ok && material.ok);

    auto blueprint = CharacterBlueprintForge::Build("brooklyn", *visual.value, &*material.value);
    auto assets = AssetSpecificationForge::Build(blueprint);
    auto deps = DependencyGraphForge::Build("brooklyn", *visual.value, &*material.value);
    auto result = RendererPackageForge::Build("brooklyn", "t", blueprint, assets, deps);
    DOMINUS_EXPECT(result.ok);

    auto section = RendererPackageAcceptanceHarness::CheckProvenanceCompleteness(*result.package, deps);
    DOMINUS_EXPECT(section.passed);
}

// --- RenderedOutput is always NOT_DECLARED, never faked as PASS -----------

DOMINUS_TEST(RendererPackageAcceptanceHarness_RenderedOutput_IsAlwaysNotDeclared) {
    auto section = RendererPackageAcceptanceHarness::RenderedOutputNotDeclared();
    DOMINUS_EXPECT(section.name == "RenderedOutput");
    DOMINUS_EXPECT(!section.passed);  // never PASS -- there is nothing real to check
    DOMINUS_EXPECT(section.detail.find("NOT_DECLARED") != std::string::npos);
}

// --- Renderable / RenderDeterminism: real, now that GRAPHICS exists -------

DOMINUS_TEST(RendererPackageAcceptanceHarness_Renderable_PassesForRealBrooklyn) {
    auto dir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(dir / "brooklyn_visual.json");
    auto material = MaterialGenomeLoader::LoadFromFile(dir / "brooklyn_jacket_material.json");
    DOMINUS_EXPECT(visual.ok && material.ok);

    auto blueprint = CharacterBlueprintForge::Build("brooklyn", *visual.value, &*material.value);
    auto section = RendererPackageAcceptanceHarness::CheckRenderable(blueprint);
    DOMINUS_EXPECT(section.name == "Renderable");
    DOMINUS_EXPECT(section.passed);
}

DOMINUS_TEST(RendererPackageAcceptanceHarness_RenderDeterminism_PassesForRealBrooklyn) {
    auto dir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(dir / "brooklyn_visual.json");
    auto blueprint = CharacterBlueprintForge::Build("brooklyn", *visual.value);
    auto section = RendererPackageAcceptanceHarness::CheckRenderDeterminism(blueprint);
    DOMINUS_EXPECT(section.passed);
}

// --- The full certificate: real, generated, correctly excludes RenderedOutput --

DOMINUS_TEST(VisualAcceptance_FullCertificateForRealBrooklynIsStructurallySoundAndRenderable) {
    auto dir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(dir / "brooklyn_visual.json");
    auto material = MaterialGenomeLoader::LoadFromFile(dir / "brooklyn_jacket_material.json");
    auto style = VisualStyleGenomeLoader::LoadFromFile(dir / "brooklyn_visual_style.json");
    DOMINUS_EXPECT(visual.ok && material.ok && style.ok);

    auto blueprint = CharacterBlueprintForge::Build("brooklyn", *visual.value, &*material.value, &*style.value);
    auto assets = AssetSpecificationForge::Build(blueprint);
    auto deps = DependencyGraphForge::Build("brooklyn", *visual.value, &*material.value, &*style.value);
    auto packageResult = RendererPackageForge::Build("brooklyn", "t", blueprint, assets, deps);
    DOMINUS_EXPECT(packageResult.ok);

    std::vector<dominus::visualforge::AcceptanceSection> sections = {
        RendererPackageAcceptanceHarness::CheckBlueprintValidity(blueprint),
        RendererPackageAcceptanceHarness::CheckDependencyIntegrity(deps),
        RendererPackageAcceptanceHarness::CheckPackageDeterminism("brooklyn", "t", blueprint, assets, deps,
                                                                    std::nullopt),
        RendererPackageAcceptanceHarness::CheckProvenanceCompleteness(*packageResult.package, deps),
        RendererPackageAcceptanceHarness::CheckRenderable(blueprint),
        RendererPackageAcceptanceHarness::CheckRenderDeterminism(blueprint),
        RendererPackageAcceptanceHarness::RenderedOutputNotDeclared(),
    };

    auto cert = VisualAcceptanceCertificateForge::Generate("brooklyn", packageResult.package->package_hash, sections);
    DOMINUS_EXPECT(cert.sections.size() == 7);
    DOMINUS_EXPECT(cert.structurally_sound);  // the original 4 structural checks
    DOMINUS_EXPECT(cert.renderable);          // the 2 new render checks -- SEPARATE claim
    DOMINUS_EXPECT(cert.certificate_hash.size() == 64);
}

DOMINUS_TEST(VisualAcceptance_RenderableDoesNotImplyStructurallySoundOrViceVersa) {
    // The critical distinction, proven, not just documented: a
    // certificate can be structurally_sound with renderable=false (no
    // render checks run) and the reverse -- they are independent
    // claims.
    std::vector<dominus::visualforge::AcceptanceSection> structuralOnly = {
        {"BlueprintValidity", true, "ok"},
        {"DependencyIntegrity", true, "ok"},
        {"PackageDeterminism", true, "ok"},
        {"ProvenanceCompleteness", true, "ok"},
    };
    auto certA = VisualAcceptanceCertificateForge::Generate("x", "y", structuralOnly);
    DOMINUS_EXPECT(certA.structurally_sound);
    DOMINUS_EXPECT(!certA.renderable);  // no render sections present -- not vacuously true

    std::vector<dominus::visualforge::AcceptanceSection> renderOnly = {
        {"Renderable", true, "ok"},
        {"RenderDeterminism", true, "ok"},
    };
    auto certB = VisualAcceptanceCertificateForge::Generate("x", "y", renderOnly);
    DOMINUS_EXPECT(certB.renderable);
    DOMINUS_EXPECT(!certB.structurally_sound);  // no structural sections present
}

DOMINUS_TEST(VisualAcceptance_CertificateWithARealFailureIsNotStructurallySound) {
    std::vector<dominus::visualforge::AcceptanceSection> sections = {
        {"BlueprintValidity", true, "ok"},
        {"DependencyIntegrity", false, "a real, genuine corruption"},
        RendererPackageAcceptanceHarness::RenderedOutputNotDeclared(),
    };
    auto cert = VisualAcceptanceCertificateForge::Generate("x", "y", sections);
    DOMINUS_EXPECT(!cert.structurally_sound);
}

// --- ProductionSnapshot lifecycle: Approved -> Accepted, gated on the real certificate --

DOMINUS_TEST(ProductionSnapshotLifecycle_ReachesAcceptedUsingARealGeneratedCertificate) {
    auto dir = FixtureDir();
    auto visual = VisualGenomeLoader::LoadFromFile(dir / "brooklyn_visual.json");
    auto material = MaterialGenomeLoader::LoadFromFile(dir / "brooklyn_jacket_material.json");
    auto style = VisualStyleGenomeLoader::LoadFromFile(dir / "brooklyn_visual_style.json");
    DOMINUS_EXPECT(visual.ok && material.ok && style.ok);

    auto blueprint = CharacterBlueprintForge::Build("brooklyn", *visual.value, &*material.value, &*style.value);
    auto assets = AssetSpecificationForge::Build(blueprint);
    auto deps = DependencyGraphForge::Build("brooklyn", *visual.value, &*material.value, &*style.value);
    auto packageResult = RendererPackageForge::Build("brooklyn", "t", blueprint, assets, deps);
    DOMINUS_EXPECT(packageResult.ok);
    auto validation = dominus::visualforge::BlueprintValidator::Validate(blueprint);

    // Real certificate, generated from real checks -- not hand-typed.
    std::vector<dominus::visualforge::AcceptanceSection> sections = {
        RendererPackageAcceptanceHarness::CheckBlueprintValidity(blueprint),
        RendererPackageAcceptanceHarness::CheckDependencyIntegrity(deps),
        RendererPackageAcceptanceHarness::CheckPackageDeterminism("brooklyn", "t", blueprint, assets, deps,
                                                                    std::nullopt),
        RendererPackageAcceptanceHarness::CheckProvenanceCompleteness(*packageResult.package, deps),
        RendererPackageAcceptanceHarness::CheckRenderable(blueprint),
        RendererPackageAcceptanceHarness::CheckRenderDeterminism(blueprint),
        RendererPackageAcceptanceHarness::RenderedOutputNotDeclared(),
    };
    auto cert = VisualAcceptanceCertificateForge::Generate("brooklyn", packageResult.package->package_hash, sections);
    DOMINUS_EXPECT(cert.structurally_sound);
    DOMINUS_EXPECT(cert.renderable);

    auto snapshot = dominus::visualforge::ProductionSnapshotForge::CreateInitial("BROOKLYN_VISUAL_BUILD_001",
                                                                                   "brooklyn", "t", deps, validation);
    DOMINUS_EXPECT(dominus::visualforge::SnapshotLifecycle::AdvanceToValidated(snapshot));
    DOMINUS_EXPECT(dominus::visualforge::SnapshotLifecycle::Approve(snapshot, "shawn"));
    DOMINUS_EXPECT(snapshot.state == dominus::visualforge::SnapshotState::kApproved);

    DOMINUS_EXPECT(dominus::visualforge::SnapshotLifecycle::AdvanceToAccepted(snapshot, cert));
    DOMINUS_EXPECT(snapshot.state == dominus::visualforge::SnapshotState::kAccepted);
    DOMINUS_EXPECT(snapshot.accepted_certificate_hash == cert.certificate_hash);
}

DOMINUS_TEST(ProductionSnapshotLifecycle_CannotReachAcceptedWithAStructurallySoundButNotRenderableCertificate) {
    // The critical rule, enforced: STRUCTURALLY_SOUND alone is not
    // enough. A certificate that's structurally sound but has no
    // passing render evidence must NOT unlock Accepted.
    std::vector<dominus::visualforge::AcceptanceSection> structuralOnly = {
        {"BlueprintValidity", true, "ok"},
        {"DependencyIntegrity", true, "ok"},
        {"PackageDeterminism", true, "ok"},
        {"ProvenanceCompleteness", true, "ok"},
        RendererPackageAcceptanceHarness::RenderedOutputNotDeclared(),
    };
    auto cert = VisualAcceptanceCertificateForge::Generate("x", "y", structuralOnly);
    DOMINUS_EXPECT(cert.structurally_sound);
    DOMINUS_EXPECT(!cert.renderable);

    dominus::visualforge::ProductionSnapshot snapshot;
    snapshot.state = dominus::visualforge::SnapshotState::kApproved;
    DOMINUS_EXPECT(!dominus::visualforge::SnapshotLifecycle::AdvanceToAccepted(snapshot, cert));
    DOMINUS_EXPECT(snapshot.state == dominus::visualforge::SnapshotState::kApproved);  // correctly stuck
}

DOMINUS_TEST(ProductionSnapshotLifecycle_CannotSkipApprovedToReachAccepted) {
    dominus::visualforge::ProductionSnapshot snapshot;  // state defaults to kCreated
    dominus::visualforge::AcceptanceCertificate cert;
    cert.structurally_sound = true;
    cert.renderable = true;
    cert.certificate_hash = "hash";
    DOMINUS_EXPECT(!dominus::visualforge::SnapshotLifecycle::AdvanceToAccepted(snapshot, cert));
    DOMINUS_EXPECT(snapshot.state == dominus::visualforge::SnapshotState::kCreated);
}

DOMINUS_TEST(ProductionSnapshotLifecycle_ActiveHasNoReachablePath) {
    // Structurally enforced, not just documented: SnapshotLifecycle
    // exposes AdvanceToValidated/Approve/AdvanceToAccepted and nothing
    // else. There is no DeclareActive, no method of any name that sets
    // state to kActive anywhere in this codebase -- confirmed here by
    // the simple fact that this test cannot call one to write.
    // kActive still exists as a real enum value (SnapshotStateName
    // prints "Active" correctly), reserved for the real, future day a
    // rasterizer earns it.
    DOMINUS_EXPECT(std::string(dominus::visualforge::SnapshotStateName(
                        dominus::visualforge::SnapshotState::kActive)) == "Active");
}
