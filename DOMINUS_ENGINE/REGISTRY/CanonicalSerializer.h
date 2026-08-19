// REGISTRY/CanonicalSerializer.h
// Produces a fixed-field-order, deterministic string from a
// CombatIdentity -- the same genome values always produce the exact same
// bytes, regardless of how the source .dominus/combat_dna JSON happened
// to format whitespace or key order. This is what "hashes are stable"
// actually requires: hashing raw source JSON bytes would make the hash
// depend on formatting, not content, and two files with identical genome
// values but different formatting would wrongly get different hashes.
//
// SerializeCreatureGenome extends this to a second, much richer genome
// type (12 sections vs. 6 flat fields) -- a real, if partial, answer to
// the open "does this generalize" question flagged in the Registry
// Prototype phase: the CanonicalSerializer/hash discipline DOES
// generalize cleanly to CreatureGenome's complexity. What still does
// NOT generalize (flagged, not hidden): GenomeCompiler/ImmutableArtifact
// bake in CombatGenome's DecisionWeights specifically -- there is no
// CreatureGenome decoder yet to bake, so CreatureGenomeCompiler (see
// GenomeCompiler.h) returns hash+canonical bytes only, not a full
// ImmutableArtifact. Structurally unifying the two remains a real,
// separate decision.
#pragma once

#include <sstream>
#include <string>

#include "CHARACTER/Genome/CombatIdentity.h"
#include "CHARACTER/Genome/CombatPhysicsGenome.h"
#include "CHARACTER/Genome/CombatStyleGenome.h"
#include "CHARACTER/Genome/CreatureGenome.h"
#include "CHARACTER/Genome/GameDesignGenome.h"
#include "CHARACTER/Genome/MaterialGenome.h"
#include "CHARACTER/Genome/VisualGenome.h"
#include "CHARACTER/Genome/VisualStyleGenome.h"

namespace dominus::registry {

class CanonicalSerializer {
public:
    static std::string SerializeCombatGenome(const character::CombatIdentity& identity) {
        // Fixed field order, fixed separator, no whitespace -- the exact
        // opposite of "however the source JSON happened to be written".
        std::ostringstream out;
        out << "style=" << identity.style << ";range=" << identity.range << ";pressure=" << identity.pressure
            << ";counter=" << identity.counter << ";mobility=" << identity.mobility << ";risk=" << identity.risk;
        return out.str();
    }

    static std::string SerializeCreatureGenome(const character::CreatureGenome& g) {
        std::ostringstream out;
        out << "species_name=" << g.identity.species_name << ";common_name=" << g.identity.common_name
            << ";origin=" << g.identity.origin;
        out << ";body_plan=" << g.taxonomy.body_plan << ";classification=" << g.taxonomy.classification
            << ";limb_count=" << g.taxonomy.limb_count;
        out << ";height_m=" << g.anatomy.height_m << ";weight_kg=" << g.anatomy.weight_kg
            << ";armor_type=" << g.anatomy.armor_type << ";notable_features=" << JoinCsv(g.anatomy.notable_features)
            << ";wing_area_m2=" << g.anatomy.wing_area_m2;
        out << ";metabolism=" << g.physiology.metabolism
            << ";energy_sources=" << JoinCsv(g.physiology.energy_sources)
            << ";reproduction=" << g.physiology.reproduction << ";mutation_rate=" << g.physiology.mutation_rate
            << ";requires_water_respiration=" << (g.physiology.requires_water_respiration ? "true" : "false");
        out << ";sense_types=" << JoinCsv(g.senses.sense_types)
            << ";perception_range_m=" << g.senses.perception_range_m;
        out << ";primary_mode=" << g.locomotion.primary_mode << ";top_speed_kmh=" << g.locomotion.top_speed_kmh;
        out << ";tier=" << static_cast<int>(g.cognition.tier) << ";problem_solving=" << g.cognition.problem_solving
            << ";communication=" << g.cognition.communication;
        out << ";temperament=" << g.behavior.temperament << ";behavior_aggression=" << g.behavior.aggression
            << ";territoriality=" << g.behavior.territoriality << ";combat_role=" << g.behavior.combat_role;
        out << ";trophic_role=" << g.ecology.trophic_role << ";diet_type=" << g.ecology.diet_type
            << ";preferred_prey=" << JoinCsv(g.ecology.preferred_prey)
            << ";natural_predators=" << JoinCsv(g.ecology.natural_predators) << ";habitat=" << g.ecology.habitat;
        out << ";combat_aggression=" << g.combat.aggression << ";combat_speed=" << g.combat.speed
            << ";combat_durability=" << g.combat.durability
            << ";combat_intelligence=" << g.combat.combat_intelligence << ";combat_range=" << g.combat.range;
        out << ";lifespan_years=" << g.growth.lifespan_years << ";life_stages=" << JoinCsv(g.growth.life_stages)
            << ";maturity_age_years=" << g.growth.maturity_age_years;
        out << ";evolutionary_origin=" << g.evolution.evolutionary_origin
            << ";lineage=" << JoinCsv(g.evolution.lineage)
            << ";adaptation_potential=" << g.evolution.adaptation_potential;
        return out.str();
    }

private:
    // Vectors preserve JSON array order (already deterministic from the
    // source file), joined with a separator that can't silently collide
    // with content -- "|" chosen because none of this schema's free-form
    // fields are expected to contain it, same assumption the ";"/"="
    // separators above already make.
    static std::string JoinCsv(const std::vector<std::string>& items) {
        std::ostringstream out;
        for (size_t i = 0; i < items.size(); ++i) {
            out << items[i];
            if (i + 1 < items.size()) out << "|";
        }
        return out.str();
    }

public:
    static std::string SerializeCombatStyleGenome(const character::CombatStyleGenome& g) {
        std::ostringstream out;
        out << "style_name=" << g.style_name << ";ancestry=" << JoinCsv(g.ancestry)
            << ";aggression=" << g.aggression << ";defense=" << g.defense << ";mobility=" << g.mobility
            << ";pressure=" << g.pressure << ";deception=" << g.deception << ";endurance=" << g.endurance
            << ";precision=" << g.precision << ";adaptability=" << g.adaptability
            << ";evolution_capacity=" << g.evolution_capacity << ";range_control=" << g.range_control
            << ";rhythm=" << g.rhythm << ";philosophy=" << g.philosophy
            << ";weaknesses=" << JoinCsv(g.weaknesses);
        return out.str();
    }

