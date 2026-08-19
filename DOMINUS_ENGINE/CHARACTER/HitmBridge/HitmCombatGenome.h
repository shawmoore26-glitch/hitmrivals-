// CHARACTER/HitmBridge/HitmCombatGenome.h
// ROADMAP.md Track H Module 2 -- real genome mapping.
//
// This is the explicit, semantic, typed representation of one real HITM
// Rivals fighter's authored combat genome (their "Volume 14 Combat
// Genome", per the source files' own "_note" field). It is built on top
// of Module 1's HitmIdentityImporter output (HitmIdentityRecord), never
// read straight from disk -- so every instance of this class already
// carries Module 1's fail-closed guarantees (file presence, JSON
// validity, required-key presence, directory/fighter id match).
//
// It is deliberately NOT a replacement for CHARACTER/Genome/CombatIdentity
// (the pre-existing 6-field style/range/pressure/counter/mobility/risk
// type). That type is untouched by this module -- see
// HITM_INTEGRATION_AUDIT.md section 2 and ROADMAP.md's Track H intro for
// why replacing or flattening the real genome into it would just move
// the same information loss one layer over instead of closing it.
//
// LOSSLESSNESS, BY CONSTRUCTION: `raw_` holds the exact
// dominus::core::json::Value this object was built from, completely
// unmodified. ToJson() returns that same tree. Every typed accessor below
// (Archetype(), Rhythm(), GetReadEngine(), ...) is a read-only EXTRACTIVE
// view over `raw_` -- none of them are the source of truth, and none of
// them are ever used to reconstruct `raw_`. That asymmetry is what makes
// "import then export loses nothing" true by the shape of the code, not
// just by a test asserting it (the test in
// tests/character/test_hitm_combat_genome.cpp exists anyway, because a
// design intention is not evidence).
//
// DERIVED vs AUTHORED: every typed field below is std::optional (or an
// empty vector when absent) unless the real data for that fighter
// actually has it. There is no default-filling anywhere in this file --
// a missing optional means "this fighter's real genome does not define
// this component," never "the importer didn't get to it yet." Concretely:
// Rocket and Static have no read_engine (Brooklyn's signature mechanic);
// GetReadEngine() returns nullptr for them, not an empty-but-present one.
//
// WHAT THIS MODULE DOES NOT DO: it does not wire these fields into any
// consumer (GenomeDecoder, ReactionSystem, CombatAI, ...) -- there is no
// gameplay effect from any of this yet. It does not touch
// CombatStyleGenome, rendering, audio, input, or any system outside
// CHARACTER/HitmBridge. Making the real genome representable and making
// it drive gameplay are two different, separately-gated pieces of work.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "CHARACTER/HitmBridge/HitmIdentityRecord.h"
#include "CORE/Serialization/DominusSerializer.h"  // for core::Result<T>
#include "CORE/Serialization/MiniJson.h"

