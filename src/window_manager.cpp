#include "window_manager.hpp"

#include <glog/logging.h>	/* TODO: Remove dependency */
#include <X11/Xutil.h>

#include <iostream>
#include <string>
#include <array>

bool WindowManager::_wmDetected = false;

std::unique_ptr<WindowManager> WindowManager::create() {
	Display *display = XOpenDisplay(nullptr);
	if(display == nullptr) {
		LOG(ERROR) << "Failed to open X display " << XDisplayName(nullptr);
		return nullptr;
	}

	return std::unique_ptr<WindowManager>(new WindowManager(display) );
}

WindowManager::WindowManager(Display *display) 
	: _display(display), _root(DefaultRootWindow(_display) ), _focused(nullptr) {
}

WindowManager::~WindowManager() {
	XCloseDisplay(_display);
}

void WindowManager::run() {
	//Set temporary errror handler
	XSetErrorHandler(&WindowManager::onWmDetected);
	XSelectInput(
			_display,
			_root,
			SubstructureRedirectMask | SubstructureNotifyMask);
	XSync(_display, false);	//Flush errors
	if(_wmDetected) {
		LOG(ERROR) << "Detected another window manager on display " 
			<< XDisplayString(_display);
		return;
	}
	//Set regular error handler
	XSetErrorHandler(&WindowManager::onXError);

	XGrabServer(_display);
	Window returnedRoot, returnedParent;
	Window *topLevel;

	unsigned int n_topLevel;

	CHECK(XQueryTree(
				_display,
				_root,
				&returnedRoot,
				&returnedParent,
				&topLevel,
				&n_topLevel));

	for(unsigned int i = 0; i < n_topLevel; i++) {
		frame(topLevel[i], true);
	}

	XFree(topLevel);
	XUngrabServer(_display);

	_screen = XDefaultScreenOfDisplay(_display);

	initKeys();

	//TODO: Log "All OK" message
	/*	Loop	*/
	while(true) {
		XEvent e;
		XNextEvent(_display, &e);
		//LOG(INFO) << "Recieved event: " << std::to_string(e.type);

		switch(e.type) {
			case ConfigureRequest:
				onConfigureRequest(e.xconfigurerequest);
				break;
			case MapRequest:
				onMapRequest(e.xmaprequest);
				break;
			case UnmapNotify:
				onUnmapNotify(e.xunmap);
				break;
			case ButtonPress:
				onButtonPress(e.xbutton);
				break;
			case FocusIn:
				onFocusIn(e.xfocus);
				break;
			case EnterNotify:
				onEnterNotify(e.xcrossing);
				break;
			case MotionNotify:
				//Waste all MotionNotify events but the latest
				while(XCheckTypedWindowEvent(
							_display,
							e.xmotion.window,
							MotionNotify,
							&e));

				onMotionNotify(e.xmotion);
				break;
			case KeyPress:
				if(e.xkey.state & modifierMask) {
					if(auto it = _binds.find(e.xkey.keycode); it != _binds.end() ) {
						it->second(e.xkey.state);
					}
				}
				break;
			case CreateNotify:
			case DestroyNotify:
			case ReparentNotify:
			case ConfigureNotify:
			case MapNotify:
			default:
				//LOG(WARNING) << "Ignored event";
				break;
		}
	}
}

int WindowManager::onXError(Display *display, XErrorEvent *e) { 
	std::cout << "X error: " << static_cast<int>(e->error_code) 
		<< " : " << static_cast<int>(e->request_code)
		<< " : " << static_cast<int>(e->resourceid) << '\n';
	return 0; 
}

int WindowManager::onWmDetected(Display *display, XErrorEvent *e) {
	CHECK_EQ(static_cast<int>(e->error_code), BadAccess);
	_wmDetected = true;
	return 0;
}

void WindowManager::onConfigureRequest(const XConfigureRequestEvent &e) {
	XWindowChanges changes;

	changes.x = e.x;
	changes.y = e.y;
	changes.width = e.width;
	changes.height = e.height;
	changes.sibling = e.above;
	changes.stack_mode = e.detail;

	if(exists(e.window)) {
		XConfigureWindow(_display, e.window, e.value_mask, &changes);
		LOG(INFO) << "Resize " << e.window << " to w:" 
			<< std::to_string(e.width) << " h:" << std::to_string(e.height);
	}
}

void WindowManager::onMapRequest(const XMapRequestEvent &e) {
	frame(e.window, false);
	XMapWindow(_display, e.window);
	focus(find(e.window));
}

void WindowManager::onUnmapNotify(const XUnmapEvent &e) {
	if(e.event == _root) {
		LOG(INFO) << "Ignore UnmapNotify for reparented pre-existing window " << e.window;
		return;
	}

	if(!exists(e.window) ) {
		LOG(INFO) << "Ignore UnmapNotify for non-client window " << e.window;
		return;
	}

	unframe(find(e.window));
}

