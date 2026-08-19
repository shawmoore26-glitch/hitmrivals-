// CORE/Runtime/JobSystem.h
// Minimal thread-pool job system. Phase 1 scope: submit/wait only. Priority
// lanes, job graphs/dependencies, and work-stealing are Phase 2+ additions
// once GRAPHICS/ANIMATION have real parallel workloads to schedule.
#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace dominus::core {

class JobSystem {
public:
    explicit JobSystem(unsigned threadCount = std::thread::hardware_concurrency())
        : stopping_(false) {
        if (threadCount == 0) threadCount = 1;
        for (unsigned i = 0; i < threadCount; ++i) {
            workers_.emplace_back([this] { WorkerLoop(); });
        }
    }

    ~JobSystem() { Shutdown(); }

    void Submit(std::function<void()> job) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(std::move(job));
            ++pending_;
        }
        cv_.notify_one();
    }

    // Blocks until every submitted job has completed.
    void WaitIdle() {
        std::unique_lock<std::mutex> lock(mutex_);
        idleCv_.wait(lock, [this] { return pending_.load() == 0; });
    }

    void Shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_) return;
            stopping_ = true;
        }
        cv_.notify_all();
        for (auto& t : workers_) {
            if (t.joinable()) t.join();
        }
    }

    size_t WorkerCount() const { return workers_.size(); }

private:
    void WorkerLoop() {
        while (true) {
            std::function<void()> job;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
                if (stopping_ && queue_.empty()) return;
                job = std::move(queue_.front());
                queue_.pop();
            }
            job();
            if (--pending_ == 0) idleCv_.notify_all();
        }
    }

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::condition_variable idleCv_;
    std::atomic<size_t> pending_{0};
    bool stopping_;
};

}  // namespace dominus::core
