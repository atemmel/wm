#include <X11/Xlib.h>

#include <iostream>

void send() {
	Display *display;
	Window root;
	XEvent e;

	display = XOpenDisplay(nullptr);

	if(!display) return;

	root = DefaultRootWindow(display);

	e.xclient.type = ClientMessage;

	e.xclient.window = root;
	e.xclient.format = 32;

	XSendEvent(display, root, false, SubstructureRedirectMask, &e);
	XSync(display, false);
	XCloseDisplay(display);
}

int main(int argc, char **argv) {
	send();
}
