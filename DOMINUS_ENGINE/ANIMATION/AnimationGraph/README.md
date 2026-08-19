# AnimationGraph

**Status: built.** The state machine + blend transition + layering core of
the Motion Intelligence System.

- `MotionGraph.h` -- pure data: named states (each pointing at a clip name)
  and transitions (trigger name, blend duration, optional auto-fire when
  the source clip finishes)
- `MotionGraphLoader` -- loads a graph from JSON, referenced by a
  `.dominus` object's `motion_graph` field
- `MotionGraphEvaluator` -- the runtime: tracks current state and clip
  time, handles `Trigger()` requests, blends outgoing/incoming poses over
  the transition's duration, and auto-fires transitions when a
  non-looping clip completes
- `AnimationLayerStack` -- composites multiple evaluators' poses, each
  layer optionally masked to specific bones and blended in by weight (e.g.
  an upper-body-only attack over a full-body idle)

No blend trees / 2D blend spaces yet (single-clip states only) -- a
tracked follow-up once locomotion needs directional blending, not a
current requirement.
