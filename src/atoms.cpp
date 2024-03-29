#include "atoms.hpp"

NetAtom::NetAtom(Display *display) :
supported       (XInternAtom(display, "_NET_SUPPORTED", False)),
activeWindow    (XInternAtom(display, "_NET_ACTIVE_WINDOW", False)),
numberOfDesktops(XInternAtom(display, "_NET_NUMBER_OF_DESKTOPS", False)),
currentDesktop  (XInternAtom(display, "_NET_CURRENT_DESKTOP", False)),
WMCheck         (XInternAtom(display, "_NET_SUPPORTING_WM_CHECK", False)),
WMName          (XInternAtom(display, "_NET_WM_NAME", False)),
WMWindowType    (XInternAtom(display, "_NET_WM_WINDOW_TYPE", False)),
WMWindowDock    (XInternAtom(display, "_NET_WM_WINDOW_TYPE_DOCK", False)),
WMWindowToolbar (XInternAtom(display, "_NET_WM_WINDOW_TYPE_TOOLBAR", False)),
WMWindowUtility (XInternAtom(display, "_NET_WM_WINDOW_TYPE_UTILITY", False)),
WMWindowDialog  (XInternAtom(display, "_NET_WM_WINDOW_TYPE_DIALOG", False)),
WMWindowMenu    (XInternAtom(display, "_NET_WM_WINDOW_TYPE_MENU", False)) {
}

size_t NetAtom::size() const {
	return sizeof(NetAtom) / sizeof(Atom);
}


IccAtom::IccAtom(Display *display) :
DeleteWindow   (XInternAtom(display, "WM_DELETE_WINDOW", False)),
WMProtocols    (XInternAtom(display, "WM_PROTOCOLS", False)) {
}

OtherAtom::OtherAtom(Display *display) :
utf8str        (XInternAtom(display, "UTF8_STRING", False)) {
}
