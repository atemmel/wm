#include "event.hpp"

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <iostream>

void send() {
	Display *display;
	Window root;
	XEvent e;

	display = XOpenDisplay(nullptr);

	if(!display) return;

	root = DefaultRootWindow(display);

	e.xclient.type = ClientMessage;
	e.xclient.message_type = XInternAtom(display, Event::RequestAtom, False);
	e.xclient.window = root;
	e.xclient.format = 32;
	
	e.xclient.data.l[0] = static_cast<long>(Event::Kill);
	std::cout << e.xclient.data.l[0] << '\n';

	XSendEvent(display, root, false, SubstructureRedirectMask, &e);
	XSync(display, false);
	XCloseDisplay(display);
}

int main(int argc, char **argv) {
	send();
}
