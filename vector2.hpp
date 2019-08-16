#pragma once

struct Vector2 {
	Vector2(int _x = 0, int _y = 0);

	Vector2 operator+(const Vector2 rhs) const;

	Vector2 operator-(const Vector2 rhs) const;

	int x, y;
};
