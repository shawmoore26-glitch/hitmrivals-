// tests/character/test_hitm_parts_rig.cpp
// ROADMAP.md Track H Module 3. Fixtures under
// tests/fixtures/hitm_parts_rig/ are real, byte-identical copies of
// HITM's generated parts.json (see that directory's README.md for
// provenance) plus deliberate single-field mutations of Brooklyn's real
// file.
#include "CHARACTER/HitmBridge/HitmPartsRig.h"
#include "tests/TestFramework.h"

#include <filesystem>
#include <fstream>
#include <sstream>

using dominus::character::hitm::HitmPartsRig;
using dominus::core::json::Value;

namespace {

std::filesystem::path FixtureDir(const std::string& name) {
    std::vector<std::filesystem::path> candidates = {
        std::filesystem::path("tests/fixtures/hitm_parts_rig") / name,
        std::filesystem::path("../tests/fixtures/hitm_parts_rig") / name,
        std::filesystem::path("../../tests/fixtures/hitm_parts_rig") / name,
    };
    for (auto& c : candidates) {
        if (std::filesystem::exists(c)) return c;
    }
    throw std::runtime_error("Fixture dir not found: " + name);
}

const dominus::character::hitm::HitmAtlasPart* FindPart(const HitmPartsRig& rig, const std::string& name) {
    for (const auto& p : rig.Parts()) {
        if (p.name == name) return &p;
    }
    return nullptr;
}

int CountBonesNamed(const HitmPartsRig& rig, const std::string& name) {
    int count = 0;
    for (const auto& b : rig.Bones()) {
        if (b.name == name) ++count;
    }
    return count;
}

}  // namespace

// --- Real data: Brooklyn ------------------------------------------------

DOMINUS_TEST(HitmPartsRig_ImportsRealBrooklyn) {
    auto result = HitmPartsRig::Import(FixtureDir("brooklyn"));
    DOMINUS_EXPECT(result.ok);
    auto& rig = *result.value;

    DOMINUS_EXPECT(rig.FighterId() == "brooklyn");
    DOMINUS_EXPECT(rig.Atlas() == "brooklyn_atlas");
    DOMINUS_EXPECT(rig.SourceWidth() == 206);
    DOMINUS_EXPECT(rig.SourceHeight() == 480);
    DOMINUS_EXPECT(rig.HandBone() == "handNear");
    DOMINUS_EXPECT(rig.HandPointX() == 0.5);
    DOMINUS_EXPECT(rig.HandPointY() == 0.55);
    DOMINUS_EXPECT(rig.Parts().size() == 22);
    DOMINUS_EXPECT(rig.DrawOrder().size() == 22);
}

DOMINUS_TEST(HitmPartsRig_BrooklynTorsoPartMatchesRealData) {
    auto rig = *HitmPartsRig::Import(FixtureDir("brooklyn")).value;
    const auto* torso = FindPart(rig, "torso");
    DOMINUS_EXPECT(torso != nullptr);
    DOMINUS_EXPECT(torso->pivot_x == 0.5);
    DOMINUS_EXPECT(torso->pivot_y == 0.1);
    DOMINUS_EXPECT(torso->frame_x == 104);
    DOMINUS_EXPECT(torso->frame_y == 0);
    DOMINUS_EXPECT(torso->frame_w == 107);
    DOMINUS_EXPECT(torso->frame_h == 166);
}

