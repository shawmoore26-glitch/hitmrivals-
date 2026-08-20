// CHARACTER/HitmBridge/HitmAssetImporter.h
// ROADMAP.md Track H Module 5B -- real HITM sprite/texture integration,
// Phase 1 (CPU-observable).
//
// This is the "REAL HITM ASSETS -> DOMINUS ASSET REPRESENTATION" stage of
// the pipeline this module builds: given a fighter's already-imported
// `HitmIdentityRecord` (Module 1) and the real hitm-engine source tree's
// root directory, it (1) discovers the real atlas PNG / parts.json /
// anim.json / rig.json this fighter's own real `identity.json` points at,
// (2) validates the atlas image is a real, well-formed PNG whose declared
// dimensions actually contain every part frame `HitmPartsRig` claims, and
// (3) assembles the three already-defined real-data importers
// (HitmPartsRig, HitmAnimationSet, HitmRigPlacement) into one bundle,
// cross-checking every place two independently-generated real files
// claim to describe the same thing (parts.json's own `atlas` field vs.
// identity.json's `sprite.atlas`; rig.json's parts vs. parts.json's
// parts) and failing loud on any disagreement.
//
// WHAT "VALIDATE THE ATLAS" MEANS HERE, AND WHY NOT MORE: this reads the
// PNG file's own header (the 8-byte signature + the mandatory first IHDR
// chunk, which every valid PNG has by the PNG standard, unconditionally)
// to obtain its real, authoritative pixel width/height -- no full pixel
// decode is attempted, because nothing after this Phase needs pixel data
// yet (this vertical slice stops at deterministic *draw data* -- which
// atlas region, at what pose -- not at actually sampling pixels, which is
// the GPU/renderer's job and explicitly out of this Phase's scope). A
// real, evidenced check WAS found and is enforced: every part's real
// `frame` rect in `parts.json` must fit entirely within the atlas
// image's real pixel bounds (verified 3/3 real fighters; Rocket's real
// atlas is packed edge-to-edge, `maxX == atlas width` exactly). The
// naive-sounding "does the atlas's pixel size equal parts.json's
// `sourceSize`" is deliberately NOT checked -- verified false for all
// three real fighters (`sourceSize` describes the single un-atlased
// source sprite's canvas; the atlas is a separately texture-packed sheet
// of cutout regions at an unrelated resolution) -- asserting that
// non-relationship would have been a DOMINUS-invented rule, not a real
// HITM invariant, exactly the kind of fabricated validation this track
// refuses to add.
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>

#include "CORE/Serialization/DominusSerializer.h"  // for core::Result<T>
#include "CHARACTER/HitmBridge/HitmAnimationSet.h"
#include "CHARACTER/HitmBridge/HitmIdentityRecord.h"
#include "CHARACTER/HitmBridge/HitmPartsRig.h"
#include "CHARACTER/HitmBridge/HitmRigPlacement.h"

namespace dominus::character::hitm {

// Everything Phase 1 needs for one fighter's visual content, all real,
// all cross-validated against each other.
struct HitmAssetBundle {
    std::string fighter_id;
    std::filesystem::path atlas_png_path;
    int32_t atlas_pixel_width = 0;
    int32_t atlas_pixel_height = 0;
    HitmPartsRig parts;
    HitmAnimationSet animations;
    HitmRigPlacement placement;
};

class HitmAssetImporter {
public:
    // `hitmEngineRoot` is the real hitm-engine source tree's root -- the
    // directory `record.identity`'s `sprite.rig`/`sprite.anim` relative
    // paths, and the real `assets/parts/<atlas>.png` convention (evidenced
    // against `tools/slice_rig.py`'s and `tools/build.py`'s own real
    // output paths, 3/3 real fighters), are resolved against.
    //
    // Fails (Result::Fail) on: a missing `sprite` block, a missing
    // `sprite.atlas`/`sprite.rig`/`sprite.anim` field; a `sprite.rig`/
    // `sprite.anim` path that does not resolve to the real, evidenced
    // `data/characters/<fighter_id>/{parts,anim}.json` convention; any
    // failure from `HitmPartsRig::Import`, `HitmAnimationSet::Import`, or
    // `HitmRigPlacement::Import`; a missing atlas PNG file; a file that
    // does not begin with the real PNG signature; a part whose real
    // `frame` rect does not fit within the atlas's real declared pixel
    // bounds; or `parts.json`'s own `atlas` field disagreeing with
    // `identity.json`'s `sprite.atlas` for the same fighter.
    static core::Result<HitmAssetBundle> Import(const HitmIdentityRecord& record,
                                                  const std::filesystem::path& hitmEngineRoot);

    // Reads just the PNG signature + first IHDR chunk -- see this file's
    // header comment for why that is enough for Phase 1. Exposed
    // separately (not just inlined into Import) so this real,
    // independently-meaningful check has its own direct test coverage.
    static core::Result<std::pair<int32_t, int32_t>> ReadPngDimensions(const std::filesystem::path& pngPath);
};

}  // namespace dominus::character::hitm
