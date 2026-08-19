// CHARACTER/Genome/GameDesignCoherenceChecker.h
// The real, scoped version of "one genre value cascades constraints
// into every forge": checks whether a CombatStyleGenome +
// CombatPhysicsGenome pairing is coherent with a declared genre.
//
// Deliberately ADVISORY, never a hard pass/fail: unlike
// CreatureGenomeSemanticValidator's checks (wing area vs. weight is
// real physics, growth stages reaching maturity is real arithmetic),
// "is this combat style soulslike enough" has no ground truth to
// validate against -- there's no objective definition, no corpus of
// games to derive thresholds from. Presenting these as errors would be
// asserting authority this engine doesn't have. What CAN be honest:
// these are the exact traits the source document itself stated for
// each genre, translated into concrete checks against fields that
// already exist. Coherence notes are suggestions to a human, not
// verdicts.
//
// Only two genres are covered -- "soulslike" and "arcade" -- because
// those are the only two the source document gave concrete criteria
// for. Every other genre (roguelike, metroidvania, ...) would mean
// inventing criteria with no stated source, which is fabrication dressed
// as analysis. Unrecognized genres produce zero notes, not guessed ones.
#pragma once

#include <string>
#include <vector>

#include "CHARACTER/Genome/CombatPhysicsGenome.h"
#include "CHARACTER/Genome/CombatStyleGenome.h"
#include "CHARACTER/Genome/GameDesignGenome.h"

namespace dominus::character {

struct CoherenceNote {
    std::string field;    // which genome field the note is about
    std::string message;  // human-readable suggestion
};

class GameDesignCoherenceChecker {
public:
    static std::vector<CoherenceNote> Evaluate(const GameDesignGenome& design, const CombatStyleGenome& style,
                                                 const CombatPhysicsGenome& physics) {
        std::vector<CoherenceNote> notes;

        if (design.genre == "soulslike") {
            EvaluateSoulslike(style, physics, notes);
        } else if (design.genre == "arcade") {
            EvaluateArcade(style, physics, notes);
        }
        // Unrecognized genre: intentionally zero notes, not a guess.

        return notes;
    }

private:
    // Source document's own stated soulslike traits: "punish mistakes,
    // telegraph attacks, encourage stamina management," "commitment-
    // based attacks." Translated into the two fields that actually speak
    // to those traits.
    static void EvaluateSoulslike(const CombatStyleGenome& style, const CombatPhysicsGenome& physics,
                                    std::vector<CoherenceNote>& notes) {
        if (physics.energy.fatigue_rate < 1.0f) {
            notes.push_back({"physics.energy.fatigue_rate",
                              "soulslike design typically wants meaningful stamina management -- current "
                              "fatigue_rate (" +
                                  std::to_string(physics.energy.fatigue_rate) +
                                  ") is below 1.0, may not punish over-extension enough"});
        }
        if (style.precision < 0.5f && style.mobility > 0.8f) {
            notes.push_back({"style.precision / style.mobility",
                              "high mobility (" + std::to_string(style.mobility) + ") with low precision (" +
                                  std::to_string(style.precision) +
                                  ") reads as uncommitted spam, not the deliberate/telegraphed engagement "
                                  "soulslike combat is known for"});
        }
    }

    // Implied opposite in the source document: fast-paced, forgiving,
    // not stamina-punishing.
    static void EvaluateArcade(const CombatStyleGenome& style, const CombatPhysicsGenome& physics,
                                 std::vector<CoherenceNote>& notes) {
        if (physics.energy.fatigue_rate > 1.0f) {
            notes.push_back({"physics.energy.fatigue_rate",
                              "arcade design typically wants low stamina friction -- current fatigue_rate (" +
                                  std::to_string(physics.energy.fatigue_rate) +
                                  ") is above 1.0, may feel punishing for the intended pace"});
        }
        if (style.mobility < 0.5f) {
            notes.push_back({"style.mobility",
                              "arcade combat is typically fast-paced -- current mobility (" +
                                  std::to_string(style.mobility) + ") is below 0.5"});
        }
    }
};

}  // namespace dominus::character
