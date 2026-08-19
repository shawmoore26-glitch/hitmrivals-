// tests/core/test_allocator.cpp
#include "CORE/Memory/Allocator.h"
#include "tests/TestFramework.h"

using dominus::core::ArenaAllocator;

DOMINUS_TEST(Allocator_AllocatesWithinCapacity) {
    ArenaAllocator arena(1024);
    void* p1 = arena.Allocate(64, alignof(std::max_align_t));
    void* p2 = arena.Allocate(64, alignof(std::max_align_t));
    DOMINUS_EXPECT(p1 != nullptr);
    DOMINUS_EXPECT(p2 != nullptr);
    DOMINUS_EXPECT(p1 != p2);
    DOMINUS_EXPECT(arena.Used() <= arena.Capacity());
}

DOMINUS_TEST(Allocator_ThrowsWhenExhausted) {
    ArenaAllocator arena(16);
    bool threw = false;
    try {
        arena.Allocate(1024, 1);
    } catch (const std::bad_alloc&) {
        threw = true;
    }
    DOMINUS_EXPECT(threw);
}

DOMINUS_TEST(Allocator_ResetReclaimsSpace) {
    ArenaAllocator arena(128);
    arena.Allocate(64, 1);
    DOMINUS_EXPECT(arena.Used() > 0);
    arena.Reset();
    DOMINUS_EXPECT(arena.Used() == 0);
    // Should be able to allocate the full capacity again after reset.
    void* p = arena.Allocate(128, 1);
    DOMINUS_EXPECT(p != nullptr);
}

DOMINUS_TEST(Allocator_RespectsAlignment) {
    ArenaAllocator arena(256);
    arena.Allocate(1, 1);  // misalign the offset
    void* p = arena.Allocate(16, 16);
    DOMINUS_EXPECT(reinterpret_cast<uintptr_t>(p) % 16 == 0);
}
