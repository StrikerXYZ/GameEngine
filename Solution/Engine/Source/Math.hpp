#pragma once

#include "Definition.hpp"

union V2
{
	struct
	{
		r32 x, y;
	};
	r32 e[2];
};

inline V2 v2(r32 x, r32 y)
{
	V2 result;
	result.x = x;
	result.y = y;
	return result;
}

inline V2 operator+(V2 a, V2 b)
{
	V2 result;
	result.x = a.x + b.x;
	result.y = a.y + b.y;
	return result;
}

inline V2 operator-(V2 a, V2 b)
{
	V2 result;
	result.x = a.x - b.x;
	result.y = a.y - b.y;
	return result;
}

inline V2 operator-(V2 a)
{
	V2 result;
	result.x = -a.x;
	result.y = -a.y;
	return result;
}

inline V2 operator*(V2 a, r32 b)
{
	V2 result;
	result.x = b * a.x;
	result.y = b * a.y;
	return result;
}

inline V2 operator*(r32 a, V2 b)
{
	V2 result;
	result = b * a;
	return result;
}

inline V2 operator+=(V2& a, V2 b)
{
	a = a + b;
	return a;
}

inline V2 operator-=(V2& a, V2 b)
{
	a = a - b;
	return a;
}

inline V2& operator*=(V2& a, r32 b)
{
	a = a * b;
	return a;
}

inline r32 Square(r32 a)
{
	return a * a;
}

inline r32 Inner(V2 a, V2 b)
{
	return a.x * b.x + a.y * b.y;
}

inline r32 LengthSquared(V2 a)
{
	return Inner(a, a);
}