# PHYSICS

**Status: Phase 4.1 -- built (Module 2 of the World Engine).**

Same rule as `WORLD`, one level down: **`PHYSICS` knows entities have
mass/colliders/position (via `WORLD::SpatialComponent`). Physics does not
know games exist** -- zero includes of `CHARACTER`/`COMBAT`/`ANIMATION`
anywhere in this directory, verified by grepping actual `#include` lines
before any test was written.

- `RigidBody.h` -- mass, velocity, a per-tick force accumulator,
  `is_static`, `affected_by_gravity`. Pure physical state.
- `Collider.h` -- circle/box shape (box shape is data only -- see
  unresolved below), layer/mask bitwise collision filtering.
- `PhysicsSystem.h` -- gravity + force integration into
  `WORLD::SpatialComponent`, exposed as a `world::WorldSystemFn` via
  `AsWorldSystem()`.
- `CollisionSystem.h` -- circle-circle pairwise detection + layer/mask
  filtering + positional-separation-and-damping resolution, also exposed
  as a `WorldSystemFn`.
- `ConstraintSolver.h` -- distance constraints (Jakobsen-style position
  correction).

**The milestone proof**
(`tests/physics/test_universal_physics_proof.cpp`): three entities with
nothing in common -- a real Brooklyn (full `.dominus` load, real combat
genome), a bare `Crate` tag, an unrelated `VehicleController` stand-in --
share only `SpatialComponent` + `Collider` + `RigidBody`. Run through
`World.Tick()` -> `PhysicsSystem` -> `CollisionSystem`: the fighter's
motion pushes the crate, and all three keep their unrelated identity
components completely untouched by `PHYSICS`. Live via
`dominus-cli physics`.

**Genuinely unresolved:** box-box and circle-box collision are not
implemented (`Collider` has the `kBox` shape as data; nothing tests it
yet); resolution is positional-separation-plus-damping, not a real
impulse/restitution solver; `CollisionSystem::Detect` is an O(n²) scan,
same open item as `EntityRegistry`'s linear scan; `ConstraintSolver`
supports one constraint type; `PHYSICS` and `COMBAT` remain fully
decoupled siblings -- nothing wires collision results into `COMBAT`'s own
`HitSystem`/`ReactionSystem` yet (deliberate, to keep the independence
proof clean). See `../ROADMAP.md` Phase 4.1 for the full status.
