#pragma once

struct Vector2 {
	constexpr Vector2(int _x = 0, int _y = 0);

	constexpr Vector2 operator+(const Vector2 rhs) const;

	constexpr Vector2 operator-(const Vector2 rhs) const;

	int x, y;
};