DOMINUS_TEST(HitmPartsRig_BrooklynDuplicateBoneNamesBothPreserved) {
    // The real, non-obvious finding this module's header documents:
    // handFar/handNear each appear TWICE in the real data -- once rigid,
    // once as a glove-bounce secondary-motion overlay. Both must survive.
    auto rig = *HitmPartsRig::Import(FixtureDir("brooklyn")).value;
    DOMINUS_EXPECT(CountBonesNamed(rig, "handFar") == 2);
    DOMINUS_EXPECT(CountBonesNamed(rig, "handNear") == 2);

    // Find the two handNear entries and confirm they are genuinely
    // different (one rigid, one a spring), not an accidental duplicate.
    int rigidCount = 0, springCount = 0;
    for (const auto& b : rig.Bones()) {
        if (b.name != "handNear") continue;
        if (b.follow.has_value()) {
            ++springCount;
            DOMINUS_EXPECT(b.follow->max_angle == 22.0);
        } else {
            ++rigidCount;
        }
    }
    DOMINUS_EXPECT(rigidCount == 1);
    DOMINUS_EXPECT(springCount == 1);
}

DOMINUS_TEST(HitmPartsRig_BrooklynSecondaryMotionBonesHaveRealFollowParams) {
    auto rig = *HitmPartsRig::Import(FixtureDir("brooklyn")).value;
    bool foundDreadFar = false;
    for (const auto& b : rig.Bones()) {
        if (b.name == "dreadFar" && b.follow.has_value()) {
            foundDreadFar = true;
            DOMINUS_EXPECT(b.follow->lag_beats == 1.4);
            DOMINUS_EXPECT(b.follow->max_angle == 46.0);
            DOMINUS_EXPECT(b.follow->gravity == 0.5);
            DOMINUS_EXPECT(b.why.find("Volume 5") != std::string::npos);
        }
    }
    DOMINUS_EXPECT(foundDreadFar);

    // "root" bone: no parent, no len, no follow.
    bool foundRoot = false;
    for (const auto& b : rig.Bones()) {
        if (b.name == "root") {
            foundRoot = true;
            DOMINUS_EXPECT(!b.parent.has_value());
            DOMINUS_EXPECT(!b.len.has_value());
            DOMINUS_EXPECT(!b.follow.has_value());
        }
    }
    DOMINUS_EXPECT(foundRoot);
}

// --- Real data: Rocket and Static (different secondary-motion sets) -----

DOMINUS_TEST(HitmPartsRig_ImportsRealRocket_DifferentPartsAndBones) {
    auto result = HitmPartsRig::Import(FixtureDir("rocket"));
    DOMINUS_EXPECT(result.ok);
    auto& rig = *result.value;
    DOMINUS_EXPECT(rig.FighterId() == "rocket");
    DOMINUS_EXPECT(rig.Atlas() == "rocket_atlas");
    DOMINUS_EXPECT(rig.SourceWidth() == 244);
    DOMINUS_EXPECT(rig.Parts().size() == 19);

    // Rocket-only real parts -- not present on Brooklyn.
    DOMINUS_EXPECT(FindPart(rig, "bandana") != nullptr);
    DOMINUS_EXPECT(FindPart(rig, "tail") != nullptr);
    DOMINUS_EXPECT(FindPart(rig, "collarChain") != nullptr);
    DOMINUS_EXPECT(FindPart(rig, "hat") == nullptr);  // Brooklyn-only, must not leak across fighters

    DOMINUS_EXPECT(CountBonesNamed(rig, "handFar") == 2);  // same real glove-bounce pattern as Brooklyn
}

DOMINUS_TEST(HitmPartsRig_ImportsRealStatic_DifferentPartsAndBones) {
    auto result = HitmPartsRig::Import(FixtureDir("static"));
    DOMINUS_EXPECT(result.ok);
    auto& rig = *result.value;
    DOMINUS_EXPECT(rig.FighterId() == "static");
    DOMINUS_EXPECT(rig.Atlas() == "static_atlas");
    DOMINUS_EXPECT(rig.Parts().size() == 19);

    DOMINUS_EXPECT(FindPart(rig, "antenna") != nullptr);
    DOMINUS_EXPECT(FindPart(rig, "wireFar") != nullptr);
    DOMINUS_EXPECT(FindPart(rig, "coatStripNear") != nullptr);
    DOMINUS_EXPECT(FindPart(rig, "dreadFar") == nullptr);  // Brooklyn-only
}

