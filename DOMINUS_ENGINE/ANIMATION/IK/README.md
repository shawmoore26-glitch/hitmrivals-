# IK

**Status: built.** `TwoBoneIK.h` -- analytic (law-of-cosines) two-bone
solver. `IKChain.h`/`IKChainLoader` -- names a root/mid/end bone triplet on
a skeleton and applies the solver to bend that chain toward a world-space
target, loaded via a `.dominus` object's `ik_chains` ref list.

Proven against a real 3-bone shoulder/elbow/wrist chain
(`tests/fixtures/ik_arm_test.skel.json`) -- the end effector provably
reaches (or clamps toward) an arbitrary target. See
`tests/motion/test_ik.cpp`.

No iterative solver (FABRIK/CCD) yet for chains longer than two bones --
not needed until a rig has one.
