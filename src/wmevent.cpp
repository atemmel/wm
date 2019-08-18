#include "event.hpp"

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <iostream>

void send() {
	Display *display;
	Window root;
	XEvent e;

	puts("here");
	display = XOpenDisplay(nullptr);

	if(!display) return;

	puts("here");
	root = DefaultRootWindow(display);

	e.xclient.type = ClientMessage;
	e.xclient.message_type = XInternAtom(display, Event::RequestAtom, False);
	e.xclient.window = root;
	e.xclient.format = 32;
	
	e.xclient.data.l[0] = static_cast<long>(Event::Kill);

	puts("here");
	XSendEvent(display, root, false, SubstructureRedirectMask, &e);
	puts("here");
	XSync(display, false);
	puts("here");
	XCloseDisplay(display);
}

int main(int argc, char **argv) {
	puts("here");
	send();
}
