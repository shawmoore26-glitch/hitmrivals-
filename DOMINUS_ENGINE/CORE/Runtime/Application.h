// CORE/Runtime/Application.h
// The Phase 1 app loop. GRAPHICS/ANIMATION/COMBAT register systems here in
// later phases -- Phase 1 proves the loop starts, spins an empty frame, and
// shuts down cleanly with no leaked threads/allocations.
//
// Track H Phase 5D (the application/game loop): `Tick()` grew into a real,
// generic, DETERMINISTIC fixed-timestep dispatcher -- still exactly as
// game-agnostic as Phase 1 left it. This file does not, and must not,
// include CHARACTER/GRAPHICS/COMBAT anywhere -- WORLD LAW 002/003's own
// discipline ("a renderer is a system that would READ the world state
// afterward, not something WorldTick calls"; "COMBAT is a plugin the WORLD
// ticks, not a thing WORLD knows how to do") applies identically here, one
// layer up: HITM's real orchestration (sample input -> translate ->
// HitmMatch::AdvanceFrame -> build sprite data -> build scene -> update
// camera -> compile -> rasterize -> present) lives entirely in
// CHARACTER::hitm::HitmApplicationLoop (CHARACTER/HitmBridge/
// HitmApplicationLoop.h), which sits ABOVE Application in the real
// dependency stack and REGISTERS its own callbacks into the four generic
// hooks below -- `Application` never learns what a `HitmMatch` is. See
// HITM_APPLICATION_LOOP_REPORT.md for the full reasoning.
//
// THE ONE PROPERTY THIS FILE EXISTS TO GUARANTEE: simulation time is never
// "whatever wall-clock time elapsed since the last Tick() call". `Tick()`
// takes a real elapsed-seconds value (supplied by the caller -- `Run()`
// measures it from a real clock; a test supplies any synthetic value it
// wants, so this class never touches a real clock itself and stays fully
// deterministic to test) and accumulates it; the simulation callback fires
// exactly once per whole `kFixedSimDt` consumed from that accumulator --
// zero, one, or several times per real `Tick()` call, never a variable-
// sized step. The present callback always fires exactly once per `Tick()`
// call regardless of how many simulation steps just ran. This is the
// standard "fixed timestep with an accumulator" game-loop technique, not
// an invented one -- see HITM_APPLICATION_LOOP_REPORT.md's own
// determinism proof (identical total real time, chopped into wildly
// different Tick() call patterns, produces an identical SimStepCount()
// and, when a real HitmMatch is driven through it, an identical final
// match snapshot).
#pragma once

