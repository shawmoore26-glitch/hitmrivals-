// CHARACTER/HitmBridge/HitmMatch.h
// ROADMAP.md Track H, Phase 3 of HITM_BROOKLYN_VS_ROCKET_PLAYABILITY_AUDIT.md
// (authorized after Phase 1's and Phase 2's own stop-and-verify
// checkpoints): the first real, complete, CPU-observable two-fighter
// HITM match. Owns exactly the real match/round/timer state the audit
// found belongs at this level, never on an individual
// `HitmFighterRuntime` (see that class's own "PHASE 2" / "Step 7"
// header comment): a real, direct port of hitm-engine's own
// `CombatSystem.js` -- specifically `startMatch()`, `resetRound()`,
// `update()`'s `roundIntro`/`fight`/`ko` phase machine, and
// `_endRound()`/`_ko()`.
//
// Deliberately MATCH-LEVEL, not fighter-level, matching the real
// architecture:
//   Match -> Round -> Timer -> Fighter A / Fighter B -> Combat resolution
// This class is that Match. It owns two `HitmFighterRuntime`s (already
// real, already move-safe, already independently proven) and drives them
// together one real frame at a time; it never reaches into either
// fighter's own private state, only their already-public seams
// (`AdvanceFrame`, `TakeHit`, `ResolveOutgoingHitLanded`, `SetFacing`,
// `ResetForNewRound`, `Snapshot`, `State`, `SpecialMove`).
//
// REAL PHASE MACHINE (a direct port of `CombatSystem.js`'s own
// `state.phase`: `'roundIntro' | 'fight' | 'ko' | 'matchOver'`):
//   - kRoundIntro: a real, hardcoded 120-frame pre-round window
//     (`CombatSystem.js:31,49`, `roundT:120`) -- neither fighter is
//     ticked at all during it (`update()`'s own `return;` right after
//     the intro check, line 54), matching the real engine exactly.
//   - kFight: live gameplay. Both fighters advance every real frame;
//     real position-based hit detection
//     (`CHARACTER/HitmBridge/HitmMeleeHitCheck.h`) resolves exactly once
//     per real attack activation (see `ResolveAttack()`'s own comment
//     for why no separate "already hit" flag is needed); the real
//     per-frame timer (`rounds.timer_seconds * 60`, already imported by
//     Module 4) counts down; a real sudden-death rule
//     (`CombatSystem.js:59-61`) resolves a timeout by comparing real
//     hp/max_hp fractions.
//   - kKO: a real, hardcoded 150-frame post-KO settle window
//     (`CombatSystem.js:460`, `s.koT=150`) -- both fighters keep
//     advancing (so a KO'd fighter's own real gravity/ground-clamp
//     settling still runs), then the round resolves.
//   - kMatchOver: terminal. A direct port of `_endRound()`
//     (`CombatSystem.js:464-473`): the round winner's real `rounds won`
//     count increments; if it reaches the real, already-imported
//     `HitmGameRules::Rounds().to_win`, the match ends; otherwise the
//     round number increments and both fighters are real-reset
//     (`HitmFighterRuntime::ResetForNewRound`) to the real per-match
//     starting positions/facing (`CombatSystem.js:26`, hardcoded
//     `x=300/760`, `facing=+1/-1` -- uniform engine constants, not
//     per-fighter authored data, the same category as
//     `HitmFighterRuntime.cpp`'s own `kBaseHp`).
//
// ONE DELIBERATE, DOCUMENTED DEPARTURE FROM THE REAL ENGINE'S OWN
// CONTROL FLOW, not from its OUTCOME: the real engine determines a
// round's loser by re-inspecting `fighters[0].state===STATE.KO` inside
// `_endRound()` (real, including its own "a double-KO in the same frame
// favors fighter B" quirk, since only fighter A's state is actually
// checked). Reproducing that by force-setting a fighter's own `state` to
// kKO on a real TIMEOUT (no hit landed) would need a new "force this
// fighter into kKO" seam on `HitmFighterRuntime` that nothing else
// needs. Instead, this class tracks `roundLoserIndex_` itself --
// computed identically to the real tie-break for a genuine hit-KO
// (fighter A checked first), and directly from the real hp-fraction
// comparison on a timeout -- so `EndRound()` always resolves the exact
// same real winner the real engine would, without requiring
// `HitmFighterRuntime` to expose a mutator nothing else needs.
//
// EXPLICITLY OUT OF SCOPE FOR THIS PHASE (per this session's own
// checkpoint discipline): Rocket's own real "Ghost Dash" special (a
// structurally different move type, `_zoneHit` + travel scheduling --
// Rocket's `SpecialMove()` is `nullptr` today, so his `kSpecial` input
// is a real, honest no-op in every match this class drives; making his
// special work is the audit's own Phase 4). Also out of scope, per the
// same discipline: rendering, full bone-hierarchy FK, new asset
// authoring, `character.json`'s generated values, real combo damage
// scaling, the attacker's own read-engine damage multiplier, networking,
// menus/UI, and any fighter beyond these two.
#pragma once

