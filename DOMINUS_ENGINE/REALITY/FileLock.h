// REALITY/FileLock.h
// Milestone 9: Concurrent Registry Safety. A real, OS-level exclusive
// lock (POSIX `flock()`) -- not an in-process mutex pretending to
// stand in for cross-process safety. `flock()` locks are associated
// with the OPEN FILE DESCRIPTION, not the process, so two threads in
// the same process that each `open()` their own file descriptor for
// the same path genuinely contend for the same OS-level lock exactly
// the way two separate processes would. This is what makes it
// possible to test real concurrency safety with `std::thread` instead
// of spawning actual subprocesses.
#pragma once

#include <chrono>
#include <filesystem>
#include <string>

namespace dominus::reality {

class FileLock {
public:
    explicit FileLock(std::filesystem::path lockFilePath);
    ~FileLock();

    FileLock(const FileLock&) = delete;
    FileLock& operator=(const FileLock&) = delete;

    // Blocks until the exclusive lock is acquired or `timeout` elapses.
    // Returns true iff the lock was acquired. A real, bounded wait --
    // never blocks forever, so an abandoned or slow-holding lock
    // produces an honest, reportable failure instead of a hang.
    bool Acquire(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));

    // Non-blocking: returns immediately, true iff the lock was free and
    // is now held by this FileLock.
    bool TryAcquire();

    // Releases the lock if held. Also happens automatically in the
    // destructor -- and, as a real OS-level guarantee independent of
    // this class, the lock is released by the kernel if the holding
    // process crashes or exits without calling this at all (closing
    // the file descriptor releases a flock lock), so an abandoned lock
    // from a crashed process can never deadlock a future caller.
    void Release();

    bool IsHeld() const { return held_; }

private:
    std::filesystem::path path_;
    int fd_ = -1;
    bool held_ = false;
};

}  // namespace dominus::reality
