// tests/core/test_application.cpp
// Covers Phase 1 exit criterion 1: Application::Initialize/Run/Shutdown
// compiles and runs a bounded frame loop with the job system spinning up
// and down cleanly.
//
// Track H Phase 5D: the fixed-timestep dispatcher tests below are
// deliberately generic -- no CHARACTER/GRAPHICS/HITM type appears
// anywhere in this file, matching Application.h's own architectural
// boundary. The HITM-specific version of the same determinism proof
// (does a real HitmMatch end up in an identical state regardless of how
// the same total real time was chopped into Tick() calls) lives in
// tests/integration/test_hitm_application_loop.cpp.
#include "CORE/Runtime/Application.h"
#include "tests/TestFramework.h"

using dominus::core::AppConfig;
using dominus::core::Application;

DOMINUS_TEST(Application_InitializeRunShutdown_BoundedLoop) {
    Application app;
    AppConfig config;
    config.jobThreadCount = 2;
    config.maxFrames = 5;  // bounded so the test terminates deterministically

    DOMINUS_EXPECT(app.Initialize(config));
    DOMINUS_EXPECT(app.IsInitialized());

    app.Run();  // returns after 5 ticks because maxFrames is set

    DOMINUS_EXPECT(app.GetJobSystem().WorkerCount() == 2);
    DOMINUS_EXPECT(app.GetWorld().ObjectCount() == 0);

    app.Shutdown();  // must not throw or hang
}

DOMINUS_TEST(Application_JobSystem_ExecutesSubmittedJobs) {
    Application app;
    AppConfig config;
    config.jobThreadCount = 4;
    config.maxFrames = 1;
    app.Initialize(config);

    std::atomic<int> counter{0};
    for (int i = 0; i < 100; ++i) {
        app.GetJobSystem().Submit([&counter] { ++counter; });
    }
    app.GetJobSystem().WaitIdle();

    DOMINUS_EXPECT(counter.load() == 100);
    app.Shutdown();
}

// --- Track H Phase 5D: the fixed-timestep dispatcher ----------------------

namespace {
Application MakeReadyApp() {
    Application app;
    AppConfig config;
    config.jobThreadCount = 1;
    app.Initialize(config);
    return app;
}
}  // namespace

DOMINUS_TEST(Application_Tick_ExactlyOneFixedStepWorthOfDt_RunsSimStepExactlyOnce) {
    Application app = MakeReadyApp();
    app.Tick(Application::kFixedSimDt);
    DOMINUS_EXPECT(app.SimStepCount() == 1);
    app.Shutdown();
}

DOMINUS_TEST(Application_Tick_LessThanOneFixedStepWorthOfDt_DoesNotAdvanceSimulationYet) {
    Application app = MakeReadyApp();
    app.Tick(Application::kFixedSimDt / 2.0);
    DOMINUS_EXPECT(app.SimStepCount() == 0);
    DOMINUS_EXPECT(app.SimAccumulatorSeconds() > 0.0);
    app.Shutdown();
}

DOMINUS_TEST(Application_Tick_SeveralFixedStepsWorthOfDt_RunsThatManySimSteps) {
    Application app = MakeReadyApp();
    app.Tick(Application::kFixedSimDt * 3.0);
    DOMINUS_EXPECT(app.SimStepCount() == 3);
    app.Shutdown();
}

DOMINUS_TEST(Application_Tick_AccumulatesAcrossCalls_UntilAWholeStepIsReached) {
    Application app = MakeReadyApp();
    app.Tick(Application::kFixedSimDt * 0.4);
    DOMINUS_EXPECT(app.SimStepCount() == 0);
    app.Tick(Application::kFixedSimDt * 0.4);
    DOMINUS_EXPECT(app.SimStepCount() == 0);
    app.Tick(Application::kFixedSimDt * 0.4);  // 1.2 fixed steps accumulated total
    DOMINUS_EXPECT(app.SimStepCount() == 1);
    app.Shutdown();
}

DOMINUS_TEST(Application_Tick_ExcessivelyLargeDt_ClampsToMaxSimStepsPerTick_NotUnboundedCatchUp) {
    Application app = MakeReadyApp();
    app.Tick(1000.0);  // a real stalled-frame/debugger-pause scenario
    DOMINUS_EXPECT(app.SimStepCount() == static_cast<uint64_t>(Application::kMaxSimStepsPerTick));
    // The excess is dropped, not owed to the next Tick() -- a second,
    // normal-sized Tick() advances by exactly one more step, not a
    // burst of queued catch-up steps.
    app.Tick(Application::kFixedSimDt);
    DOMINUS_EXPECT(app.SimStepCount() == static_cast<uint64_t>(Application::kMaxSimStepsPerTick) + 1);
    app.Shutdown();
}

