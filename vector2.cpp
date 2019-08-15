#include "vector2.hpp"

constexpr Vector2::Vector2(int _x, int _y) 
	: x(_x), y(_y) {}

constexpr Vector2 Vector2::operator+(const Vector2 rhs) const {
	return {x + rhs.x, y + rhs.y};
}

constexpr Vector2 Vector2::operator-(const Vector2 rhs) const {
	return {x - rhs.x, y - rhs.y};
}
