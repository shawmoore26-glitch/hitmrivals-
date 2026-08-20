// CHARACTER/HitmBridge/HitmRigPlacement.h
// ROADMAP.md Track H Module 5B -- real HITM sprite/texture integration,
// Phase 1 (CPU-observable). Gives HITM Rivals' real, generated `rig.json`
// a home in DOMINUS.
//
// PROVENANCE: `rig.json` lives under hitm-engine's
// `data/characters/<fighter>/`, sibling to `parts.json`/`anim.json`.
// Its own `_generated` field says it verbatim: "rig_compiler.py --
// derived from character_dna + design + combat_genome" -- generated
// output, same "legitimate to read, never to hand-edit" status as
// `parts.json` (see HitmPartsRig.h's top comment).
//
// WHY THIS IS A SEPARATE FILE FROM HitmPartsRig, NOT AN EXTENSION OF IT:
// Module 3's `HitmPartsRig` already closed over `parts.json` alone and is
// part of a formally-closed module. `rig.json` is a genuinely different
// real file this track has never imported before now -- it carries a
// *placement* rect per part (`[x0,y0,x1,y1]`, normalized to the full
// character silhouette) that `parts.json` does not have, is not
// referenced anywhere in `identity.json`'s `sprite` block (only `rig`
// (parts.json) and `anim` are), and is not consumed by hitm-engine's own
// `SkeletonSystem.js` at all -- but IS the exact placement data
// hitm-engine's own real, working offline verification tool
// (`tools/rig_render.py`, "reproduces SkeletonSystem's transform chain
// ... proves parts assemble into a figure") actually uses to position
// every part. See HitmSpriteDrawData.h's top comment for the full
// reasoning on why this module follows `rig_render.py`'s real, working
// convention rather than `SkeletonSystem.js`'s incomplete one.
//
// REAL, EVIDENCED CROSS-CHECK (verified against all three real fighters):
// every part named in `rig.json`'s `parts` map also appears in the same
// fighter's `parts.json` `parts` map, under the exact same key, with the
// exact same `pivot` value. `Import()` enforces both -- a real fighter
// whose two independently-generated files disagree about a part's pivot,
// or name a part the other file doesn't have, is corrupted data, not a
// looser variant of the schema.
#pragma once

#include <filesystem>
#include <map>
#include <string>

#include "CORE/Serialization/DominusSerializer.h"  // for core::Result<T>
#include "CORE/Serialization/MiniJson.h"
#include "CHARACTER/HitmBridge/HitmPartsRig.h"

namespace dominus::character::hitm {

// One real `rig.json` `parts.<name>` entry.
struct HitmPartPlacement {
    double rect_x0 = 0;
    double rect_y0 = 0;
    double rect_x1 = 0;
    double rect_y1 = 0;
    double pivot_x = 0;
    double pivot_y = 0;
};

class HitmRigPlacement {
public:
    // Reads <characterDir>/rig.json. Fails on a missing file, malformed
    // JSON, a missing/wrong-typed required field, or -- given a non-null
    // `crossCheck` -- a part named here that `crossCheck` (the same
    // fighter's already-imported HitmPartsRig) does not also have, or
    // whose `pivot` disagrees between the two real files. `crossCheck`
    // is optional so this class can also be exercised standalone (e.g.
    // to prove a corrupt `rig.json` fails on its own malformed-JSON
    // grounds before the cross-file check would even run).
    static core::Result<HitmRigPlacement> Import(const std::filesystem::path& characterDir,
                                                   const HitmPartsRig* crossCheck = nullptr);

    const std::string& FighterId() const { return fighter_id_; }
    bool HasPart(const std::string& name) const { return parts_.count(name) > 0; }
    const HitmPartPlacement* Part(const std::string& name) const;

private:
    std::string fighter_id_;
    std::map<std::string, HitmPartPlacement> parts_;
};

}  // namespace dominus::character::hitm
