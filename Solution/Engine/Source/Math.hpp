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

struct Rectangle
{
	V2 min;
	V2 max;
};

inline Rectangle RectMinMax(V2 min, V2 max)
{
	return { min, max };
}

inline Rectangle RectMinDim(V2 min, V2 dim)
{
	return { min, min + dim };
}

inline Rectangle RectCenterHalfDim(V2 center, V2 half_dim)
{
	return { center - half_dim, center + half_dim };
}

inline Rectangle RectCenterDim(V2 center, V2 dim)
{
	return { center - dim * 0.5f, center + dim * 0.5f };
}

inline b32 IsInRectangle(Rectangle rect, V2 a)
{
	b32 result {	
		(rect.min.x <= a.x) &&
		(rect.min.y <= a.y) &&
		(rect.max.x > a.x) &&
		(rect.max.y > a.y)
	};


	return result;
}