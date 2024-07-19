#pragma once

#include <stdint.h>

#ifdef IS_ENGINE
#define ENGINE_API __declspec(dllexport)
#else
#define ENGINE_API __declspec(dllimport)
#endif

#if !defined(COMPILER_MSVC)
#define COMPILER_MSVC 0
#endif

#if !defined(COMPILER_LLVM)
#define COMPILER_LLVM 0
#endif

#if  !COMPILER_MSVC && !COMPILER_LLVM
#if _MSC_VER
#undef COMPILER_MSVC
#define COMPILER_MSVC 1
#else
//default
#undef COMPILER_LLVM
#define COMPILER_LLVM 1
#endif
#endif

#ifdef COMPILER_MSVC
#include <intrin.h>
#endif // COMPILER_MSVC


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

using r32 = float;
using r64 = double;

using b32 = u32;

using MemoryIndex = size_t;

#define KiloBytes(Value) (1024ull * (Value))
#define MegaBytes(Value) (1024ull * KiloBytes(Value))
#define GigaBytes(Value) (1024ull * MegaBytes(Value))
#define TeraBytes(Value) (1024ull * GigaBytes(Value))

#if ENGINE_BUILD_DEBUG
	#define Assert(Expression) if(!(Expression)) {__debugbreak();}
	#define Ensure(Expression) ((Expression) || ([] () {__debugbreak();}(), false))
	#define Halt() __debugbreak()
	#define InvalidCodePath() Assert(false)
#else
	#define Assert(Expression)
	#define Ensure(Expression) (Expression)
	#define Halt()
	#define InvalidCodePath()
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