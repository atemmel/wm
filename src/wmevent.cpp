#include "event.hpp"

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <string_view>
#include <algorithm>
#include <iostream>
#include <array>

struct Option {
	const std::string_view name;
	const int args;
};

static void die(std::string_view str);

static void send(size_t type, char **argv);

int main(int argc, char **argv) {
	if(argc == 1) die("No argument given. Run -h to see arguments");

	constexpr std::array<Option, static_cast<size_t>(Event::NEvents)> options = {{
		{ "move_to", 1 },		//Move focused window to workspace
		{ "change_to", 1 },		//Change active workspace
		{ "zoom", 0 },			//Zoom focused window
		{ "kill", 0 }			//Kill focused window
	}};

	std::string_view arg = argv[1];

	auto it = std::find_if(options.begin(), options.end(), [&](const Option &opt) {
		return arg == opt.name;
	});

	if(it == options.end() ) {
		die("Argument not recognized. Run -h to see arguments.");
	}

	if(it->args != argc - 2) {
		die("Parameter count mismatch. Run -h to see arguments.");
	}

	size_t type = std::distance(options.begin(), it);
		
	send(type, argv);

	return EXIT_SUCCESS;
}

static void die(std::string_view str) {
	std::cout << str << '\n';
	std::exit(EXIT_FAILURE);
}

void send(size_t type, char **argv) {
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
	
	e.xclient.data.l[0] = static_cast<long>(type);

	XSendEvent(display, root, false, SubstructureRedirectMask, &e);
	XSync(display, false);
	XCloseDisplay(display);
}