void WindowManager::onButtonPress(const XButtonEvent &e) {
	const Client &client = find(e.window);
	LOG(INFO) << "Click in window " << std::to_string(e.window);

	startCursorPos = {e.x_root, e.y_root};

	Window returned;
	Vector2 pos;
	unsigned int w, h, border, depth;

	XGetGeometry(
			_display,
			client.border,
			&returned,
			&pos.x, &pos.y,
			&w, &h,
			&border,
			&depth);

	startWindowPos = pos;
	startWindowSize = {
		static_cast<int>(w), 
		static_cast<int>(h)};
}

void WindowManager::onFocusIn(const XFocusChangeEvent &e) {
	std::cerr << "Focus changed" << e.window << '\n';
}

void WindowManager::onEnterNotify(const XEnterWindowEvent &e) {
	LOG(INFO) << "Entered window " << std::to_string(e.window);
	focus(find(e.window));
}

void WindowManager::onMotionNotify(const XMotionEvent &e) {
	//LOG(INFO) << "Motion in window " << std::to_string(e.window);

	const Vector2 cursorPos = {e.x_root, e.y_root};
	const Client &client = find(e.window);

	if(e.state & Button1Mask) {	//Move window
		const Vector2 delta = cursorPos - startCursorPos;
		const Vector2 newPos = startWindowPos + delta;
		XMoveWindow(
				_display,
				client.border,
				newPos.x,
				newPos.y);

	}
	else if(e.state & Button3Mask) { //Resize window
		constexpr int minWinSize = 64;
		const Vector2 delta = cursorPos - startCursorPos;
		const Vector2 newSize = {
			std::max(startWindowSize.x + delta.x, minWinSize),
			std::max(startWindowSize.y + delta.y, minWinSize)};

		//One for the window
		XResizeWindow(
				_display,
				e.window,
				newSize.x, newSize.y);
		//One for the border
		XResizeWindow(
				_display,
				client.border,
				newSize.x, newSize.y);
	}
}

void WindowManager::focus(Client &client) {
	_focused = &client;
	XRaiseWindow(_display, client.border);
	XSetInputFocus(_display, client.window, RevertToParent, CurrentTime);
}

void WindowManager::focusNext() {
	for(auto it = _clients.rbegin(); it != _clients.rend(); it++) {
		if(it->workspace == _currentWorkspace) {
			focus(*it);
			return;
		}
	}

	_focused = nullptr;
	XSetInputFocus(_display, _root, RevertToPointerRoot, CurrentTime);
}

void WindowManager::frame(Window w, bool createdBefore) {

	XWindowAttributes attrs;
	CHECK(XGetWindowAttributes(_display, w, &attrs) );

	if(createdBefore) {
		if(attrs.override_redirect || 
				attrs.map_state != IsViewable) {
			return;
		}
	}

	const Window frame = XCreateSimpleWindow(
			_display,
			_root,
			attrs.x,
			attrs.y,
			attrs.width,
			attrs.height,
			borderWidth,
			borderColor,
			bgColor);

	XSelectInput(
			_display,
			frame,
			SubstructureRedirectMask | SubstructureNotifyMask);

	XSelectInput(
			_display,
			w,
			EnterWindowMask);

	XReparentWindow(
			_display,
			w,
			frame,
			0, 0);

	XMapWindow(_display, frame);
	_clients.push_back({w, frame, _currentWorkspace });

	//Grab Alt + LMB
	XGrabButton(
			_display,
			Button1,
			modifierMask,
			w,
			false,
			ButtonPressMask | ButtonMotionMask,
			GrabModeAsync,
			GrabModeAsync,
			None,
			None);

	//Grab Alt + RMB
	XGrabButton(
			_display,
			Button3,
			modifierMask,
			w,
			false,
			ButtonPressMask | ButtonMotionMask,
			GrabModeAsync,
			GrabModeAsync,
			None,
			None);

	LOG(INFO) << "Framed window " << w << " [" << frame << ']';
}

void WindowManager::unframe(const Client &client) {

	XUnmapWindow(_display, client.border);

	XDestroyWindow(_display, client.border);
	erase(client.window);
	focusNext();

	LOG(INFO) << "Unframed Window" << client.window << " [" << client.border << ']';
}

void WindowManager::switchWorkspace(int workspace) {
	XWindowAttributes xattr;

	for(auto &client : _clients) {
		if(client.workspace == _currentWorkspace) {
			hide(client);
		}
	}

	_currentWorkspace = workspace;

	for(auto &client : _clients) {
		if(client.workspace == _currentWorkspace) {
			show(client);
		}
	}

	focusNext();

	printLayout();
}

void WindowManager::hide(Client &client) {
	XWindowAttributes xattr;
	XGetWindowAttributes(_display, client.border, &xattr);

	client.restore = {xattr.x, xattr.y};

	//Big brain window hide
	XMoveWindow(
		_display,
		client.border,
		xattr.x + _screen->width,
		xattr.y + _screen->height);
};

void WindowManager::show(const Client &client) {
	XWindowAttributes xattr;

	XMoveWindow(
		_display,
		client.border,
		client.restore.x,
		client.restore.y);
}

