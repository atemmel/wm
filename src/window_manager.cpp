#define LOG_DEBUG 1
#define LOG_ERROR 1
#include "event.hpp"
#include "log.hpp"
#include "window_manager.hpp"

#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include <iostream>
#include <cassert>
#include <string>
#include <array>

bool WindowManager::_wmDetected = false;

std::unique_ptr<WindowManager> WindowManager::create() {
	Display *display = XOpenDisplay(nullptr);
	if(display == nullptr) {
		LogError << "Failed to open X display " << XDisplayName(nullptr);
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
	XSync(_display, False);	//Flush errors
	if(_wmDetected) {
		LogError << "Detected another window manager on display " 
			<< XDisplayString(_display);
		return;
	}
	//Set regular error handler
	XSetErrorHandler(&WindowManager::onXError);

	XGrabServer(_display);
	Window returnedRoot, returnedParent;
	Window *topLevel;

	unsigned int n_topLevel;

	assert(XQueryTree(
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

	//Sources for icccm atoms:
	//https://www.x.org/docs/ICCCM/icccm.pdf
	_atomDeleteWindow    = XInternAtom(_display, "WM_DELETE_WINDOW", False); //Page 43
	_atomWMProtocols     = XInternAtom(_display, "WM_PROTOCOLS", False); //Page 26

	_atomWMWindowType    = XInternAtom(_display, "_NET_WM_WINDOW_TYPE", False);
	_atomWMWindowDock    = XInternAtom(_display, "_NET_WM_WINDOW_TYPE_DOCK", False);
	_atomWMWindowToolbar = XInternAtom(_display, "_NET_WM_WINDOW_TYPE_TOOLBAR", False);
	_atomWMWindowUtility = XInternAtom(_display, "_NET_WM_WINDOW_TYPE_UTILITY", False);
	_atomWMWindowDialog  = XInternAtom(_display, "_NET_WM_WINDOW_TYPE_DIALOG", False);
	_atomWMWindowMenu    = XInternAtom(_display, "_NET_WM_WINDOW_TYPE_MENU", False);

	std::array<std::function<void(long*)>, static_cast<size_t>(Event::NEvents)>
		events = {
			[&](long *arg) {	//Move Direction
				LogDebug << "Move Direction " << arg[0] << '\n';
				if(!_focused) return;
				auto dir = static_cast<WindowManager::Direction>(arg[0]);
				moveClient(*_focused, workspaceMap(dir));
			},
			[&](long *arg) {	//Go Direction
				LogDebug << "Go Direction " << arg[0] << '\n';
				auto dir = static_cast<WindowManager::Direction>(arg[0]);
				switchWorkspace(workspaceMap(dir));
			},
			[&](long *arg) {	//Zoom
				LogDebug << "Zoom\n";
				if(!_focused) return;
				zoomClient(*_focused);
			},
			[&](long *arg) {	//Kill
				LogDebug << "Kill\n";
				if(!_focused) return;
				kill(*_focused);
			},
			[&](long *arg) {	//Exit
				LogDebug << "Exit\n";
				std::exit(EXIT_SUCCESS);
			}
		};

	LogDebug << "All clear, wm starting\n";
	/*	Loop	*/
	while(true) {
		XEvent e;
		XNextEvent(_display, &e);

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
			case ClientMessage:
				if(e.xclient.message_type == XInternAtom(_display, Event::RequestAtom, False) ) {
					LogDebug << "Client message: " << e.xclient.data.l[0] << '\n';
					events[e.xclient.data.l[0]](&e.xclient.data.l[1]);
				}
				break;
			case KeyPress:
			case CreateNotify:
			case DestroyNotify:
			case ReparentNotify:
			case ConfigureNotify:
			case MapNotify:
			default:
				break;
		}
	}
}

int WindowManager::onXError(Display *display, XErrorEvent *e) { 
	LogDebug << "X error: " << static_cast<int>(e->error_code) 
		<< " : " << static_cast<int>(e->request_code)
		<< " : " << static_cast<int>(e->resourceid) << '\n';
	return 0; 
}

int WindowManager::onWmDetected(Display *display, XErrorEvent *e) {
	assert(static_cast<int>(e->error_code) == BadAccess);
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

	if(exists(e.window)) {	//TODO: Rewrite this so there is no double lookup
		Client client = find(e.window);
		client.size = Vector2{e.width, e.height};
		client.position = Vector2{e.x, e.y};
		XConfigureWindow(_display, e.window, e.value_mask, &changes);
		LogDebug << "Resize " << e.window << " to w:" 
			<< e.width << " h:" << e.height << '\n';
	}
}

void WindowManager::onMapRequest(const XMapRequestEvent &e) {
	unsigned char *propStr = nullptr;

	//Dummy variables
	int di;
	unsigned long dl;
	Atom da;

	bool status = XGetWindowProperty(_display, e.window, _atomWMWindowType, 0, sizeof(Atom), False, 
				XA_ATOM, &da, &di, &dl, &dl, &propStr) == Success;

	LogDebug << "Status: " << std::boolalpha << status << '\n';
	LogDebug << "Propstr: " << std::boolalpha << (propStr != nullptr) << '\n';

	if(status && propStr) {
		const Atom prop = static_cast<Atom>(propStr[0]);

		if(prop == _atomWMWindowDock) {
				registerDock(e.window);
		}

		if(prop == _atomWMWindowDock ||
				prop == _atomWMWindowToolbar ||
				prop == _atomWMWindowUtility ||
				prop == _atomWMWindowMenu) {
			LogDebug << "Window " << propStr << " not managed\n";
			XMapWindow(_display, e.window);
			return;	//Do not manage the window
		}
	} 

	LogDebug << "Window managed\n";
	LogDebug << "Map as usual\n";

	frame(e.window, false);
	XMapWindow(_display, e.window);
	focus(find(e.window) );
}

void WindowManager::onUnmapNotify(const XUnmapEvent &e) {
	if(e.event == _root) {
		LogDebug << "Ignore UnmapNotify for reparented pre-existing window " << e.window << '\n';
		return;
	}

	if(!exists(e.window) ) {	//TODO: Rewrite this so there is no double lookup
		LogDebug << "Ignore UnmapNotify for non-client window " << e.window << '\n';
		return;
	}

	unframe(find(e.window));
}

void WindowManager::onButtonPress(const XButtonEvent &e) {
	const Client &client = find(e.window);
	LogDebug << "Click in window " << e.window << '\n';

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
	LogDebug << "Focus changed" << e.window << '\n';
}

void WindowManager::onEnterNotify(const XEnterWindowEvent &e) {
	LogDebug << "Entered window " << e.window << '\n';
	focus(find(e.window));
}

void WindowManager::onMotionNotify(const XMotionEvent &e) {
	const Vector2 cursorPos = {e.x_root, e.y_root};
	Client &client = find(e.window);

	if(client.fullscreen) return;

	if(e.state & Button1Mask) {	//Move window
		const Vector2 delta = cursorPos - startCursorPos;
		const Vector2 newPos = startWindowPos + delta;
		client.position = Vector2(newPos.x, newPos.y);
		XMoveWindow(
				_display,
				client.border,
				newPos.x,
				newPos.y);

	} else if(e.state & Button3Mask) { //Resize window
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
	assert(XGetWindowAttributes(_display, w, &attrs) );

	if(createdBefore) {
		if(attrs.override_redirect || attrs.map_state != IsViewable) {
			return;
		}
	}

	if(attrs.y < _upperBorder) {
		attrs.y = _upperBorder;
	}

	if(const int dy = attrs.height + _lowerBorder + attrs.y - _screen->height; 
			dy > 0) {
		attrs.height -= dy;
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
	_clients.push_back({
		w, 
		frame, 
		_currentWorkspace,
		{attrs.x, attrs.y},
		{attrs.width, attrs.height},
		{attrs.x, attrs.y},
		false
	});

	//Grab Alt + LMB
	XGrabButton(
			_display,
			Button1,
			modifierMask,
			w,
			False,
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
			False,
			ButtonPressMask | ButtonMotionMask,
			GrabModeAsync,
			GrabModeAsync,
			None,
			None);

	LogDebug << "Framed window " << w << " [" << frame << "]\n";
}

void WindowManager::unframe(const Client &client) {

	XUnmapWindow(_display, client.border);

	XDestroyWindow(_display, client.border);
	erase(client.window);
	focusNext();

	LogDebug << "Unframed Window" << client.window << " [" << client.border << "]\n";
}

void WindowManager::switchWorkspace(int workspace) {
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
}

void WindowManager::show(const Client &client) {
	XMoveWindow(
		_display,
		client.border,
		client.restore.x,
		client.restore.y);
}

void WindowManager::kill(const Client &client) {
	XEvent ev;
    ev.type = ClientMessage;
    ev.xclient.window = client.window;
    ev.xclient.message_type = _atomWMProtocols;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = _atomDeleteWindow;
    ev.xclient.data.l[1] = CurrentTime;
    XSendEvent(_display, client.window, False, NoEventMask, &ev);
	focusNext();
}

void WindowManager::moveClient(Client &client, int workspace) {
	if(client.workspace == workspace) return;
	client.workspace = workspace;
	hide(client);
	focusNext();
}

void WindowManager::zoomClient(Client &client) {
	constexpr int border2W = static_cast<int>(borderWidth << 1);
	Vector2 size = client.fullscreen ? client.size 
		: Vector2{_screen->width - border2W, _screen->height 
			- border2W - (_lowerBorder + _upperBorder)};
	Vector2 position = client.fullscreen ? client.position 
		: Vector2(0, _upperBorder);

	XResizeWindow(
			_display, 
			client.border, 
			size.x, 
			size.y);

	XResizeWindow(
			_display, 
			client.window, 
			size.x, 
			size.y);

	XMoveWindow(
			_display,
			client.border,
			position.x,
			position.y);

	client.fullscreen ^= 1;
}

void WindowManager::registerDock(Window w) {
	XWindowAttributes xattr;
	XGetWindowAttributes(_display, w, &xattr);

	unsigned int lowerRadius = xattr.height,
				 upperRadius = xattr.height;

	if(xattr.y == 0) {
		lowerRadius = _lowerBorder;
	} else {
		upperRadius = _upperBorder;
	}

	setOuterBorder(upperRadius, lowerRadius);
}

void WindowManager::setOuterBorder(int upper, int lower) {
	_upperBorder = upper, _lowerBorder = lower;
}

void WindowManager::printLayout() const {
	constexpr std::string_view current = "[*]", other = "[ ]";
	auto p = [&](WindowManager::Ws ws) {
		return static_cast<int>(ws) == _currentWorkspace ? current : other;
	};

	LogDebug << "Layout:\n" 
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
