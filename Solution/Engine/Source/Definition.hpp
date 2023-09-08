#pragma once

#pragma warning (push, 0)
#include <stdint.h>
#pragma warning (pop)

//define static types
#define local_static static
#define global_static static //variables cannot be used in other translation units and initialized to 0 by default
#define internal_static static

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
