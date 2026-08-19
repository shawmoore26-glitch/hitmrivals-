// WORLD/Core/WorldPersistence.h
// Society Phase 0: "Entity exists -> History recorded -> World saved ->
// Creator leaves -> Creator returns -> World continues." This is the
// missing organ -- the first moment anything in this engine survives
// past process exit.
//
// Scope, stated plainly: WorldPersistence saves and restores what WORLD
// actually owns -- entity ids, SourceRefComponent (which .dominus file
// each came from), SpatialComponent (position), elapsed simulation time,
// and the history log. It does NOT save bound runtime state (current
// motion-graph state, current combat phase, current AI decision) --
// those live in CHARACTER/COMBAT/ANIMATION-owned components WORLD
// doesn't know exist, and per WORLD LAW 001/002 never should. Loading a
// saved world means: read these records back, then re-run the EXISTING
// pipeline (DominusSerializer::Load + RigBinder::Bind +
// CombatBinder::Bind) against each entity's SourceRefComponent to
// rebuild full bound state, then restore its saved SpatialComponent.
// That reconstruction step is external orchestration code's job (same
// as every other place WORLD and COMBAT/CHARACTER meet) -- not
// WorldPersistence's, which only ever touches WORLD/CORE types.
#pragma once

#include <filesystem>

#include "CORE/Serialization/DominusSerializer.h"  // for VoidResult
#include "WORLD/Core/SourceRefComponent.h"
#include "WORLD/Core/SpatialComponent.h"
#include "WORLD/Core/World.h"

namespace dominus::world {

struct EntitySaveRecord {
    std::string entity_id;
    std::string source_ref;   // empty if the entity has no SourceRefComponent
    bool has_spatial = false;
    SpatialComponent spatial;  // only valid if has_spatial
};

struct WorldLoadResult {
    bool ok = false;
    std::string error;
    float elapsed_seconds = 0.0f;
    std::vector<EntitySaveRecord> entities;
    std::vector<HistoryEvent> history;
};

class WorldPersistence {
public:
    // Writes world.json, entities/<id>.json (one per entity currently in
    // the registry), and history/timeline.json under `dir`, creating the
    // directory structure if needed.
    static core::VoidResult Save(const World& world, const std::filesystem::path& dir);

    // Reads the same structure back into plain records -- does NOT touch
    // an EntityRegistry or attempt to rebind anything (see file header).
    static WorldLoadResult Load(const std::filesystem::path& dir);
};

}  // namespace dominus::world
