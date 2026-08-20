// tests/character/test_hitm_rig_placement.cpp
// ROADMAP.md Track H Module 5B. Fixtures under
// tests/fixtures/hitm_sprite_assets/ are real, byte-identical copies of
// HITM's generated rig.json (see that directory's README.md for
// provenance) plus deliberate single-field mutations of Brooklyn's real
// file.
#include "CHARACTER/HitmBridge/HitmRigPlacement.h"
#include "CHARACTER/HitmBridge/HitmPartsRig.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::character::hitm::HitmPartsRig;
using dominus::character::hitm::HitmRigPlacement;

namespace {

std::filesystem::path CharacterDir(const std::string& root, const std::string& fighter) {
    std::vector<std::filesystem::path> candidates = {
        std::filesystem::path("tests/fixtures/hitm_sprite_assets") / root / "data/characters" / fighter,
        std::filesystem::path("../tests/fixtures/hitm_sprite_assets") / root / "data/characters" / fighter,
        std::filesystem::path("../../tests/fixtures/hitm_sprite_assets") / root / "data/characters" / fighter,
    };
    for (auto& c : candidates) {
        if (std::filesystem::exists(c)) return c;
    }
    throw std::runtime_error("Fixture dir not found: " + root + "/" + fighter);
}

std::filesystem::path RealCharacterDir(const std::string& fighter) {
    std::vector<std::filesystem::path> candidates = {
        std::filesystem::path("tests/fixtures/hitm_sprite_assets/data/characters") / fighter,
        std::filesystem::path("../tests/fixtures/hitm_sprite_assets/data/characters") / fighter,
        std::filesystem::path("../../tests/fixtures/hitm_sprite_assets/data/characters") / fighter,
    };
    for (auto& c : candidates) {
        if (std::filesystem::exists(c)) return c;
    }
    throw std::runtime_error("Fixture dir not found: data/characters/" + fighter);
}

}  // namespace

// --- Real data: Brooklyn, standalone (no cross-check) ---------------------

DOMINUS_TEST(HitmRigPlacement_ImportsRealBrooklynStandalone) {
    auto result = HitmRigPlacement::Import(RealCharacterDir("brooklyn"));
    DOMINUS_EXPECT(result.ok);
    auto& rig = *result.value;
    DOMINUS_EXPECT(rig.FighterId() == "brooklyn");
    DOMINUS_EXPECT(rig.HasPart("torso"));
    const auto* torso = rig.Part("torso");
    DOMINUS_EXPECT(torso != nullptr);
    DOMINUS_EXPECT(torso->rect_x0 == 0.26);
    DOMINUS_EXPECT(torso->rect_y0 == 0.275);
    DOMINUS_EXPECT(torso->rect_x1 == 0.74);
    DOMINUS_EXPECT(torso->rect_y1 == 0.605);
    DOMINUS_EXPECT(torso->pivot_x == 0.5);
    DOMINUS_EXPECT(torso->pivot_y == 0.1);
}

// --- Real data: cross-checked against the real, sibling parts.json --------

DOMINUS_TEST(HitmRigPlacement_ImportsRealBrooklynWithCrossCheck_Passes) {
    auto parts = *HitmPartsRig::Import(RealCharacterDir("brooklyn")).value;
    auto result = HitmRigPlacement::Import(RealCharacterDir("brooklyn"), &parts);
    DOMINUS_EXPECT(result.ok);
}

DOMINUS_TEST(HitmRigPlacement_RealBrooklynPivotsAgreeWithPartsJsonForEveryPart) {
    // The real, evidenced correspondence this module's README documents:
    // rig.json's pivot equals parts.json's pivot for every one of
    // Brooklyn's 22 real parts, not asserted for a sample.
    auto parts = *HitmPartsRig::Import(RealCharacterDir("brooklyn")).value;
    auto rig = *HitmRigPlacement::Import(RealCharacterDir("brooklyn")).value;
    int checked = 0;
    for (const auto& p : parts.Parts()) {
        const auto* placement = rig.Part(p.name);
        DOMINUS_EXPECT(placement != nullptr);
        DOMINUS_EXPECT(placement->pivot_x == p.pivot_x);
        DOMINUS_EXPECT(placement->pivot_y == p.pivot_y);
        ++checked;
    }
    DOMINUS_EXPECT(checked == 22);
}

DOMINUS_TEST(HitmRigPlacement_ImportsRealRocketAndStatic) {
    auto rocketParts = *HitmPartsRig::Import(RealCharacterDir("rocket")).value;
    auto rocketRig = HitmRigPlacement::Import(RealCharacterDir("rocket"), &rocketParts);
    DOMINUS_EXPECT(rocketRig.ok);

    auto staticParts = *HitmPartsRig::Import(RealCharacterDir("static")).value;
    auto staticRig = HitmRigPlacement::Import(RealCharacterDir("static"), &staticParts);
    DOMINUS_EXPECT(staticRig.ok);
}

// --- Deliberate-break: every real-world corruption mode must fail loud ----

DOMINUS_TEST(HitmRigPlacement_Break_MissingPart_FailsCrossCheck) {
    auto parts = *HitmPartsRig::Import(RealCharacterDir("brooklyn")).value;  // real, unmodified parts.json (has "torso")
    auto result = HitmRigPlacement::Import(CharacterDir("broken_rig_missing_part", "brooklyn"), &parts);
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("torso") != std::string::npos);
}

DOMINUS_TEST(HitmRigPlacement_Break_MissingPart_StandaloneStillImports) {
    // Without a cross-check, a rig.json missing one part is not itself
    // malformed JSON -- it imports fine on its own terms; the real defect
    // (parts.json and rig.json disagreeing) is only detectable by
    // comparing the two real files, which is exactly what the cross-check
    // parameter exists to do.
    auto result = HitmRigPlacement::Import(CharacterDir("broken_rig_missing_part", "brooklyn"));
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(!result.value->HasPart("torso"));
}

DOMINUS_TEST(HitmRigPlacement_Break_PivotMismatch_FailsCrossCheck) {
    auto parts = *HitmPartsRig::Import(RealCharacterDir("brooklyn")).value;  // real pivot: [0.5, 0.1]
    auto result = HitmRigPlacement::Import(CharacterDir("broken_rig_pivot_mismatch", "brooklyn"), &parts);
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("pivot disagrees") != std::string::npos);
}

DOMINUS_TEST(HitmRigPlacement_Break_MissingDirectory_Fails) {
    auto result = HitmRigPlacement::Import(std::filesystem::path("tests/fixtures/hitm_sprite_assets/does_not_exist"));
    DOMINUS_EXPECT(!result.ok);
}