void WindowManager::kill(const Client &client) const {
	/*	To be implemented
	XEvent ev;
    ev.type = ClientMessage;
    ev.xclient.window = client.window;
    ev.xclient.message_type = _NET_CLOSE_WINDOW;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = WM_DELETE_WINDOW;
    ev.xclient.data.l[1] = CurrentTime;
    XSendEvent(display, client.window, False, NoEventMask, &ev);
	*/
}

void WindowManager::moveClient(Client &client, int workspace) {
	if(client.workspace == workspace) return;
	client.workspace = workspace;
	hide(client);
	focusNext();
}

void WindowManager::zoomClient(Client &client) {
	/* To be implemented */
}

void WindowManager::initKeys() {

	KeyCode left  = XKeysymToKeycode(_display, XK_H),
			right = XKeysymToKeycode(_display, XK_L),
			up    = XKeysymToKeycode(_display, XK_K),
			down  = XKeysymToKeycode(_display, XK_J),
			kill  = XKeysymToKeycode(_display, XK_Q),
			enter = XKeysymToKeycode(_display, XK_Return);

	//Left
	XGrabKey(_display,
			left,
			AnyModifier,
			_root,
			False,
			GrabModeAsync,
			GrabModeAsync);

	//Right
	XGrabKey(_display,
			right,
			AnyModifier,
			_root,
			False,
			GrabModeAsync,
			GrabModeAsync);

	//Up
	XGrabKey(_display,
			up,
			AnyModifier,
			_root,
			False,
			GrabModeAsync,
			GrabModeAsync);

	//Down
	XGrabKey(_display,
			down,
			AnyModifier,
			_root,
			False,
			GrabModeAsync,
			GrabModeAsync);

	//Kill
	XGrabKey(_display,
			kill,
			AnyModifier,
			_root,
			False,
			GrabModeAsync,
			GrabModeAsync);

	//Spawn term
	XGrabKey(_display,
			enter,
			AnyModifier,
			_root,
			False,
			GrabModeAsync,
			GrabModeAsync);

	auto changeWsCall = [&](WindowManager::Direction dir, unsigned int state) {
		if(state & ShiftMask) {
			const int newWorkspace = workspaceMap(dir);
			std::cout << "Moving window to workspace " << newWorkspace << '\n';
			if(_focused) moveClient(*_focused, newWorkspace);
		}
		else {
			const int newWorkspace = workspaceMap(dir);
			std::cout << "Switching to workspace " << newWorkspace << '\n';
			switchWorkspace(newWorkspace);
		}
	};

	using namespace std::placeholders;

	_binds.insert(
	{
		left, 
		std::bind(changeWsCall, WindowManager::Direction::Left, _1)
	});

	_binds.insert(
	{
		right, 
		std::bind(changeWsCall, WindowManager::Direction::Right, _1)
	});

	_binds.insert(
	{
		up, 
		std::bind(changeWsCall, WindowManager::Direction::Up, _1)
	});

	_binds.insert(
	{
		down, 
		std::bind(changeWsCall, WindowManager::Direction::Down, _1)
	});

	_binds.insert(
	{
		kill,
		[&](unsigned int state) {
			
		}
	});

	_binds.insert(
	{
		enter, 
		[&](unsigned int state) {
			system("urxvt &");
		}
	});

}

void WindowManager::printLayout() const {
	using Ws = WindowManager::Ws;
	constexpr auto current = "[*]", other = "[ ]";
	auto p = [&](Ws ws) {
		return static_cast<int>(ws) == _currentWorkspace ? current : other;
	};

	std::cout 
		<< "    "	<<			p(North) << '\n'
		<< p(West)	<< ' ' <<	p(Center) << ' ' 	<< 	p(East) << '\n'
		<< "    "	<< 			p(South) << '\n';
}

constexpr int WindowManager::workspaceMap(Direction dir) const {

	//Table to map current workspace + direction to a new workspace
	constexpr std::array<std::array<Ws, 4>, nWorkspaces> table {{
		//Left			Right		Up			Down
		{{ West,		East,		North,		South	}},	//Center
		{{ East,		Center,		North,		South	}},	//West
		{{ Center,		West,		North,		South	}},	//East
		{{ West,		East,		South,		Center	}},	//North
		{{ West,		East,		Center,		North	}}	//South
	}};

	return static_cast<int>(table[_currentWorkspace][static_cast<int>(dir)]);
}

bool WindowManager::exists(Window w) const {
	return std::find_if(_clients.begin(), _clients.end(), [&](const Client &client) {
				return client.window == w;
		}) != _clients.end();
}

Client &WindowManager::find(Window w) {
	return *std::find_if(_clients.begin(), _clients.end(), [&](const Client &client) {
				return client.window == w;
		});
}

void WindowManager::erase(Window w) {
	_clients.erase(std::remove_if(_clients.begin(), _clients.end(), [&](const Client &client) {
					return client.window == w;
		}));
}