// --- Losslessness: import -> export must reproduce the exact source tree ---

DOMINUS_TEST(HitmPartsRig_ExportRoundTripsBrooklynWithZeroLoss) {
    auto rig = *HitmPartsRig::Import(FixtureDir("brooklyn")).value;
    std::string sourceText = [] {
        std::ifstream in(FixtureDir("brooklyn") / "parts.json", std::ios::binary);
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }();
    Value sourceParsed = Value::Parse(sourceText);

    // Same proof method as Module 2: canonical dump of the independently
    // re-parsed source file must equal the canonical dump of what
    // ToJson() returns. If ExtractBone/ExtractPart or anything else in
    // Import() had reconstructed a tree instead of holding raw_ verbatim,
    // this is what would catch it.
    DOMINUS_EXPECT(sourceParsed.Dump() == rig.ToJson().Dump());

    Value reparsed = Value::Parse(rig.ToJson().Dump());
    DOMINUS_EXPECT(reparsed.Dump() == rig.ToJson().Dump());
}

DOMINUS_TEST(HitmPartsRig_ExportRoundTripsRocketWithZeroLoss) {
    auto rig = *HitmPartsRig::Import(FixtureDir("rocket")).value;
    std::string sourceText = [] {
        std::ifstream in(FixtureDir("rocket") / "parts.json", std::ios::binary);
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }();
    Value sourceParsed = Value::Parse(sourceText);
    DOMINUS_EXPECT(sourceParsed.Dump() == rig.ToJson().Dump());
}

// --- Deliberate-break: every real-world corruption mode must fail loud ---

DOMINUS_TEST(HitmPartsRig_Break_MissingFile_Fails) {
    auto result = HitmPartsRig::Import(FixtureDir("broken_missing_file"));
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("parts.json") != std::string::npos);
}

DOMINUS_TEST(HitmPartsRig_Break_MalformedJson_Fails) {
    auto result = HitmPartsRig::Import(FixtureDir("broken_malformed_json"));
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("parse error") != std::string::npos);
}

DOMINUS_TEST(HitmPartsRig_Break_AtlasMismatch_Fails) {
    auto result = HitmPartsRig::Import(FixtureDir("broken_atlas_mismatch"));
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("brooklyn_atlas") != std::string::npos);
}

DOMINUS_TEST(HitmPartsRig_Break_MissingHandBoneField_Fails) {
    auto result = HitmPartsRig::Import(FixtureDir("broken_missing_handbone"));
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("handBone") != std::string::npos);
}

DOMINUS_TEST(HitmPartsRig_Break_DanglingHandBoneReference_Fails) {
    auto result = HitmPartsRig::Import(FixtureDir("broken_dangling_handbone"));
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("handThatDoesNotExist") != std::string::npos);
}

DOMINUS_TEST(HitmPartsRig_Break_DrawOrderUnknownPart_Fails) {
    auto result = HitmPartsRig::Import(FixtureDir("broken_draworder_unknown_part"));
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("nonexistentPart") != std::string::npos);
}

DOMINUS_TEST(HitmPartsRig_Break_DanglingBoneParent_Fails) {
    auto result = HitmPartsRig::Import(FixtureDir("broken_bone_dangling_parent"));
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("nonexistentParent") != std::string::npos);
}

DOMINUS_TEST(HitmPartsRig_Break_PartMissingFrame_Fails) {
    auto result = HitmPartsRig::Import(FixtureDir("broken_part_missing_frame"));
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(result.error.find("frame") != std::string::npos);
}

DOMINUS_TEST(HitmPartsRig_Break_FailedImportDoesNotThrow) {
    bool threw = false;
    bool ok = true;
    try {
        auto result = HitmPartsRig::Import(FixtureDir("broken_missing_handbone"));
        ok = result.ok;
    } catch (...) {
        threw = true;
    }
    DOMINUS_EXPECT(!threw);
    DOMINUS_EXPECT(!ok);
}
