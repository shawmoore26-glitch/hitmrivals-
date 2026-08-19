// CHARACTER/Genome/CombatStyleGenome.h
// A fighter's CombatIdentity.style (see CombatIdentity.h) has always
// been a free-form string -- "psycho_drunken_martial_arts" means
// whatever GenomeDecoder's string-matching says it means, with no real
// definition behind the name itself. CombatStyleGenome gives that
// string a real, structured referent: STYLES are entities in their own
// right, decoupled from any one fighter, so multiple fighters could
// reference the same style, and a style's own lineage (`ancestry`) can
// be tracked independently of any fighter's evolution.
//
// Deliberately does NOT replace CombatIdentity -- CombatIdentity stays
// exactly what it is (LAW C002/C003), deeply embedded across
// COMBAT/AI/tests since Phase 3. This is a separate, additive concept
// sitting alongside it, matching the same "one responsibility per
// genome type" discipline as CreatureGenome/SocialGenome.
//
// Same "no fake numbers" discipline as every other genome in this
// engine: every weight here is a real value someone (a human author, or
// eventually a real decoder) can reason about -- never invented to look
// like output from a process that didn't run.
#pragma once

#include <string>
#include <vector>

namespace dominus::character {

struct CombatStyleGenome {
    std::string style_name;                // required -- e.g. "psycho_drunken_martial_arts"
    std::vector<std::string> ancestry;      // style names this one is derived from, e.g. ["drunken_boxing", "capoeira"]

    float aggression = 0.5f;      // 0-1
    float defense = 0.5f;         // 0-1
    float mobility = 0.5f;        // 0-1
    float pressure = 0.5f;        // 0-1
    float deception = 0.5f;       // 0-1
    float endurance = 0.5f;       // 0-1
    float precision = 0.5f;       // 0-1
    float adaptability = 0.5f;    // 0-1
    float evolution_capacity = 0.5f;  // 0-1 -- a genome trait describing how much room the style has to
                                       // change under the Evolution Protocol (see CombatStyleVersioning
                                       // note in the loader) -- not a claim that evolution is simulated.

    std::string range_control;  // free-form: "close", "mid", "far", "adaptive", ...
    std::string rhythm;         // free-form, e.g. "erratic, off-beat, feints disguised as stumbles"
    std::string philosophy;     // free-form, the style's own stated reasoning for why it fights this way

    std::vector<std::string> weaknesses;  // free-form, authored, not derived
};

}  // namespace dominus::character
