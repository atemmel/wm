#pragma once
#ifndef EVENT_HPP
#define EVENT_HPP

namespace Event {

constexpr static auto RequestAtom = "WM_REQUEST";

enum {
	MoveDirection = 0,
	GoDirection,
	Zoom,
	Kill,
	NEvents
};

};

#endif
