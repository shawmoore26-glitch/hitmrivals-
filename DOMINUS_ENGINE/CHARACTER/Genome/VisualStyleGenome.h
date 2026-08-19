// CHARACTER/Genome/VisualStyleGenome.h
// "Style Description Language" -- distinct from CombatStyleGenome
// (fighting style: aggression/defense/mobility) and distinct from
// VisualGenome (one entity's own visual traits). This describes an
// ART STYLE as a comparable, referenceable object in its own right --
// "Neo Mythic," "Urban Combat" -- so multiple entities/materials could
// point at the same style.
//
// `influences` is DATA ONLY -- a list of parent style names/ids, same
// shape as CombatStyleGenome's `ancestry`. This is deliberately NOT an
// auto-combine algorithm: the source document's own explicit
// correction ruled that "AI creates styles" requires future
// intelligence systems, and "Style Fusion" (blending two styles'
// visual_rules by percentage into a genuinely new set of rules) has no
// objective criteria to validate against -- the same epistemic problem
// GameDesignCoherenceChecker was built to respect rather than paper
// over. `influences` lets a style DECLARE its lineage; nothing computes
// a merged style from it.
#pragma once

#include <string>
#include <vector>

namespace dominus::character {

struct VisualStyleRules {
    std::string line_quality;     // free-form: "heavy", "delicate", "sketchy", ...
    std::string color_behavior;   // free-form: "high_contrast", "muted", "monochrome", ...
    std::string shape_behavior;   // free-form: "exaggerated", "realistic", "geometric", ...
    std::string motion_behavior;  // free-form: "dramatic", "subtle", "frenetic", ...
};

struct VisualStyleGenome {
    std::string style_id;    // required -- e.g. "STYLE-001"
    std::string name;        // required -- e.g. "Neo Mythic"
    VisualStyleRules visual_rules;
    std::vector<std::string> influences;  // parent style names/ids -- data only, no combine algorithm
};

}  // namespace dominus::character