namespace dominus::character::hitm {

// The 8 short descriptive sub-profiles common to all three real fighters'
// combat_genome.json (evidenced key-for-key across brooklyn/rocket/static
// -- see HitmIdentityImporter.cpp's RequiredKeys comment for the same
// evidence method applied one level up). Every field here is optional
// because even within a "common" profile, fighters differ on which of its
// inner fields they use (e.g. only Brooklyn's defense_profile has "_law"
// and "from"/"not" -- Rocket's and Static's do not).
struct RhythmProfile {
    std::optional<std::string> categorical;
    std::optional<std::string> beat;
    std::optional<std::string> tempo;
    std::optional<std::string> signature;
};

struct WeightProfile {
    std::optional<std::string> categorical;
    std::optional<double> inertia;
    std::optional<std::string> settle;
};

struct RiskProfile {
    std::optional<std::string> commitment;
    std::optional<std::string> reward;
    std::optional<std::string> recovery_exposure;
};

struct DefenseProfile {
    std::optional<std::string> style;
    std::optional<double> block_preference;  // real numeric field, e.g. Brooklyn 0.18, capped at 0.25 by his "_law"
    std::optional<std::string> escape;
    std::optional<std::string> law;  // "_law" -- Brooklyn only
    std::optional<std::vector<std::string>> from;  // Brooklyn only
    std::optional<std::vector<std::string>> not_allowed;  // JSON key is "not" -- renamed, "not" is a C++ keyword-adjacent trap
};

struct PressureProfile {
    std::optional<std::string> style;
    std::optional<std::string> turn_retention;
    std::optional<std::string> corner_carry;
};

struct RangeProfile {
    std::optional<std::string> band;
    std::optional<double> dominant;
    std::optional<std::string> deadzone;
};

struct RecoveryProfile {
    std::optional<std::string> categorical;
    std::optional<double> settle_beats;
};

struct ImpactProfile {
    std::optional<std::string> hitstop;
    std::optional<std::string> camera_shake;
    std::optional<std::string> sound_intent;
};

// Brooklyn's signature mechanic, explicit and typed -- not just present
// somewhere inside a JSON blob. See CHARACTER/HitmBridge/HitmCombatGenome.h
// top comment: Rocket and Static genuinely do not have one.
struct ReadEngineTier {
    double reads = 0;
    std::string name;
    double damage_mult = 0;
    std::optional<std::string> note;
};

struct ReadEngineDecay {
    double frames = 0;
    double amount = 0;
    std::optional<std::string> note;
};

struct ReadEngine {
    double max_reads = 0;
    std::vector<std::string> gain_on;
    std::vector<std::string> lose_on;
    std::optional<std::string> lose_law;  // "_lose_law"
    std::vector<ReadEngineTier> tiers;
    ReadEngineDecay decay;
};

class HitmCombatGenome {
public:
    // Builds from an already-imported, already-validated
    // HitmIdentityRecord (Module 1) -- never reads a file directly.
    // Fails (Result::Fail) on any of: a required field present with the
    // wrong JSON type, a malformed read_engine (present but missing
    // max_reads/tiers/decay, or a tier missing reads/name/damage_mult, or
    // tiers not an array of objects). Required-key *presence* was already
    // enforced by Module 1; this validates *shape*, one level deeper.
    static core::Result<HitmCombatGenome> FromRecord(const HitmIdentityRecord& record);

    const std::string& FighterId() const { return fighter_id_; }
    const std::string& Archetype() const { return archetype_; }
    const std::string& Philosophy() const { return philosophy_; }
    const std::string& FrameDna() const { return frame_dna_; }
    const std::vector<std::string>& AiIntent() const { return ai_intent_; }
    const std::vector<std::string>& CombatVerbs() const { return combat_verbs_; }

    const RhythmProfile& Rhythm() const { return rhythm_; }
    const WeightProfile& Weight() const { return weight_; }
    const RiskProfile& Risk() const { return risk_; }
    const DefenseProfile& Defense() const { return defense_; }
    const PressureProfile& Pressure() const { return pressure_; }
    const RangeProfile& Range() const { return range_; }
    const RecoveryProfile& Recovery() const { return recovery_; }
    const ImpactProfile& Impact() const { return impact_; }

    bool HasReadEngine() const { return read_engine_.has_value(); }
    const ReadEngine* GetReadEngine() const { return read_engine_ ? &*read_engine_ : nullptr; }

    // The exact source tree -- see this file's top comment for why this,
    // not the typed fields above, is the actual losslessness guarantee.
    const core::json::Value& ToJson() const { return raw_; }

private:
    std::string fighter_id_;
    core::json::Value raw_;
    std::string archetype_;
    std::string philosophy_;
    std::string frame_dna_;
    std::vector<std::string> ai_intent_;
    std::vector<std::string> combat_verbs_;
    RhythmProfile rhythm_;
    WeightProfile weight_;
    RiskProfile risk_;
    DefenseProfile defense_;
    PressureProfile pressure_;
    RangeProfile range_;
    RecoveryProfile recovery_;
    ImpactProfile impact_;
    std::optional<ReadEngine> read_engine_;
};

}  // namespace dominus::character::hitm
