#pragma once
#ifndef EVENT_HPP
#define EVENT_HPP

namespace Event {

constexpr static auto RequestAtom = "WM_REQUEST";

enum {
	MoveToWorkspace = 0,
	ChangeWorkspace,
	Zoom,
	Kill,
	NEvents
};

};

#endif