    static std::string SerializeCombatPhysicsGenome(const character::CombatPhysicsGenome& g) {
        std::ostringstream out;
        out << "mass_kg=" << g.body.mass_kg << ";height_m=" << g.body.height_m << ";density=" << g.body.density
            << ";armor=" << g.body.armor << ";flexibility=" << g.body.flexibility
            << ";stamina=" << g.energy.stamina << ";recovery_rate=" << g.energy.recovery_rate
            << ";fatigue_rate=" << g.energy.fatigue_rate << ";strike_force=" << g.impact.strike_force
            << ";grapple_force=" << g.impact.grapple_force << ";durability=" << g.impact.durability;
        return out.str();
    }

    static std::string SerializeGameDesignGenome(const character::GameDesignGenome& g) {
        std::ostringstream out;
        out << "genre=" << g.genre << ";core_loop=" << g.core_loop << ";difficulty=" << g.difficulty
            << ";risk_reward_balance=" << g.risk_reward_balance;
        return out.str();
    }

    static std::string SerializeVisualGenome(const character::VisualGenome& g) {
        std::ostringstream out;
        out << "body_type=" << g.form.body_type << ";silhouette=" << g.form.silhouette
            << ";proportion=" << g.form.proportion << ";shape_language=" << g.form.shape_language
            << ";skin_roughness=" << g.skin.roughness << ";skin_subsurface=" << g.skin.subsurface
            << ";skin_age=" << g.skin.age_years
            << ";skin_damage_response=" << (g.skin.damage_response ? "true" : "false")
            << ";clothing_material=" << g.clothing.material
            << ";clothing_adaptive_damage=" << (g.clothing.adaptive_damage ? "true" : "false")
            << ";clothing_weather_response=" << (g.clothing.weather_response ? "true" : "false")
            << ";aura=" << g.presence.aura << ";threat_signature=" << g.presence.threat_signature
            << ";emotional_visual_weight=" << g.presence.emotional_visual_weight
            << ";style_id=" << g.presence.style_id;
        return out.str();
    }

    static std::string SerializeMaterialGenome(const character::MaterialGenome& g) {
        std::ostringstream out;
        out << "material_id=" << g.material_id << ";type=" << g.identity.type
            << ";age_years=" << g.properties.age_years << ";wear_state=" << g.properties.wear_state
            << ";damage_history=" << (g.properties.damage_history ? "true" : "false")
            << ";weather_exposure=" << (g.properties.weather_exposure ? "true" : "false");
        return out.str();
    }

    static std::string SerializeVisualStyleGenome(const character::VisualStyleGenome& g) {
        std::ostringstream out;
        out << "style_id=" << g.style_id << ";name=" << g.name
            << ";line_quality=" << g.visual_rules.line_quality
            << ";color_behavior=" << g.visual_rules.color_behavior
            << ";shape_behavior=" << g.visual_rules.shape_behavior
            << ";motion_behavior=" << g.visual_rules.motion_behavior
            << ";influences=" << JoinCsv(g.influences);
        return out.str();
    }
};

}  // namespace dominus::registry
