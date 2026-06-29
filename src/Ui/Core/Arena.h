#pragma once
#include "DTL.h"

// A bump allocator placed over a user-supplied byte buffer. The framework never
// calls malloc itself: all per-frame and persistent storage is carved from here.
namespace Ui {

struct Arena {
    u8* base { nullptr };
    u64 size { 0 };
    u64 offset { 0 };
    u64 highWater { 0 }; // peak offset, for right-sizing the buffer.
    bool overflowed { false };

    void Init(void* buffer, u64 bytes)
    {
        base = static_cast<u8*>(buffer);
        size = bytes;
        offset = 0;
        highWater = 0;
        overflowed = false;
    }

    // Bump-allocate `bytes` with the given alignment. On overflow sets the flag
    // and returns nullptr rather than corrupting memory or aborting.
    void* Alloc(u64 bytes, u64 align = 16)
    {
        u64 aligned = (offset + (align - 1)) & ~(align - 1);
        if (aligned + bytes > size) {
            overflowed = true;
            return nullptr;
        }
        offset = aligned + bytes;
        if (offset > highWater) {
            highWater = offset;
        }
        return base + aligned;
    }

    template <typename T>
    T* AllocArray(u32 count)
    {
        return static_cast<T*>(Alloc(static_cast<u64>(sizeof(T)) * count, alignof(T)));
    }

    void Reset()
    {
        offset = 0;
        overflowed = false;
    }

    u64 Mark() const { return offset; }
    void Rewind(u64 mark) { offset = mark; }
};

} // namespace Ui
