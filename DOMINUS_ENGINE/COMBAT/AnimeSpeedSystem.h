// COMBAT/AnimeSpeedSystem.h
// LAW C009: anime-speed movement is simulated (acceleration/velocity/
// momentum), not faked as instant teleportation. This is foundation
// scope: a real 2D velocity integrator plus a bounded-history afterimage
// trail buffer for the visual effect LAW C009 calls for. Dash/teleport as
// distinct move types, camera effects, and environmental reaction are
// COMBAT-move-data and rendering concerns layered on top of this, not
// implemented here.
#pragma once

#include <cmath>
#include <deque>

namespace dominus::combat {

struct SpeedState {
    float velocity_x = 0.0f;
    float velocity_y = 0.0f;
    float max_speed = 400.0f;
};

class SpeedSimulator {
public:
    // Applies a burst of acceleration for one frame (a "dash"/"teleport
    // windup" is just a large acceleration impulse over a short time, not
    // a special-cased instant position set), then integrates position.
    static void ApplyAcceleration(SpeedState& state, float accel_x, float accel_y, float dt) {
        state.velocity_x += accel_x * dt;
        state.velocity_y += accel_y * dt;

        float speed = Magnitude(state.velocity_x, state.velocity_y);
        if (speed > state.max_speed && speed > 0.0f) {
            float scale = state.max_speed / speed;
            state.velocity_x *= scale;
            state.velocity_y *= scale;
        }
    }

    static void Integrate(const SpeedState& state, float& pos_x, float& pos_y, float dt) {
        pos_x += state.velocity_x * dt;
        pos_y += state.velocity_y * dt;
    }

private:
    static float Magnitude(float x, float y) { return std::sqrt(x * x + y * y); }
};

struct AfterimageSample {
    float x = 0.0f;
    float y = 0.0f;
    float rotation_deg = 0.0f;
    float timestamp = 0.0f;
};

// Bounded-history ring buffer of position snapshots -- what a renderer
// would draw fading afterimage silhouettes from. Sampling policy (how
// often to push) is the caller's call; this just retains and prunes.
class AfterimageTrail {
public:
    explicit AfterimageTrail(float maxAgeSeconds = 0.3f, size_t maxSamples = 32)
        : maxAge_(maxAgeSeconds), maxSamples_(maxSamples) {}

    void PushSample(float x, float y, float rotation_deg, float timestamp) {
        samples_.push_back(AfterimageSample{x, y, rotation_deg, timestamp});
        Prune(timestamp);
    }

    void Prune(float currentTime) {
        while (!samples_.empty() && (currentTime - samples_.front().timestamp) > maxAge_) {
            samples_.pop_front();
        }
        while (samples_.size() > maxSamples_) {
            samples_.pop_front();
        }
    }

    const std::deque<AfterimageSample>& Samples() const { return samples_; }
    size_t Count() const { return samples_.size(); }

private:
    std::deque<AfterimageSample> samples_;
    float maxAge_;
    size_t maxSamples_;
};

}  // namespace dominus::combat
