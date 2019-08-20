#pragma once
#ifndef VECTOR2_HPP
#define VECTOR2_HPP

struct Vector2 {
	Vector2(int _x = 0, int _y = 0);

	Vector2 operator+(const Vector2 rhs) const;

	Vector2 operator-(const Vector2 rhs) const;

	int x, y;
};

#endif
