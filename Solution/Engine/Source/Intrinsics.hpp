#pragma once

#include "Definition.hpp"

#include "math.h"

inline i32 RoundToI32(r32 real)
{
	return static_cast<i32>(roundf(real));
}

inline u32 RoundToU32(r32 real)
{
	return static_cast<u32>(roundf(real));
}

inline i32 TruncateToI32(r32 real)
{
	return static_cast<i32>(real);
}

inline i32 FloorToI32(r32 real)
{
	return static_cast<i32>(floorf(real));
}

inline r32 Sin(r32 angle)
{
	return sinf(angle);
}

inline r32 Cos(r32 angle)
{
	return cosf(angle);
}

inline r32 ATan2(r32 y, r32 x)
{
	return atan2f(y, x);
}

struct BitScanResult
{
	b32 found;
	u32 index;
};
inline BitScanResult FindLeastSignificantSetBit(u32 value)
{
	BitScanResult result{};

#if 1 //COMPILER_MSVC
	result.found = _BitScanForward(reinterpret_cast<unsigned long*>(&result.index), static_cast<unsigned long>(value));

#else

	for (u32 test = 0; test < 32; ++test)
	{
		if (value & 1 << test)
		{
			result.found = true;
			result.index = test;
			break;
		}
	}

#endif

	return result;
}
