#pragma once
#ifndef ATOMS_HPP
#define ATOMS_HPP

extern "C" {
	#include <X11/Xlib.h>
}

struct IccAtom {
	IccAtom(Display *display);

	//Sources for icccm atoms:
	//https://www.x.org/docs/ICCCM/icccm.pdf
	Atom DeleteWindow;
	Atom WMProtocols;
};

struct NetAtom {
	NetAtom(Display *display);

	size_t size() const;
	//Further sources
	//https://specifications.freedesktop.org/wm-spec/1.3/ar01s03.html
	Atom supported;
	Atom activeWindow;
	Atom WMCheck;
	Atom WMName;
	Atom WMWindowType;
	Atom WMWindowDock;
	Atom WMWindowToolbar;
	Atom WMWindowUtility;
	Atom WMWindowDialog;
	Atom WMWindowMenu;
};

struct OtherAtom {
	OtherAtom(Display *display);

	Atom utf8str;
};

#endif
