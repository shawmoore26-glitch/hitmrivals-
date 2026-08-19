// CORE/Runtime/Application.h
// The Phase 1 app loop. GRAPHICS/ANIMATION/COMBAT register systems here in
// later phases -- Phase 1 proves the loop starts, spins an empty frame, and
// shuts down cleanly with no leaked threads/allocations.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

#include "CORE/Runtime/EcsWorld.h"
#include "CORE/Runtime/JobSystem.h"

namespace dominus::core {

struct AppConfig {
    unsigned jobThreadCount = 0;  // 0 = hardware_concurrency
    uint64_t maxFrames = 0;       // 0 = run until Shutdown() is called externally
};

class Application {
public:
    bool Initialize(const AppConfig& config) {
        config_ = config;
        jobSystem_ = std::make_unique<JobSystem>(
            config.jobThreadCount == 0 ? std::thread::hardware_concurrency() : config.jobThreadCount);
        world_ = std::make_unique<EcsWorld>();
        initialized_ = true;
        return true;
    }

    // Runs the frame loop. With maxFrames == 0 this only returns after an
    // external call to Shutdown() sets running_ to false -- intended for a
    // real app; tests should set maxFrames explicitly for a bounded run.
    void Run() {
        running_ = true;
        uint64_t frame = 0;
        while (running_) {
            Tick();
            ++frame;
            if (config_.maxFrames != 0 && frame >= config_.maxFrames) {
                running_ = false;
            }
        }
    }

    void Shutdown() {
        running_ = false;
        if (jobSystem_) jobSystem_->Shutdown();
    }

    JobSystem& GetJobSystem() { return *jobSystem_; }
    EcsWorld& GetWorld() { return *world_; }
    bool IsInitialized() const { return initialized_; }

private:
    void Tick() {
        // Phase 1: empty tick. Systems attach here starting Phase 2.
        jobSystem_->WaitIdle();
    }

    AppConfig config_;
    std::unique_ptr<JobSystem> jobSystem_;
    std::unique_ptr<EcsWorld> world_;
    bool initialized_ = false;
    bool running_ = false;
};

}  // namespace dominus::core
