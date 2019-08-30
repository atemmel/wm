#include "window_manager.hpp"
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

static void help();

static long dirToLong(std::string_view str);

static void send(size_t type, int argc, char **argv);

int main(int argc, char **argv) {
	if(argc == 1) die("No argument given. Run -h to see arguments");

	std::string_view arg = argv[1];

	if(arg == "-h") help();

	constexpr std::array<Option, static_cast<size_t>(Event::NEvents)> options = {{
		{ "move", 1 },		//Move focused window to workspace
		{ "go",   1 },		//Change active workspace
		{ "zoom", 0 },		//Zoom focused window
		{ "kill", 0 },		//Kill focused window
		{ "exit", 0 },		//Exit wm
		{ "focusnext", 0},	//Focus next window in workspace
		{ "focusprev", 0}	//Focus previous window in workspace
	}};

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
		
	send(type, argc, argv);

	return EXIT_SUCCESS;
}

static void die(std::string_view str) {
	std::cout << str << '\n';
	std::exit(EXIT_FAILURE);
}

static void help() {
	std::cout <<
		"Options:\n"
		"move N        Moves focused window in direction N\n"
		"go N          Changes active workspace to workspace in direction N\n"
		"zoom          Zooms focused window\n"
		"kill          Kills focused window\n";
	std::exit(EXIT_SUCCESS);
}

static long dirToLong(std::string_view str) {
	std::array<std::string_view, 4> dirs;

	dirs[WindowManager::Direction::Left]  = "left";
	dirs[WindowManager::Direction::Right] = "right";
	dirs[WindowManager::Direction::Up]    = "up";
	dirs[WindowManager::Direction::Down]  = "down";

	for(size_t i = 0; i < dirs.size(); i++) {
		if(str == dirs[i]) return static_cast<long>(i);
	}

	return -1l;
}

static void send(size_t type, int argc, char **argv) {
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
	for(int i = 2; i < argc; i++) {
		if(type == Event::MoveDirection || type == Event::GoDirection) {
			e.xclient.data.l[i - 1] = dirToLong(argv[i]);
			if(e.xclient.data.l[i - 1] == -1l) die("Invalid direction.");
		}
		else e.xclient.data.l[i - 1] = std::atol(argv[i]);
	}

	XSendEvent(display, root, false, SubstructureRedirectMask, &e);
	XSync(display, false);
	XCloseDisplay(display);
}
