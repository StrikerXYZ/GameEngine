#pragma once

#include "Definition.hpp"

#include "math.h"

inline i32 RoundToI32(r32 real)
{
	return static_cast<i32>(real + .5f);
}

inline u32 RoundToU32(r32 real)
{
	return static_cast<u32>(real + .5f);
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
