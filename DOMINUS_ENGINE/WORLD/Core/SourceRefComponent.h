// WORLD/Core/SourceRefComponent.h
// The missing link for persistence: a MetaBinObject itself doesn't
// remember which .dominus file it was loaded from (DominusSerializer
// just parses a file into components, it doesn't stamp the object with
// its own origin). Without this, "save the world" would have nothing to
// reconstruct a full bound entity FROM on reload -- WorldPersistence can
// save an entity's position, but not rebuild its skeleton/combat/
// animation state, because those are CHARACTER/COMBAT/ANIMATION-owned
// components WORLD doesn't know exist (and per WORLD LAW 001/002's own
// rule, never should). Attaching this component at entity-creation time
// (external orchestration code's job, same as SpatialComponent) is what
// lets a reload step re-run the EXISTING DominusSerializer::Load +
// RigBinder::Bind + CombatBinder::Bind pipeline against the right file.
#pragma once

#include <string>

namespace dominus::world {

struct SourceRefComponent {
    std::string dominus_path;  // relative to the world's base content directory
};

}  // namespace dominus::world
