// CHARACTER/HitmBridge/HitmIdentityImporter.h
// ROADMAP.md Track H Module 1.
#pragma once

#include <filesystem>

#include "CHARACTER/HitmBridge/HitmIdentityRecord.h"
#include "CORE/Serialization/DominusSerializer.h"  // for core::Result<T>

namespace dominus::character::hitm {

// Reads a real hitm-engine fighter identity directory --
// <identityDir>/{character_dna,combat_genome,design,identity,signature}.json
// -- and returns a validated, lossless HitmIdentityRecord.
//
// Fails loud (Result::Fail, never a partial record) on:
//  - a missing directory or missing file
//  - malformed JSON in any of the five files
//  - a missing required top-level key in any of the five files (see
//    RequiredKeys() in the .cpp -- the exact sets are evidenced against
//    all three real HITM Rivals fighters: brooklyn, rocket, static)
//  - identity.json's "id" field not matching the directory name (catches
//    a copy/rename mistake rather than silently importing under the
//    wrong fighter id)
//
// Does NOT read motion_bible.json or hit_feel_profile.json, even though
// they exist in the real fixture directories -- this module has no
// consumer for them yet, and reading a field with nothing downstream to
// use it is exactly the "loaded but inert" pattern
// HITM_INTEGRATION_AUDIT.md flags against COMBAT/Profiles.h's unconsumed
// Audio/Visual/Camera profiles. Extending this importer to those files is
// real future work, gated on a real consumer existing first.
class HitmIdentityImporter {
public:
    static core::Result<HitmIdentityRecord> Import(const std::filesystem::path& identityDir);
};

}  // namespace dominus::character::hitm
