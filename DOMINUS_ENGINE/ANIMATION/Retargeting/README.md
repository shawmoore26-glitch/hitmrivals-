# Retargeting

**Status: foundation built.** `RetargetMap`/`RetargetMapLoader` -- a
target-bone -> source-bone name mapping, loaded via a `.dominus` object's
`retarget_map` ref. `Retarget.h` -- copies an `AnimationClip`'s tracks onto
a differently-named skeleton per that mapping.

Proven end to end: Brooklyn's `idle` clip, authored against Brooklyn's bone
names, correctly drives a differently-named "generic biped" skeleton (see
`tests/motion/test_retargeting.cpp` and `tests/fixtures/generic_biped.*`).

Name-mapping only -- no proportional rescaling for skeletons with different
bone lengths, no topology-mismatch handling (missing/extra bones). Both are
tracked follow-ups, not silently assumed away.
