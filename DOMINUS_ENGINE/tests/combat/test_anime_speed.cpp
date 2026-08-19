// tests/combat/test_anime_speed.cpp
#include "COMBAT/AnimeSpeedSystem.h"
#include "tests/TestFramework.h"

#include <cmath>

using dominus::combat::AfterimageTrail;
using dominus::combat::SpeedSimulator;
using dominus::combat::SpeedState;

namespace {
bool NearlyEqual(float a, float b, float eps = 0.01f) { return std::fabs(a - b) < eps; }
}  // namespace

DOMINUS_TEST(SpeedSimulator_AccelerationIncreasesVelocity) {
    SpeedState state;
    SpeedSimulator::ApplyAcceleration(state, 100.0f, 0.0f, 0.1f);
    DOMINUS_EXPECT(NearlyEqual(state.velocity_x, 10.0f));
}

DOMINUS_TEST(SpeedSimulator_ClampsToMaxSpeed) {
    SpeedState state;
    state.max_speed = 50.0f;
    SpeedSimulator::ApplyAcceleration(state, 10000.0f, 0.0f, 1.0f);
    float speed = std::sqrt(state.velocity_x * state.velocity_x + state.velocity_y * state.velocity_y);
    DOMINUS_EXPECT(NearlyEqual(speed, 50.0f, 0.1f));
}

DOMINUS_TEST(SpeedSimulator_IntegratesPositionFromVelocity) {
    SpeedState state;
    state.velocity_x = 20.0f;
    state.velocity_y = 0.0f;
    float x = 0.0f, y = 0.0f;
    SpeedSimulator::Integrate(state, x, y, 0.5f);
    DOMINUS_EXPECT(NearlyEqual(x, 10.0f));
    DOMINUS_EXPECT(NearlyEqual(y, 0.0f));
}

DOMINUS_TEST(AfterimageTrail_RetainsRecentSamplesWithinMaxAge) {
    AfterimageTrail trail(0.3f, 32);
    trail.PushSample(0.0f, 0.0f, 0.0f, 0.0f);
    trail.PushSample(1.0f, 0.0f, 0.0f, 0.1f);
    trail.PushSample(2.0f, 0.0f, 0.0f, 0.2f);
    DOMINUS_EXPECT(trail.Count() == 3);
}

DOMINUS_TEST(AfterimageTrail_PrunesSamplesOlderThanMaxAge) {
    AfterimageTrail trail(0.3f, 32);
    trail.PushSample(0.0f, 0.0f, 0.0f, 0.0f);
    trail.PushSample(1.0f, 0.0f, 0.0f, 0.1f);
    // Pushing at t=0.5 prunes anything older than 0.3s: t=0.0 (age 0.5) AND
    // t=0.1 (age 0.4) both exceed maxAge, leaving only this new sample.
    trail.PushSample(2.0f, 0.0f, 0.0f, 0.5f);
    DOMINUS_EXPECT(trail.Count() == 1);
}

DOMINUS_TEST(AfterimageTrail_CapsAtMaxSampleCount) {
    AfterimageTrail trail(1000.0f, 3);  // huge max age, tiny sample cap
    for (int i = 0; i < 10; ++i) trail.PushSample(static_cast<float>(i), 0.0f, 0.0f, static_cast<float>(i) * 0.01f);
    DOMINUS_EXPECT(trail.Count() == 3);
    // Should have kept the MOST RECENT 3, not the first 3.
    DOMINUS_EXPECT(NearlyEqual(trail.Samples().back().x, 9.0f));
}
