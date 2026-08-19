// CHARACTER/Genome/CombatIdentity.h
// LAW C002: combat identity lives in data, never hardcoded. LAW C003: two
// fighters using the same moves must not fight identically -- style,
// range, pressure, counter, mobility, and risk are what COMBAT systems
// read to vary behavior per-genome instead of per-hardcoded-character.
#pragma once

#include <string>

namespace dominus::character {

struct CombatIdentity {
    std::string style;      // e.g. "psycho_drunken_martial_arts"
    std::string range;      // "close" | "mid" | "far"
    std::string pressure;   // "relentless" | "cautious" | "reactive" | ...
    std::string counter;    // "expert" | "average" | "poor"
    std::string mobility;   // "unpredictable" | "grounded" | "aerial" | ...
    std::string risk;       // "low" | "medium" | "high"
};

}  // namespace dominus::character
