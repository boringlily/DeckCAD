#pragma once
#include <cinttypes>

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using s8 = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;

using f32 = float;
using f64 = double;

constexpr u8 u8_max = UINT8_MAX;
constexpr u16 u16_max = UINT16_MAX;
constexpr u32 u32_max = UINT32_MAX;
constexpr u64 u64_max = UINT64_MAX;

constexpr s8 s8_max = INT8_MAX;
constexpr s16 s16_max = INT16_MAX;
constexpr s32 s32_max = INT32_MAX;
constexpr s64 s64_max = INT64_MAX;