// CHARACTER/Genome/VisualGenome.h
// Real, scoped to what's actually storable as data: form/surface/
// presence descriptors. Deliberately does NOT include anything that
// requires an actual renderer to mean something -- no shaders, no
// lighting rules, no GPU pipeline, no "style fusion" (combining art
// styles has no objective criteria to validate against, same epistemic
// problem GameDesignCoherenceChecker was built to respect rather than
// fabricate an answer to). GRAPHICS remains an empty placeholder; this
// schema describes what SHOULD eventually be rendered, same as
// CombatStyleGenome describes a style before any decoder consumes it.
//
// "Memory" (Section XI's "objects remember: battles, kills, damage%")
// is deliberately NOT a field here with invented numbers. See
// VisualMemorySummary in this same file -- it's derived from the REAL
// WORLD::WorldHistory event log (Society Phase 0), not a second,
// parallel, fabricated history system.
#pragma once

#include <string>
#include <vector>

namespace dominus::character {

struct VisualForm {
    std::string body_type;        // free-form: "humanoid", "quadruped", ...
    std::string silhouette;       // free-form: "heavy_fighter", "lithe", ...
    std::string proportion;       // free-form: "street_brawler", "elongated", ...
    std::string shape_language;   // free-form: "aggressive", "soft", "angular", ...
};

struct VisualSkin {
    float roughness = 0.5f;        // 0-1
    float subsurface = 0.2f;       // 0-1
    int age_years = 25;
    bool damage_response = true;   // does the skin visually react to damage
};

struct VisualClothing {
    std::string material;          // free-form: "urban_leather", "cloth", ...
    bool adaptive_damage = true;   // does clothing visually wear over time
    bool weather_response = true;
};

struct VisualPresence {
    std::string aura;              // free-form: "chaotic", "calm", "menacing", ...
    float threat_signature = 0.5f;         // 0-1
    float emotional_visual_weight = 0.5f;  // 0-1
    // References a VisualStyleGenome's style_id, same relationship as
    // CombatIdentity.style -> CombatStyleGenome.style_name. Optional --
    // empty means no style assigned yet. Added when VisualStyleGenome
    // was built specifically so the two could be cross-checked for
    // real, not just conceptually related.
    std::string style_id;
};

struct VisualGenome {
    VisualForm form;
    VisualSkin skin;
    VisualClothing clothing;
    VisualPresence presence;
};

}  // namespace dominus::character
