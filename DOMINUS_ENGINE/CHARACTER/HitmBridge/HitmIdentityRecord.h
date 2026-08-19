// CHARACTER/HitmBridge/HitmIdentityRecord.h
// ROADMAP.md Track H Module 1. Losslessly holds one real HITM Rivals
// fighter's authored identity data as ingested from the real
// hitm-engine/data/identity/<fighter>/*.json source files.
//
// Deliberately NOT the same thing as CHARACTER/Genome/CombatIdentity: that
// type is a 6-field (style/range/pressure/counter/mobility/risk) strawman
// -- see HITM_INTEGRATION_AUDIT.md section 2 for the evidence that it
// cannot represent Brooklyn's real 13-component combat genome or read-
// engine mechanic. This record exists to preserve the real data intact so
// a future, real genome mapping (Track H Module 2) has something faithful
// to map from, instead of re-deriving a strawman from a strawman.
//
// Every field present in the authored source survives as a real
// dominus::core::json::Value subtree here -- nothing is flattened,
// summarized, renamed, or dropped. A field this engine has no consumer
// for yet (motion_bible.json, hit_feel_profile.json) is simply not read
// by this module at all, rather than read and left inert -- see
// HitmIdentityImporter.h for which five files this module actually
// touches and why.
#pragma once

#include <string>

#include "CORE/Serialization/MiniJson.h"

namespace dominus::character::hitm {

struct HitmIdentityRecord {
    std::string fighter_id;                  // e.g. "brooklyn" -- from the directory name, cross-checked against identity.json's "id"
    core::json::Value character_dna;          // character_dna.json  (numeric + categorical DNA scalars)
    core::json::Value combat_genome;          // combat_genome.json  (the real, rich combat identity -- see file header)
    core::json::Value design;                 // design.json         (physical parts, draw order, hand anchors)
    core::json::Value identity;               // identity.json       (display name, faction, sprite/atlas refs, palettes)
    core::json::Value signature;              // signature.json      (authored moves, chains, projectiles)
};

}  // namespace dominus::character::hitm
