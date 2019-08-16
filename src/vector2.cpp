#include "vector2.hpp"

Vector2::Vector2(int _x, int _y) 
	: x(_x), y(_y) {}

Vector2 Vector2::operator+(const Vector2 rhs) const {
	return {x + rhs.x, y + rhs.y};
}

Vector2 Vector2::operator-(const Vector2 rhs) const {
	return {x - rhs.x, y - rhs.y};
}
