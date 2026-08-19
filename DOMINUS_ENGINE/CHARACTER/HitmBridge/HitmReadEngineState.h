// CHARACTER/HitmBridge/HitmReadEngineState.h
// ROADMAP.md Track H Module 5A. A real, transitionable state machine over
// Brooklyn's authored read-engine mechanic (CHARACTER/HitmBridge/
// HitmCombatGenome.h's ReadEngine, imported losslessly by Module 2).
//
// Implements exactly what the real data's own "_note"/"_law"/"_lose_law"
// fields describe: reads increment on GainRead() up to the real
// `max_reads`; decrement on LoseRead() down to 0 ("he is punished for
// being read back"); decay by the real `decay.amount` every real
// `decay.frames` frames with no gain ("stand still and the knowledge
// goes stale"); `CurrentDamageMultiplier()` looks up the real per-tier
// `damage_mult` this reads count actually earned -- nothing here invents
// a trigger condition (what counts as a "counter hit"/"whiff punish" is
// real authored data, but recognizing one in a live match requires an
// opponent/hit-classification model this module does not build --
// GainRead()/LoseRead() are called explicitly by whatever proves it
// happened, matching the honest scope documented in
// HITM_FIGHTER_RUNTIME_REPORT.md).
#pragma once

#include <string>

#include "CHARACTER/HitmBridge/HitmCombatGenome.h"

namespace dominus::character::hitm {

class HitmReadEngineState {
public:
    explicit HitmReadEngineState(ReadEngine engine) : engine_(std::move(engine)) {}

    int CurrentReads() const { return current_reads_; }
    int MaxReads() const { return static_cast<int>(engine_.max_reads); }
    int FramesSinceLastGain() const { return frames_since_gain_; }
    int DecayFrames() const { return static_cast<int>(engine_.decay.frames); }

    // The real, authored tier this reads count actually earned.
    double CurrentDamageMultiplier() const { return engine_.tiers[static_cast<size_t>(current_reads_)].damage_mult; }
    const std::string& CurrentTierName() const { return engine_.tiers[static_cast<size_t>(current_reads_)].name; }

    // Capped at the real max_reads -- gaining at the cap is a real no-op,
    // not an error (matching "at 0 reads Brooklyn is the weakest fighter
    // ... the governor still audits him at 0" -- the boundary is a real,
    // intended state, not a fault).
    void GainRead() {
        if (current_reads_ < MaxReads()) ++current_reads_;
        frames_since_gain_ = 0;
    }

    // Floored at 0, same reasoning as GainRead's cap.
    void LoseRead() {
        if (current_reads_ > 0) --current_reads_;
        frames_since_gain_ = 0;
    }

    // Advances the decay clock by one real HITM frame. When
    // frames_since_gain_ reaches the real decay.frames threshold, applies
    // the real decay.amount and restarts the countdown -- repeatable, so
    // standing still for multiple decay windows genuinely drains reads to
    // 0 rather than decaying once and stopping.
    void TickFrame() {
        ++frames_since_gain_;
        if (frames_since_gain_ >= DecayFrames() && DecayFrames() > 0) {
            int amount = static_cast<int>(engine_.decay.amount);
            current_reads_ = current_reads_ > amount ? current_reads_ - amount : 0;
            frames_since_gain_ = 0;
        }
    }

private:
    ReadEngine engine_;
    int current_reads_ = 0;
    int frames_since_gain_ = 0;
};

}  // namespace dominus::character::hitm
