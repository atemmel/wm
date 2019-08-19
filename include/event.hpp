#pragma once

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


