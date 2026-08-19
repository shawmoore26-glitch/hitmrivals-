// VISUALFORGE/AssetSpecification.h
// A mechanical checklist, not a creative brief: every entry here is
// derived directly from a real CharacterBlueprint field that already
// has a value. This does NOT decide what the assets should look like
// (that's a real artist's or a future generator's job, same boundary
// VisualGenome's own header comment already draws) -- it only states
// WHICH assets a complete production of this character would need,
// and WHY (which blueprint field demanded it).
#pragma once

#include <string>
#include <vector>

#include "VISUALFORGE/CharacterBlueprint.h"

namespace dominus::visualforge {

struct AssetRequirement {
    std::string category;     // "mesh" | "texture" | "material" | "reference"
    std::string name;          // a concrete, derived asset name
    std::string source_field;  // which CharacterBlueprint field demanded this entry
    std::string notes;         // real values pulled from the blueprint, not invented description
};

struct AssetSpecification {
    std::string entity_id;
    std::vector<AssetRequirement> requirements;
};

class AssetSpecificationForge {
public:
    static AssetSpecification Build(const CharacterBlueprint& blueprint) {
        AssetSpecification spec;
        spec.entity_id = blueprint.entity_id;

        // Body mesh -- always required, driven by form.
        spec.requirements.push_back({"mesh", blueprint.entity_id + "_body_mesh", "form",
                                      "body_type=" + blueprint.form.body_type +
                                          " silhouette=" + blueprint.form.silhouette +
                                          " proportion=" + blueprint.form.proportion});

        // Skin texture -- always required, driven by skin.
        spec.requirements.push_back(
            {"texture", blueprint.entity_id + "_skin_texture", "skin",
             "roughness=" + std::to_string(blueprint.skin.roughness) +
                 " subsurface=" + std::to_string(blueprint.skin.subsurface) +
                 " age_years=" + std::to_string(blueprint.skin.age_years) +
                 (blueprint.skin.damage_response ? " (needs damage-response variant)" : "")});

        // Clothing material -- always required, driven by clothing.
        spec.requirements.push_back(
            {"material", blueprint.entity_id + "_clothing_material", "clothing",
             "material=" + blueprint.clothing.material +
                 (blueprint.clothing.adaptive_damage ? " (needs wear-state variants)" : "") +
                 (blueprint.clothing.weather_response ? " (needs weather-response variant)" : "")});

        // Material genome, if attached -- a real, separate object asset
        // (e.g. a jacket as its own tracked material), distinct from
        // the clothing texture above.
        if (blueprint.has_material) {
            std::string notes = "type=" + blueprint.material_identity.type +
                                 " wear_state=" + std::to_string(blueprint.material_properties.wear_state) +
                                 " age_years=" + std::to_string(blueprint.material_properties.age_years);
            spec.requirements.push_back(
                {"material", blueprint.material_id.empty() ? blueprint.entity_id + "_material" : blueprint.material_id,
                 "material_genome", notes});
        }

        // Style reference -- if a visual style is attached, its
        // visual_rules are real production guidance (not invented).
        if (blueprint.has_visual_style) {
            spec.requirements.push_back(
                {"reference", blueprint.style_name.empty() ? blueprint.style_id : blueprint.style_name,
                 "visual_style",
                 "line=" + blueprint.visual_rules.line_quality + " color=" + blueprint.visual_rules.color_behavior +
                     " shape=" + blueprint.visual_rules.shape_behavior +
                     " motion=" + blueprint.visual_rules.motion_behavior});
        }

        return spec;
    }
};

}  // namespace dominus::visualforge
