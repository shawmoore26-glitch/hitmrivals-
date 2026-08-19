// tests/core/test_application.cpp
// Covers Phase 1 exit criterion 1: Application::Initialize/Run/Shutdown
// compiles and runs a bounded frame loop with the job system spinning up
// and down cleanly.
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
