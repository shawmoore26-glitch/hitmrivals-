// REALITY/FileLock.cpp
#include "REALITY/FileLock.h"

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

#include <thread>

namespace dominus::reality {

FileLock::FileLock(std::filesystem::path lockFilePath) : path_(std::move(lockFilePath)) {}

FileLock::~FileLock() { Release(); }

bool FileLock::TryAcquire() {
    if (held_) return true;

    int fd = ::open(path_.c_str(), O_CREAT | O_RDWR, 0644);
    if (fd < 0) return false;

    // LOCK_EX | LOCK_NB: real OS-level exclusive lock, non-blocking.
    // flock() (not fcntl() byte-range locks) chosen specifically
    // because its lock lifetime is tied to the open file description,
    // not the process -- a crashed holder's lock is released by the
    // kernel automatically when its descriptor closes, which is the
    // exact property that keeps a crash from ever deadlocking a future
    // caller.
    if (::flock(fd, LOCK_EX | LOCK_NB) != 0) {
        ::close(fd);
        return false;
    }

    fd_ = fd;
    held_ = true;
    return true;
}

bool FileLock::Acquire(std::chrono::milliseconds timeout) {
    if (held_) return true;

    auto deadline = std::chrono::steady_clock::now() + timeout;
    // A real, bounded polling loop -- flock() itself has no portable
    // timed-wait variant, so contention is detected by retrying
    // TryAcquire() until the deadline. The poll interval is short
    // enough that lock hand-off between contending callers in this
    // codebase's own tests (which hold the lock only for the duration
    // of one real rebuild pass) is picked up promptly, not after a
    // multi-second stall.
    while (std::chrono::steady_clock::now() < deadline) {
        if (TryAcquire()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return TryAcquire();  // one last real attempt exactly at the deadline
}

void FileLock::Release() {
    if (!held_) return;
    ::flock(fd_, LOCK_UN);
    ::close(fd_);
    fd_ = -1;
    held_ = false;
}

}  // namespace dominus::reality
