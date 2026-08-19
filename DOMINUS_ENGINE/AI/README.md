# AI

**Status: Phase 3 foundation + Phase 3.75 genome wiring -- built.**

- `Agents/BehaviorTree.h` -- Selector/Sequence/Condition/Action nodes
- `Agents/CombatAI.h` -- `OpponentPatternTracker` (bounded move-history
  buffer, most-frequent-move detection) driving a behavior tree that now
  genuinely reads `CHARACTER::DecisionWeights` (LAW C011 + C003 closed
  together): a poor-counter genome never attempts the counter branch, a
  highly aggressive genome presses attacks instead of blocking, and
  `DecideMoveName()` picks the actual move via `COMBAT::MoveSelector`
  once a category is chosen. `ApplyProfile()` lets a transformation's
  `AIProfile` bias an already-decoded genome (scaled, not replaced).

Two genomes facing the identical opponent pattern now provably decide
differently -- verified by test and live via `dominus-cli ai`. As of
Phase 3.9, that decision also genuinely reaches the skeleton runtime
(the motion library is complete), so the full pipeline closes end to
end, not just up to the decision. Not data-driven yet -- trees are
code-constructed, no `.dominus` `ai_behavior` ref loader exists. No
cross-session memory ("previous encounters") or emotional-state
modeling. See `../ROADMAP.md` Phase 3.9 for full status.

`Learning/` and `Director/` remain placeholders -- `Learning/` (neural
systems) is explicitly Phase 5 scope per the Constitution; `Director/`
(pacing/encounter direction) has no concrete requirement yet.
