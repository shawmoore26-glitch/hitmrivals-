// VISUALFORGE/CharacterBlueprint.h
// "Identity + Visual Rules + Material State + Style Language + World
// History" -- five real, already-existing data sources, combined into
// one structured, authoritative visual production spec. Nothing here
// is invented: every field is either copied straight from a genome
// that already validated it, or (for the cross-reference flag) a real
// comparison between two fields that both already exist.
#pragma once

#include <string>
#include <vector>

#include "CHARACTER/Genome/MaterialGenome.h"
#include "CHARACTER/Genome/VisualGenome.h"
#include "CHARACTER/Genome/VisualMemorySummary.h"
#include "CHARACTER/Genome/VisualStyleGenome.h"
#include "WORLD/Core/WorldHistory.h"

namespace dominus::visualforge {

struct CharacterBlueprint {
    std::string entity_id;

    // Identity -- VisualGenome, always present (the one required input).
    character::VisualForm form;
    character::VisualSkin skin;
    character::VisualClothing clothing;
    character::VisualPresence presence;

    // Material State -- MaterialGenome, optional (an entity may have no
    // material ref attached, e.g. a bare-skin creature with no clothing/
    // equipment genome).
    bool has_material = false;
    std::string material_id;
    character::MaterialIdentity material_identity;
    character::MaterialProperties material_properties;

    // Style Language -- VisualStyleGenome, optional.
    bool has_visual_style = false;
    std::string style_id;
    std::string style_name;
    character::VisualStyleRules visual_rules;
    std::vector<std::string> style_influences;

    // World History -- VisualMemorySummary, optional (only populated
    // when a WorldHistory was supplied to Build).
    bool has_memory = false;
    character::VisualMemorySummary memory;

    // Real cross-check, not a merge: VisualGenome.presence.style_id is
    // supposed to reference a VisualStyleGenome.style_id (see
    // VisualPresence's own doc comment). True whenever both are
    // present and agree, OR when at most one side names a style at all
    // (nothing to contradict). False only when both are set AND
    // disagree -- a real, checkable data-integrity signal, not an
    // invented similarity score.
    bool style_reference_matches = true;
};

class CharacterBlueprintForge {
public:
    // visual is the one required input -- a CharacterBlueprint without
    // an identity isn't a character blueprint. material/visualStyle/
    // history are all optional and each is nullptr-safe, same
    // graceful-degrade discipline RigBinder's optional components
    // already established.
    static CharacterBlueprint Build(const std::string& entityId, const character::VisualGenome& visual,
                                     const character::MaterialGenome* material = nullptr,
                                     const character::VisualStyleGenome* visualStyle = nullptr,
                                     const world::WorldHistory* history = nullptr) {
        CharacterBlueprint blueprint;
        blueprint.entity_id = entityId;
        blueprint.form = visual.form;
        blueprint.skin = visual.skin;
        blueprint.clothing = visual.clothing;
        blueprint.presence = visual.presence;

        if (material) {
            blueprint.has_material = true;
            blueprint.material_id = material->material_id;
            blueprint.material_identity = material->identity;
            blueprint.material_properties = material->properties;
        }

        if (visualStyle) {
            blueprint.has_visual_style = true;
            blueprint.style_id = visualStyle->style_id;
            blueprint.style_name = visualStyle->name;
            blueprint.visual_rules = visualStyle->visual_rules;
            blueprint.style_influences = visualStyle->influences;
        }

        if (history) {
            blueprint.has_memory = true;
            blueprint.memory = character::VisualMemoryDeriver::Derive(*history, entityId);
        }

        if (visualStyle && !visual.presence.style_id.empty()) {
            blueprint.style_reference_matches = (visual.presence.style_id == visualStyle->style_id);
        }
        // else: nothing to contradict -- stays true (its own default).

        return blueprint;
    }
};

}  // namespace dominus::visualforge
