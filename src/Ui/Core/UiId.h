#pragma once
#include "DTL.h"

// Stable element identity. An element's id must be the same every frame so that
// persistent state (hover/active) keyed by id survives across the immediate-mode
// rebuild. Ids are an FNV-1a hash of (instance pointer + sibling index + seed),
// mirroring Clay's CLAY_ID / CLAY_IDI scheme.
namespace Ui {

using UiId = u32;
constexpr UiId kNullId = 0;

namespace detail {
    constexpr u32 kFnvOffset = 2166136261u;
    constexpr u32 kFnvPrime = 16777619u;

    inline u32 FnvMix(u32 hash, u32 value)
    {
        for (u32 i = 0; i < 4; ++i) {
            hash ^= (value >> (i * 8)) & 0xFFu;
            hash *= kFnvPrime;
        }
        return hash;
    }
}

// Combine a pointer (component instance), a sibling/loop index, and a seed into a
// stable, non-zero id. Reserving 0 as kNullId lets callers pass 0 to mean "auto".
inline UiId HashId(const void* ptrSeed, u32 index, u32 seed = 0)
{
    u64 p = reinterpret_cast<u64>(ptrSeed);
    u32 hash = detail::kFnvOffset;
    hash = detail::FnvMix(hash, static_cast<u32>(p & 0xFFFFFFFFu));
    hash = detail::FnvMix(hash, static_cast<u32>((p >> 32) & 0xFFFFFFFFu));
    hash = detail::FnvMix(hash, index);
    hash = detail::FnvMix(hash, seed);
    return hash == kNullId ? 1u : hash;
}

// Mix a parent id into a child id so nested components built from the same
// instance stay unique without the caller threading indices everywhere.
inline UiId HashChild(UiId parent, UiId child)
{
    return detail::FnvMix(parent ^ 0x9E3779B9u, child);
}

} // namespace Ui