DOMINUS_TEST(Application_Tick_PresentCallbackRunsExactlyOnceRegardlessOfSimStepCount) {
    Application app = MakeReadyApp();
    int presentCount = 0;
    app.SetPresentCallback([&] { presentCount++; });

    app.Tick(Application::kFixedSimDt / 2.0);  // zero sim steps this call
    DOMINUS_EXPECT(presentCount == 1);
    app.Tick(Application::kFixedSimDt * 4.0);  // several sim steps this call
    DOMINUS_EXPECT(presentCount == 2);

    app.Shutdown();
}

DOMINUS_TEST(Application_Tick_PollCallbackRunsExactlyOncePerTick) {
    Application app = MakeReadyApp();
    int pollCount = 0;
    app.SetPollCallback([&] { pollCount++; });
    app.Tick(Application::kFixedSimDt);
    app.Tick(Application::kFixedSimDt);
    DOMINUS_EXPECT(pollCount == 2);
    app.Shutdown();
}

DOMINUS_TEST(Application_Run_StopsWhenShouldCloseCallbackReturnsTrue) {
    Application app = MakeReadyApp();  // maxFrames left at 0 (unbounded)
    int tickCount = 0;
    app.SetPresentCallback([&] { tickCount++; });
    app.SetShouldCloseCallback([&] { return tickCount >= 3; });

    app.Run();  // would spin forever on maxFrames alone; ShouldClose stops it

    DOMINUS_EXPECT(tickCount == 3);
    app.Shutdown();
}

// THE determinism guarantee this phase exists to prove: the exact same
// total real elapsed time, fed to Tick() as wildly different dt patterns
// (one huge call, many tiny calls, an irregular/jittery sequence), always
// produces the exact same number of real simulation steps. Simulation
// time is never "whatever time elapsed since the last render" -- it is
// always a whole multiple of the real, fixed kFixedSimDt.
DOMINUS_TEST(Application_Tick_TotalSimStepsAreIndependentOfHowRealTimeWasChopped) {
    // Deliberately 60.5 steps' worth, not an exact multiple of
    // kFixedSimDt: landing exactly ON a step boundary is the one case
    // where IEEE-754 addition's non-associativity can legitimately tip
    // the final partial step's accumulated rounding error either side of
    // the threshold depending on summation order (a real, well-known
    // property of ANY accumulator-based fixed timestep, not a defect --
    // the same "genuine float-accumulation drift, not a bug" class of
    // finding Track H Phase 3's own HitmMatch tests already documented).
    // A real wall clock effectively never lands on an exact multiple of
    // 1/60s anyway. Landing solidly mid-step (a comfortable half-step
    // margin either side) is what this test asserts must be safe and
    // deterministic regardless of chopping pattern -- and empirically is.
    const double totalRealSeconds = 1.0 + Application::kFixedSimDt / 2.0;

    // Every per-call dt below is kept well under
    // kMaxSimStepsPerTick*kFixedSimDt (~0.083s) deliberately -- this test
    // proves determinism under different real chopping patterns, not the
    // separate, already-covered anti-catch-up clamp; conflating the two
    // would make this test's own result ambiguous.
    Application coarseTicks = MakeReadyApp();
    for (int i = 0; i < 20; ++i) coarseTicks.Tick(totalRealSeconds / 20.0);  // 0.05s/call, 3 steps/call

    Application fineTicks = MakeReadyApp();
    for (int i = 0; i < 100; ++i) fineTicks.Tick(totalRealSeconds / 100.0);  // 0.01s/call

    Application jitteryTicks = MakeReadyApp();
    // An irregular, jitter-like real frame-time pattern (fast/slow/fast/
    // slow...) that still sums to exactly totalRealSeconds.
    double remaining = totalRealSeconds;
    bool fast = true;
    while (remaining > 1e-9) {
        double dt = fast ? 0.004 : 0.021;
        if (dt > remaining) dt = remaining;
        jitteryTicks.Tick(dt);
        remaining -= dt;
        fast = !fast;
    }

    DOMINUS_EXPECT(coarseTicks.SimStepCount() == fineTicks.SimStepCount());
    DOMINUS_EXPECT(coarseTicks.SimStepCount() == jitteryTicks.SimStepCount());
    DOMINUS_EXPECT(coarseTicks.SimStepCount() == 60);  // 1.0s / (1/60s) == 60 whole steps

    coarseTicks.Shutdown();
    fineTicks.Shutdown();
    jitteryTicks.Shutdown();
}
