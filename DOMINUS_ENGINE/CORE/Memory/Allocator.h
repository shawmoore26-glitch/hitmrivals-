// CORE/Memory/Allocator.h
// Frame-scoped arena allocator. Leaf module -- depends on nothing else in
// the engine. Every other module allocates through an ArenaAllocator (or a
// pool built on one) rather than calling new/malloc directly. See
// docs/ARCHITECTURE_v0.1.md section 4 for the module contract.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>

namespace dominus::core {

// A simple bump allocator over a fixed-size backing buffer. Allocate() never
// frees individual allocations -- call Reset() to reclaim the whole arena
// at once (typically once per frame, or once per load scope).
class ArenaAllocator {
public:
    explicit ArenaAllocator(size_t byteCapacity)
        : capacity_(byteCapacity),
          buffer_(std::make_unique<uint8_t[]>(byteCapacity)),
          offset_(0) {}

    ArenaAllocator(const ArenaAllocator&) = delete;
    ArenaAllocator& operator=(const ArenaAllocator&) = delete;

    // Returns a pointer to `bytes` of memory aligned to `alignment`.
    // Throws std::bad_alloc if the arena is exhausted -- callers that need
    // graceful degradation should catch this at the call site; the arena
    // itself never silently grows (that would defeat the point of a
    // frame-scoped allocator).
    void* Allocate(size_t bytes, size_t alignment) {
        uintptr_t current = reinterpret_cast<uintptr_t>(buffer_.get()) + offset_;
        uintptr_t aligned = AlignUp(current, alignment);
        size_t padding = aligned - current;

        if (offset_ + padding + bytes > capacity_) {
            throw std::bad_alloc();
        }

        offset_ += padding + bytes;
        return reinterpret_cast<void*>(aligned);
    }

    // Reclaims the entire arena. Does not call destructors -- callers are
    // responsible for the lifetime semantics of what they placed here
    // (this allocator is for POD/trivially-destructible frame data by
    // design; anything else belongs in a pool with explicit ownership).
    void Reset() { offset_ = 0; }

    size_t Capacity() const { return capacity_; }
    size_t Used() const { return offset_; }

private:
    static uintptr_t AlignUp(uintptr_t value, size_t alignment) {
        return (value + alignment - 1) & ~(alignment - 1);
    }

    size_t capacity_;
    std::unique_ptr<uint8_t[]> buffer_;
    size_t offset_;
};

}  // namespace dominus::core