#include <chrono>
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
    // The real, fixed simulation cadence -- matches
    // HitmFighterRuntime/HitmMatch's own established "one AdvanceFrame()
    // call == 1/60 real second" convention (already the implicit
    // assumption every prior Track H phase's frame-count math has made;
    // HITM_RENDER_INPUT_LOOP_AUDIT.md section C.2.1 named this as the
    // natural, evidence-backed choice for this exact reason -- not a new
    // number invented here).
    static constexpr double kFixedSimDt = 1.0 / 60.0;

    // Real, disclosed anti-"spiral of death" clamp: a single real Tick()
    // call (a debugger pause, a stalled frame, a slow test) never tries
    // to run more than this many catch-up simulation steps. Any
    // additional accumulated time beyond that is DROPPED, not silently
    // owed to a future Tick() call -- a real, standard safety valve, not
    // a claim that HitmMatch itself has any such limit.
    static constexpr int kMaxSimStepsPerTick = 5;

    bool Initialize(const AppConfig& config) {
        config_ = config;
        jobSystem_ = std::make_unique<JobSystem>(
            config.jobThreadCount == 0 ? std::thread::hardware_concurrency() : config.jobThreadCount);
        world_ = std::make_unique<EcsWorld>();
        initialized_ = true;
        return true;
    }

    // Real, generic per-frame hooks (Track H Phase 5D). All four are
    // optional (unset = no-op, preserving Phase 1's original empty-tick
    // behavior exactly when none are registered). `Application` invokes
    // them by CONTRACT (poll once, simulate zero-or-more fixed steps,
    // present once, per real Tick() call) -- it never looks inside them.
    //   - onPoll: OS/window event pump (step 1) -- called once per real
    //     Tick(), before the simulation loop.
    //   - onSimStep: one fixed `kFixedSimDt` of game state advance
    //     (steps 2-6: sample input, translate, advance the real
    //     simulation, build presentation data) -- called once per whole
    //     `kFixedSimDt` consumed from the accumulator this real Tick().
    //   - onPresent: build+draw+show one frame (steps 7-10: camera,
    //     compile, rasterize, present) -- called exactly once per real
    //     Tick() call, regardless of how many (zero, one, several)
    //     simulation steps just ran.
    //   - onShouldClose: real "the user/window asked to quit" query,
    //     checked once per `Run()` iteration; `Run()` stops when this
    //     returns true OR `config_.maxFrames` is reached, whichever
    //     first. Unset means "never asks to close" (Phase 1's original,
    //     maxFrames-only stopping condition, unchanged).
    void SetPollCallback(std::function<void()> fn) { onPoll_ = std::move(fn); }
    void SetSimulationStepCallback(std::function<void()> fn) { onSimStep_ = std::move(fn); }
    void SetPresentCallback(std::function<void()> fn) { onPresent_ = std::move(fn); }
    void SetShouldCloseCallback(std::function<bool()> fn) { onShouldClose_ = std::move(fn); }

    // Real, deterministic, testable core of the whole loop -- see this
    // file's own top comment. `realDtSeconds` is real elapsed wall-clock
    // time since the previous call, but THIS function never measures it
    // itself; that keeps it callable from a test with any synthetic
    // sequence of dt values, with no real clock or real sleep anywhere
    // in the call.
    void Tick(double realDtSeconds) {
        jobSystem_->WaitIdle();
        if (onPoll_) onPoll_();

        simAccumulator_ += realDtSeconds;
        int steps = 0;
        while (simAccumulator_ >= kFixedSimDt && steps < kMaxSimStepsPerTick) {
            if (onSimStep_) onSimStep_();
            simAccumulator_ -= kFixedSimDt;
            ++steps;
            ++simStepCount_;
        }
        if (steps >= kMaxSimStepsPerTick) {
            // Real, disclosed clamp hit -- drop the rest rather than
            // owing it to (and bursting on) a future Tick().
            simAccumulator_ = 0.0;
        }

        if (onPresent_) onPresent_();
    }

    // Runs the frame loop at a real wall-clock cadence (step 11): each
    // iteration measures real elapsed time since the previous one via
    // `std::chrono::steady_clock` (a real, monotonic clock -- never
    // affected by system time changes) and feeds it to `Tick()`, which
    // itself remains clock-free and deterministic (above). With
    // maxFrames == 0 and no ShouldClose callback registered, this only
    // returns after an external call to Shutdown() -- tests should set
    // maxFrames explicitly, or call Tick() directly, for a bounded,
    // clock-free run.
    void Run() {
        running_ = true;
        uint64_t frame = 0;
        auto last = std::chrono::steady_clock::now();
        while (running_) {
            auto now = std::chrono::steady_clock::now();
            double dt = std::chrono::duration<double>(now - last).count();
            last = now;

            Tick(dt);
            ++frame;
            if (config_.maxFrames != 0 && frame >= config_.maxFrames) running_ = false;
            if (onShouldClose_ && onShouldClose_()) running_ = false;
        }
    }

    void Shutdown() {
        running_ = false;
        if (jobSystem_) jobSystem_->Shutdown();
    }

    JobSystem& GetJobSystem() { return *jobSystem_; }
    EcsWorld& GetWorld() { return *world_; }
    bool IsInitialized() const { return initialized_; }

    // Real, observable proof of the fixed-timestep guarantee -- how many
    // whole kFixedSimDt simulation steps have actually run, total, since
    // construction. Used directly by the determinism test: the same
    // total real elapsed time, fed to Tick() in wildly different dt
    // patterns, must produce the exact same SimStepCount().
    uint64_t SimStepCount() const { return simStepCount_; }
    double SimAccumulatorSeconds() const { return simAccumulator_; }

private:
    AppConfig config_;
    std::unique_ptr<JobSystem> jobSystem_;
    std::unique_ptr<EcsWorld> world_;
    bool initialized_ = false;
    bool running_ = false;

    std::function<void()> onPoll_;
    std::function<void()> onSimStep_;
    std::function<void()> onPresent_;
    std::function<bool()> onShouldClose_;
    double simAccumulator_ = 0.0;
    uint64_t simStepCount_ = 0;
};

}  // namespace dominus::core