#include <optional>

#include "CHARACTER/HitmBridge/HitmCombatGenome.h"
#include "CHARACTER/HitmBridge/HitmFighterRuntime.h"
#include "CHARACTER/HitmBridge/HitmGameRules.h"
#include "CHARACTER/HitmBridge/HitmIdentityRecord.h"
#include "CORE/Serialization/DominusSerializer.h"  // for core::Result<T>

namespace dominus::character::hitm {

enum class HitmMatchPhase {
    kRoundIntro,
    kFight,
    kKO,
    kMatchOver,
};

// A plain, comparable snapshot of every piece of state this class
// exposes -- same discipline as `HitmFighterSnapshot`, and the basis for
// this class's own determinism proof.
struct HitmMatchSnapshot {
    HitmMatchPhase phase = HitmMatchPhase::kRoundIntro;
    int round = 1;
    int round_intro_frames_remaining = 0;
    int timer_frames_remaining = 0;
    int ko_frames_remaining = 0;
    int fighter_a_rounds_won = 0;
    int fighter_b_rounds_won = 0;
    HitmFighterSnapshot fighter_a;
    HitmFighterSnapshot fighter_b;
    // Set exactly when phase becomes kMatchOver; -1/false before that.
    bool match_over = false;
    int match_winner_index = -1;  // 0 = fighter A, 1 = fighter B

    bool operator==(const HitmMatchSnapshot&) const = default;
};

class HitmMatch {
public:
    // Builds a real two-fighter match from already-imported,
    // already-validated real data (Modules 1/2/4) -- fighter A and
    // fighter B, in that real, fixed order (matching
    // `CombatSystem.js`'s own `mk(defA,0,...)`/`mk(defB,1,...)`: fighter
    // A always starts at the real x=300 facing right, fighter B always
    // at real x=760 facing left). Fails (Result::Fail) if either
    // fighter's own `HitmFighterRuntime::Create` fails -- see that
    // function's own real, specific failure reasons; this does not
    // relax or route around them further.
    static core::Result<HitmMatch> Create(const HitmIdentityRecord& recordA, const HitmCombatGenome& genomeA,
                                            const HitmIdentityRecord& recordB, const HitmCombatGenome& genomeB,
                                            const HitmGameRules& rules);

    HitmMatch(HitmMatch&&) noexcept = default;
    HitmMatch& operator=(HitmMatch&&) noexcept = default;
    HitmMatch(const HitmMatch&) = delete;
    HitmMatch& operator=(const HitmMatch&) = delete;
    ~HitmMatch() = default;

    // Advances exactly one real match frame. During kRoundIntro and
    // kKO, inputs are ignored (matching the real engine's own
    // `roundIntro` early-return and `ko`-phase `{}` empty-input tick,
    // respectively) -- still safe/harmless to pass real input during
    // those phases, exactly like a player mashing buttons during a real
    // round-intro or KO freeze.
    void AdvanceFrame(HitmInputCommand inputA, HitmInputCommand inputB);

    HitmMatchSnapshot Snapshot() const;

private:
    HitmMatch(HitmGameRules rules, HitmFighterRuntime fighterA, HitmFighterRuntime fighterB);

    void ResetRound();  // real resetRound()
    void EndRound();    // real _endRound()

    // Real position-based hit resolution for one attacker/defender pair
    // -- see the .cpp for the full real-formula citation.
    static void ResolveAttack(HitmFighterRuntime& attacker, HitmFighterRuntime& defender);

    HitmGameRules rules_;
    HitmFighterRuntime fighterA_;
    HitmFighterRuntime fighterB_;

    HitmMatchPhase phase_ = HitmMatchPhase::kRoundIntro;
    int round_ = 1;
    int roundIntroFramesRemaining_ = 0;
    int timerFramesRemaining_ = 0;
    int koFramesRemaining_ = 0;
    int fighterARoundsWon_ = 0;
    int fighterBRoundsWon_ = 0;
    // See this header's top comment for why this class tracks the round
    // loser itself rather than re-deriving it from fighter state the way
    // the real engine's own _endRound() does. Set exactly once per
    // kKO-phase entry (by a real hit-KO or a real timeout), consumed and
    // cleared by EndRound().
    std::optional<int> roundLoserIndex_;
    bool matchOver_ = false;
    int matchWinnerIndex_ = -1;
};

}  // namespace dominus::character::hitm
