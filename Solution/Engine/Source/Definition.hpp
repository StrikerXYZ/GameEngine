#pragma once

#include <stdint.h>

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

using f32 = float;
using f64 = double;

using b32 = bool;

#define KiloBytes(Value) (1024ull * (Value))
#define MegaBytes(Value) (1024ull * KiloBytes(Value))
#define GigaBytes(Value) (1024ull * MegaBytes(Value))
#define TeraBytes(Value) (1024ull * GigaBytes(Value))

#if ENGINE_BUILD_DEBUG
	#define Assert(Expression) if(!(Expression)) {__debugbreak();}
	#define Ensure(Expression) ((Expression) || ([] () {__debugbreak();}(), false))
	#define Halt() __debugbreak()
#else
	#define Assert(Expression)
	#define Ensure(Expression) (Expression)
	#define Halt()
#endif // ENGINE_BUILD_DEBUG

internal_static u32 SafeTruncate32(i64 value)
{
	Assert(value <= 0xffffffff); //should be less than max dword 32bit
	return static_cast<u32>(value);
}

template<typename T>
internal_static void Swap(T& a, T& b)
{
	const T temp = a;
	a = b;
	b = temp;
}

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))