# ANIMATION

**Status: Phase 2 + DOMINUS MOTION INTELLIGENCE SYSTEM -- built.**

- `SkeletonSystem/` -- `Transform2D`, `Bone`, `Skeleton`, `AnimationClip`,
  and their file loaders (`.skel.json`, `.clip.json`)
- `ProceduralMotion/` -- `AnimationPlayer` (samples a clip against a
  skeleton, composes world poses through the hierarchy) and
  `ProceduralHooks` (a post-process pipeline over a final pose --
  breathing, look-at, and any custom `PoseModifier`)
- `AnimationGraph/` -- `MotionGraph`/`MotionGraphLoader` (state machine
  data, loaded via `.dominus`'s `motion_graph` ref) and
  `MotionGraphEvaluator` (runtime: current state, blend transitions,
  auto-on-complete transitions) and `AnimationLayerStack` (masked, weighted
  compositing of multiple evaluators -- e.g. an upper-body attack layered
  over a full-body idle)
- `IK/` -- `TwoBoneIK` (analytic two-bone solver) and `IKChain`/
  `IKChainLoader` (names which three skeleton bones form a chain, loaded
  via `.dominus`'s `ik_chains` ref list)
- `Retargeting/` -- `RetargetMap`/`RetargetMapLoader` (bone-name mapping,
  loaded via `.dominus`'s `retarget_map` ref) and `Retarget` (renames a
  clip's tracks onto a differently-named skeleton)

2D skeletal, not 3D -- HITM CITY's actual production pipeline is 2D/Spine
(see the `spine-fighting-game` skill).

**Genuinely unresolved:** retargeting is name-mapping only, no proportional
rescaling for different bone lengths; IK is two-bone analytic only, no
iterative solver for longer chains; look-at hook ignores parent rotation
(not a full constraint solver). See `../ROADMAP.md` Phase 2.5 for the full
status.

`MotionCapture/` remains a placeholder -- no mocap import pipeline yet.
