// REALITY/RealityWatcher.cpp
#include "REALITY/RealityWatcher.h"

#include <fcntl.h>
#include <poll.h>
#include <sys/inotify.h>
#include <unistd.h>

#include <cstring>
#include <stdexcept>

namespace dominus::reality {

namespace {

RawEventKind TranslateMask(uint32_t mask) {
    // The ONLY classification this file performs: an inotify bitmask
    // to one of RawFileEvent's four kinds. This is NOT artifact
    // classification (is this file authoritative?) or dependency
    // analysis (what does this affect?) -- both of those already live
    // downstream, in ChangeEventNormalizer and RealityRebuilder.
    if (mask & IN_CLOSE_WRITE) return RawEventKind::kModified;
    if (mask & IN_CREATE) return RawEventKind::kCreated;
    if (mask & (IN_DELETE | IN_MOVED_FROM)) return RawEventKind::kDeleted;
    if (mask & IN_MOVED_TO) return RawEventKind::kRenamed;
    return RawEventKind::kUnknown;
}

}  // namespace

RealityWatcher::RealityWatcher(std::filesystem::path watchDir, std::filesystem::path registryPath)
    : watchDir_(std::move(watchDir)), registryPath_(std::move(registryPath)) {
    inotifyFd_ = ::inotify_init1(IN_NONBLOCK);
    if (inotifyFd_ < 0) {
        throw std::runtime_error(std::string("RealityWatcher: inotify_init1 failed: ") + std::strerror(errno));
    }

    // IN_CLOSE_WRITE (not IN_MODIFY): fires once when a writer actually
    // closes the file after writing, which is what "a save happened"
    // really means -- IN_MODIFY fires once per write() syscall and
    // would make even a single editor save look like a burst before
    // any normalization even gets involved. IN_CREATE/IN_DELETE/
    // IN_MOVED_TO/IN_MOVED_FROM cover the rename/create/delete
    // sequences real editors and temp files produce -- exactly the
    // raw noise ChangeEventNormalizer (Milestone 10) already proved it
    // can canonicalize, dedupe, and verify correctly.
    watchDescriptor_ = ::inotify_add_watch(inotifyFd_, watchDir_.c_str(),
                                            IN_CLOSE_WRITE | IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM);
    if (watchDescriptor_ < 0) {
        std::string err = std::strerror(errno);
        ::close(inotifyFd_);
        throw std::runtime_error("RealityWatcher: inotify_add_watch failed for '" + watchDir_.string() +
                                  "': " + err);
    }

    int pipeFds[2];
    if (::pipe(pipeFds) != 0) {
        std::string err = std::strerror(errno);
        ::close(inotifyFd_);
        throw std::runtime_error(std::string("RealityWatcher: pipe() failed: ") + err);
    }
    stopPipeRead_ = pipeFds[0];
    stopPipeWrite_ = pipeFds[1];
}

RealityWatcher::~RealityWatcher() {
    if (inotifyFd_ >= 0) ::close(inotifyFd_);
    if (stopPipeRead_ >= 0) ::close(stopPipeRead_);
    if (stopPipeWrite_ >= 0) ::close(stopPipeWrite_);
}

void RealityWatcher::Run(bool reconcileOnStart) {
    if (reconcileOnStart && !stopRequested_.load()) {
        // Milestone 12: the first thing a (re)started watcher does is
        // ask "what changed while I wasn't watching" -- a full scan,
        // via the exact same normalization and rebuild pipeline every
        // live event already goes through. Not a separate code path,
        // not a separate kind of result -- these reports land in
        // History() exactly like a live event's would. Skipped
        // entirely if Stop() was already called before Run() even
        // started -- a caller that wants to stop immediately shouldn't
        // have to wait through a full reconciliation pass first.
        auto reconciliationReports = Reconciler::Reconcile(watchDir_, registryPath_);
        for (auto& report : reconciliationReports) history_.push_back(std::move(report));
    }

    struct pollfd fds[2];
    fds[0].fd = inotifyFd_;
    fds[0].events = POLLIN;
    fds[1].fd = stopPipeRead_;
    fds[1].events = POLLIN;

    while (!stopRequested_.load()) {
        fds[0].revents = 0;
        fds[1].revents = 0;
        // No timeout policy of its own: blocks until there is
        // something real to do, or Stop() writes to the pipe. No
        // periodic polling, no busy loop.
        int ready = ::poll(fds, 2, -1);
        if (ready < 0) {
            if (errno == EINTR) continue;  // a real, retriable interruption -- not an error condition
            break;                          // a genuine, unrecoverable poll() failure -- stop rather than spin
        }

        if (fds[1].revents & POLLIN) break;  // Stop() was called
        if (fds[0].revents & POLLIN) HandleReadyEvents();
    }
}

void RealityWatcher::Stop() {
    stopRequested_.store(true);
    char byte = 1;
    // Best-effort wake-up write -- if this fails (e.g. the watcher was
    // never Run() and nothing is polling yet), the destructor's
    // pipe/fd cleanup still leaves nothing dangling; Stop() before
    // Run() simply means Run() will see stopRequested_ already true
    // and return immediately on its first check.
    ssize_t written = ::write(stopPipeWrite_, &byte, 1);
    (void)written;
}

void RealityWatcher::HandleReadyEvents() {
    // Drains whatever the kernel has already batched into this one
    // read() -- the entirety of this class's "batching policy." A
    // 4096-byte buffer comfortably holds many real events (each is
    // sizeof(inotify_event) + up to NAME_MAX+1 bytes for the name);
    // if genuinely more arrived than fit in one read(), the next
    // poll() iteration picks up the rest -- ChangeEventNormalizer
    // handles that as just another, still-correct batch.
    char buffer[4096];
    ssize_t length = ::read(inotifyFd_, buffer, sizeof(buffer));
    if (length <= 0) return;  // EAGAIN (non-blocking, nothing left) or a real read error -- either way, nothing to do

    std::vector<RawFileEvent> events;
    ssize_t offset = 0;
    while (offset < length) {
        const auto* event = reinterpret_cast<const struct inotify_event*>(buffer + offset);
        if (event->len > 0) {
            // The ONLY path-construction logic here: join the watched
            // directory with the filename inotify reported. No
            // filtering, no classification beyond the raw event kind
            // -- that all happens inside ChangeEventNormalizer next.
            events.push_back({watchDir_ / event->name, TranslateMask(event->mask)});
        }
        offset += static_cast<ssize_t>(sizeof(struct inotify_event)) + event->len;
    }

    if (events.empty()) return;

    // The entire "intelligent" part of this watcher's job is this one
    // call -- canonicalization, deduplication, verification, impact
    // analysis, selective recompilation, locking, and atomic
    // persistence are ALL owned by code this file does not contain.
    auto reports = ChangeEventNormalizer::ProcessEvents(watchDir_, registryPath_, events);
    for (auto& report : reports) history_.push_back(std::move(report));
}

}  // namespace dominus::reality
