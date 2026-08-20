// CHARACTER/HitmBridge/HitmMatch.cpp
#include "CHARACTER/HitmBridge/HitmMatch.h"

#include "CHARACTER/HitmBridge/HitmMeleeHitCheck.h"
#include "COMBAT/HitSystem/MoveDef.h"  // for combat::kFramesPerSecond

namespace dominus::character::hitm {

using core::Result;

namespace {

// Real, hardcoded, uniform engine constants (CombatSystem.js:26,
// startMatch()'s own `mk(defA,0,300,1,...)`/`mk(defB,1,760,-1,...)`) --
// NOT per-fighter authored data, same category as HitmFighterRuntime.cpp's
// own kBaseHp / HitmMeleeHitCheck.h's kMeleeFighterWidth.
constexpr float kMatchStartXFighterA = 300.0f;
constexpr float kMatchStartXFighterB = 760.0f;
constexpr int kMatchStartFacingA = 1;
constexpr int kMatchStartFacingB = -1;

// CombatSystem.js:31,49 -- real, hardcoded 120-frame pre-round window.
constexpr int kRoundIntroFrames = 120;
// CombatSystem.js:460, _ko()'s own `s.koT=150` -- real, hardcoded
// 150-frame post-KO settle window.
constexpr int kKOFrames = 150;

}  // namespace

Result<HitmMatch> HitmMatch::Create(const HitmIdentityRecord& recordA, const HitmCombatGenome& genomeA,
                                     const HitmIdentityRecord& recordB, const HitmCombatGenome& genomeB,
                                     const HitmGameRules& rules) {
    auto fighterAResult = HitmFighterRuntime::Create(recordA, genomeA, rules);
    if (!fighterAResult.ok) {
        return Result<HitmMatch>::Fail("HitmMatch::Create: fighter A ('" + recordA.fighter_id +
                                        "'): " + fighterAResult.error);
    }
    auto fighterBResult = HitmFighterRuntime::Create(recordB, genomeB, rules);
    if (!fighterBResult.ok) {
        return Result<HitmMatch>::Fail("HitmMatch::Create: fighter B ('" + recordB.fighter_id +
                                        "'): " + fighterBResult.error);
    }

    HitmMatch match(rules, std::move(*fighterAResult.value), std::move(*fighterBResult.value));
    match.round_ = 1;
    // Real startMatch() positions both fighters directly at construction
    // time rather than calling resetRound() itself -- but resetRound()'s
    // own real effect (position/facing/full-hp/idle-state/real
    // round-intro-then-timer setup) is exactly what round 1 also needs,
    // so this class uses the one real code path for both, matching
    // startMatch()'s real OUTCOME exactly.
    match.ResetRound();
    return Result<HitmMatch>::Ok(std::move(match));
}

HitmMatch::HitmMatch(HitmGameRules rules, HitmFighterRuntime fighterA, HitmFighterRuntime fighterB)
    : rules_(std::move(rules)), fighterA_(std::move(fighterA)), fighterB_(std::move(fighterB)) {}

void HitmMatch::ResetRound() {
    fighterA_.ResetForNewRound(kMatchStartXFighterA, static_cast<float>(rules_.Physics().ground), kMatchStartFacingA);
    fighterB_.ResetForNewRound(kMatchStartXFighterB, static_cast<float>(rules_.Physics().ground), kMatchStartFacingB);
    phase_ = HitmMatchPhase::kRoundIntro;
    roundIntroFramesRemaining_ = kRoundIntroFrames;
    // Real: rounds.timer_seconds (already imported, Module 4) * the real
    // 60fps convention this whole codebase already uses
    // (COMBAT::kFramesPerSecond).
    timerFramesRemaining_ = static_cast<int>(rules_.Rounds().timer_seconds * combat::kFramesPerSecond);
    koFramesRemaining_ = 0;
}

void HitmMatch::EndRound() {
    // See this class's header comment for why roundLoserIndex_ (not a
    // re-inspection of fighter state) is this class's own source of
    // truth here -- it is guaranteed set by the only two places that
    // enter kKO, below.
    int loserIndex = *roundLoserIndex_;
    roundLoserIndex_.reset();
    int winnerIndex = (loserIndex == 0) ? 1 : 0;

    int winnerRounds = (winnerIndex == 0) ? ++fighterARoundsWon_ : ++fighterBRoundsWon_;

    if (winnerRounds >= static_cast<int>(rules_.Rounds().to_win)) {
        phase_ = HitmMatchPhase::kMatchOver;
        matchOver_ = true;
        matchWinnerIndex_ = winnerIndex;
        return;
    }
    ++round_;
    ResetRound();
}

void HitmMatch::ResolveAttack(HitmFighterRuntime& attacker, HitmFighterRuntime& defender) {
    // Real hit resolution happens exactly once per real attack
    // activation, on the real first active frame -- a direct match for
    // hitm-engine's own real `hitFrame` convention
    // (`engine/combat/FrameData.js:14`: "this.hitFrame = this.startup;
    // // first active frame", consumed by `CombatSystem.js:254`'s
    // `!f.hitDone && f.move.t===def.hitFrame`). No separate "already hit
    // this activation" flag is needed here: `state==kAttackActive &&
    // state_frame==0` is true for exactly one real frame per activation
    // (Module 5A's own `state_frame`, which resets to 0 on the very
    // transition into kAttackActive), the same real invariant the real
    // engine's own hitDone flag exists to enforce.
    if (attacker.State() != HitmFighterState::kAttackActive) return;
    auto attackerSnap = attacker.Snapshot();
    if (attackerSnap.state_frame != 0) return;

    // Real no-op for a fighter with no working special (Rocket, until
    // the audit's still-unauthorized Phase 4) -- see
    // HitmFighterRuntime.h's own "PHASE 3" comment.
    const HitmMoveInstance* move = attacker.SpecialMove();
    if (!move) return;

    auto defenderSnap = defender.Snapshot();
    if (!MeleeHitConnects(attackerSnap, *move, defenderSnap)) return;

    // Real: CombatSystem.js:410, `d.state===STATE.BLOCK && d.y>=ground`.
    bool defenderBlocking = defenderSnap.state == HitmFighterState::kBlockingStance && defenderSnap.grounded;
    defender.TakeHit(*move, defenderBlocking);

    if (!defenderBlocking) {
        // Real: the attacker gets meter on ANY landed hit, blocked or
        // not (CombatSystem.js:438, `blocking?onBlockGive:onHitGive`) --
        // but HitmFighterRuntime::ResolveOutgoingHitLanded (Module 5A,
        // unmodified by this phase) only ever applies the real
        // onHitGive amount, with no blocked-vs-unblocked branch of its
        // own. Calling it on a blocked hit would silently overstate the
        // attacker's real meter gain, so it is deliberately skipped here
        // instead: a real, documented gap (the attacker gets zero meter
        // for landing a blocked hit, not the real, smaller onBlockGive
        // amount), not a wrong number presented as correct.
        attacker.ResolveOutgoingHitLanded(*move);
    }
}

void HitmMatch::AdvanceFrame(HitmInputCommand inputA, HitmInputCommand inputB) {
    switch (phase_) {
        case HitmMatchPhase::kRoundIntro: {
            // Real: CombatSystem.js:54, `if(--s.roundT<=0){...} return;`
            // -- neither fighter ticks during the real intro window.
            if (--roundIntroFramesRemaining_ <= 0) {
                phase_ = HitmMatchPhase::kFight;
            }
            return;
        }
        case HitmMatchPhase::kFight: {
            auto snapABefore = fighterA_.Snapshot();
            auto snapBBefore = fighterB_.Snapshot();
            // Real continuous facing (CombatSystem.js:488,509):
            // auto-face the opponent every real frame, using each
            // fighter's position from the start of this frame.
            // SetFacing() itself already enforces the real "locked
            // during any attack sub-state" rule; skipped entirely while
            // either fighter is in real hitstop, matching the real
            // engine's own global hitstop freeze (which halts this
            // update too, not just movement).
            if (snapABefore.hitstop_frames_remaining == 0) {
                fighterA_.SetFacing(snapBBefore.x >= snapABefore.x ? 1 : -1);
            }
            if (snapBBefore.hitstop_frames_remaining == 0) {
                fighterB_.SetFacing(snapABefore.x >= snapBBefore.x ? 1 : -1);
            }

            fighterA_.AdvanceFrame(inputA);
            fighterB_.AdvanceFrame(inputB);

            // Real per-frame tick order (CombatSystem.js's own
            // `s.fighters.forEach`): fighter A's own action resolved
            // before fighter B's.
            ResolveAttack(fighterA_, fighterB_);
            ResolveAttack(fighterB_, fighterA_);

            // Real KO check and tie-break (CombatSystem.js:455,466):
            // fighter A's state is checked FIRST -- if A is KO'd, B is
            // the round winner, even in the (rare) case both fighters'
            // hp reached 0 on the same real frame. See this class's
            // header comment for why this is tracked as
            // roundLoserIndex_ rather than re-derived inside EndRound().
            if (fighterA_.State() == HitmFighterState::kKO || fighterB_.State() == HitmFighterState::kKO) {
                roundLoserIndex_ = (fighterA_.State() == HitmFighterState::kKO) ? 0 : 1;
                phase_ = HitmMatchPhase::kKO;
                koFramesRemaining_ = kKOFrames;
                return;
            }

            if (--timerFramesRemaining_ <= 0) {
                // Real sudden-death (CombatSystem.js:58-61): the higher
                // real hp/max_hp fraction wins; ties favor fighter A
                // (`a.hp/a.maxhp >= b.hp/b.maxhp ? a : b`).
                auto snapA = fighterA_.Snapshot();
                auto snapB = fighterB_.Snapshot();
                double fracA = static_cast<double>(snapA.hp) / static_cast<double>(snapA.max_hp);
                double fracB = static_cast<double>(snapB.hp) / static_cast<double>(snapB.max_hp);
                roundLoserIndex_ = (fracA >= fracB) ? 1 : 0;
                phase_ = HitmMatchPhase::kKO;
                koFramesRemaining_ = kKOFrames;
            }
            return;
        }
        case HitmMatchPhase::kKO: {
            // Real: CombatSystem.js:63-65 -- fighters keep ticking with
            // no real input (`{}`) so a KO'd fighter's own real
            // gravity/ground-clamp settling still runs, then the round
            // resolves once the real settle window elapses.
            fighterA_.AdvanceFrame(HitmInputCommand::kNeutral);
            fighterB_.AdvanceFrame(HitmInputCommand::kNeutral);
            if (--koFramesRemaining_ <= 0) {
                EndRound();
            }
            return;
        }
        case HitmMatchPhase::kMatchOver:
            // Real: update() has no case for 'matchOver' -- nothing
            // advances once the match is over, matching that omission
            // exactly (terminal, frozen).
            return;
    }
}

HitmMatchSnapshot HitmMatch::Snapshot() const {
    HitmMatchSnapshot snap;
    snap.phase = phase_;
    snap.round = round_;
    snap.round_intro_frames_remaining = roundIntroFramesRemaining_;
    snap.timer_frames_remaining = timerFramesRemaining_;
    snap.ko_frames_remaining = koFramesRemaining_;
    snap.fighter_a_rounds_won = fighterARoundsWon_;
    snap.fighter_b_rounds_won = fighterBRoundsWon_;
    snap.fighter_a = fighterA_.Snapshot();
    snap.fighter_b = fighterB_.Snapshot();
    snap.match_over = matchOver_;
    snap.match_winner_index = matchWinnerIndex_;
    return snap;
}

}  // namespace dominus::character::hitm
