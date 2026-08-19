// CHARACTER/Genome/CombatPhysicsGenome.h
// Fills a real, specific gap: COMBAT::ClashSystem already resolves
// attack-vs-ATTACK (LAW C008: force/speed/skill when two attacks
// collide). Nothing resolves attack-vs-BODY -- how much an attack's
// force actually costs a defender given their mass, armor, durability,
// and which body part was hit. This genome supplies that data; a
// separate calculator (COMBAT/PhysicsCombat/ImpactSolver.h) supplies
// the real math. Neither duplicates ClashSystem's job.
//
// Deliberately NOT built here, flagged rather than silently skipped: a
// second Combat State Machine (CombatController already has a real,
// tested, motion-graph-driven one -- see Phase 3.9), a second Decision
// Engine (AI::CombatAI already has a real, genome-weighted behavior
// tree -- see Phase 3.75), and fighting-game frame data
// (startup/active/recovery/advantage already exist, real and tested,
// on COMBAT::MoveDef since Phase 3). Building parallel versions of any
// of these would fragment decision-making across systems that don't
// talk to each other -- a regression, not an addition.
#pragma once

namespace dominus::character {

struct CombatBodyModel {
    float mass_kg = 70.0f;
    float height_m = 1.8f;
    float density = 1.0f;       // relative density multiplier, > 0
    float armor = 0.0f;         // 0-1, fraction of incoming force absorbed before damage calc
    float flexibility = 0.5f;   // 0-1
};

struct CombatEnergyModel {
    float stamina = 100.0f;         // > 0
    float recovery_rate = 1.0f;     // stamina/sec, >= 0
    float fatigue_rate = 1.0f;      // stamina-cost multiplier, > 0
};

struct CombatImpactProfile {
    float strike_force = 1.0f;   // force multiplier for strikes, > 0
    float grapple_force = 1.0f;  // force multiplier for grapples, > 0
    float durability = 50.0f;    // resistance threshold used by ImpactSolver, > 0
};

struct CombatPhysicsGenome {
    CombatBodyModel body;
    CombatEnergyModel energy;
    CombatImpactProfile impact;
};

}  // namespace dominus::character
