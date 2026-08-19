// VISUALFORGE/VisualForgeCanonicalSerializer.h
// Same job REGISTRY::CanonicalSerializer and COMBAT::
// ImpactCanonicalSerializer already do -- fixed-field-order,
// deterministic strings so identical blueprint/spec values always
// produce identical bytes. Lives here rather than in REGISTRY for the
// same reason COMBAT::ImpactCanonicalSerializer does: REGISTRY's own
// law is "depends on CHARACTER/Genome only," and CharacterBlueprint/
// AssetSpecification/AnimationSpecification are VISUALFORGE types.
#pragma once

#include <sstream>
#include <string>

#include "VISUALFORGE/AnimationSpecification.h"
#include "VISUALFORGE/AssetSpecification.h"
#include "VISUALFORGE/CharacterBlueprint.h"

namespace dominus::visualforge {

class VisualForgeCanonicalSerializer {
public:
    static std::string SerializeCharacterBlueprint(const CharacterBlueprint& b) {
        std::ostringstream out;
        out << "entity_id=" << b.entity_id;
        out << ";body_type=" << b.form.body_type << ";silhouette=" << b.form.silhouette
            << ";proportion=" << b.form.proportion << ";shape_language=" << b.form.shape_language;
        out << ";skin_roughness=" << b.skin.roughness << ";skin_subsurface=" << b.skin.subsurface
            << ";skin_age=" << b.skin.age_years
            << ";skin_damage_response=" << (b.skin.damage_response ? "true" : "false");
        out << ";clothing_material=" << b.clothing.material
            << ";clothing_adaptive_damage=" << (b.clothing.adaptive_damage ? "true" : "false")
            << ";clothing_weather_response=" << (b.clothing.weather_response ? "true" : "false");
        out << ";aura=" << b.presence.aura << ";threat_signature=" << b.presence.threat_signature
            << ";emotional_visual_weight=" << b.presence.emotional_visual_weight
            << ";presence_style_id=" << b.presence.style_id;
        out << ";has_material=" << (b.has_material ? "true" : "false") << ";material_id=" << b.material_id
            << ";material_type=" << b.material_identity.type
            << ";material_age_years=" << b.material_properties.age_years
            << ";material_wear_state=" << b.material_properties.wear_state
            << ";material_damage_history=" << (b.material_properties.damage_history ? "true" : "false")
            << ";material_weather_exposure=" << (b.material_properties.weather_exposure ? "true" : "false");
        out << ";has_visual_style=" << (b.has_visual_style ? "true" : "false") << ";style_id=" << b.style_id
            << ";style_name=" << b.style_name << ";visual_rules_line=" << b.visual_rules.line_quality
            << ";visual_rules_color=" << b.visual_rules.color_behavior
            << ";visual_rules_shape=" << b.visual_rules.shape_behavior
            << ";visual_rules_motion=" << b.visual_rules.motion_behavior << ";influences=" << JoinCsv(b.style_influences);
        out << ";has_memory=" << (b.has_memory ? "true" : "false") << ";memory_event_count=" << b.memory.event_count
            << ";memory_has_history=" << (b.memory.has_history ? "true" : "false");
        out << ";style_reference_matches=" << (b.style_reference_matches ? "true" : "false");
        return out.str();
    }

    static std::string SerializeAssetSpecification(const AssetSpecification& s) {
        std::ostringstream out;
        out << "entity_id=" << s.entity_id;
        for (const auto& r : s.requirements) {
            out << ";[category=" << r.category << ";name=" << r.name << ";source_field=" << r.source_field
                << ";notes=" << r.notes << "]";
        }
        return out.str();
    }

    static std::string SerializeAnimationSpecification(const AnimationSpecification& s) {
        std::ostringstream out;
        out << "entity_id=" << s.entity_id << ";entry_state=" << s.entry_state;
        for (const auto& st : s.states) {
            out << ";[state=" << st.state_name << ";clip=" << st.clip_name
                << ";clip_found=" << (st.clip_found ? "true" : "false") << ";duration=" << st.duration
                << ";loop=" << (st.loop ? "true" : "false") << "]";
        }
        for (const auto& t : s.transitions) {
            out << ";[from=" << t.from_state << ";to=" << t.to_state << ";trigger=" << t.trigger
                << ";blend=" << t.blend_duration << ";auto=" << (t.auto_on_complete ? "true" : "false") << "]";
        }
        return out.str();
    }

private:
    static std::string JoinCsv(const std::vector<std::string>& items) {
        std::ostringstream out;
        for (size_t i = 0; i < items.size(); ++i) {
            out << items[i];
            if (i + 1 < items.size()) out << "|";
        }
        return out.str();
    }
};

}  // namespace dominus::visualforge
