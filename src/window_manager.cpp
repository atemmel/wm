#define LOG_DEBUG 0
#define LOG_ERROR 1

#include "window_manager.hpp"
#include "event.hpp"
#include "log.hpp"

#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include <iostream>
#include <cassert>
#include <cstring>
#include <string>
#include <array>

using Clients = WindowManager::Clients;

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
	: _display(display), _root(DefaultRootWindow(_display) ), 
	_check(XCreateSimpleWindow(_display, _root, 0, 0, 1, 1, 0, 0, 0) ),
	_focused(nullptr),
	_netAtoms(_display),
	_iccAtoms(_display),
	_otherAtoms(_display) {
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

	LogDebug << "Mapping toplevel windows:\n";
	for(unsigned int i = 0; i < n_topLevel; i++) {
		LogDebug << i << " : " << topLevel[i] << '\n';
		frame(topLevel[i], true);
	}
	LogDebug << "Mapped " << n_topLevel << " toplevel windows\n";

	XFree(topLevel);
	XUngrabServer(_display);

	_screen = XDefaultScreenOfDisplay(_display);

	//Set wm check window
	XChangeProperty(_display, _check, _netAtoms.WMCheck, XA_WINDOW, 32, PropModeReplace,
			reinterpret_cast<const unsigned char*>(&_check), 1);

	//Set wm name
	XChangeProperty(_display, _root, _netAtoms.WMName, _otherAtoms.utf8str, 8, PropModeReplace,
			reinterpret_cast<const unsigned char*>("wm"), strlen("wm") );

	//Restore wm check window
	XChangeProperty(_display, _root, _netAtoms.WMCheck, XA_WINDOW, 32, PropModeReplace,
			reinterpret_cast<const unsigned char*>(&_check), 1);

	//Set all supported net atoms
	XChangeProperty(_display, _root, _netAtoms.supported, XA_ATOM, 32, PropModeReplace,
			reinterpret_cast<const unsigned char*>(&_netAtoms), _netAtoms.size() );

	//Set number of desktops
	unsigned long data = 5;
	XChangeProperty(_display, _root, _netAtoms.numberOfDesktops, XA_CARDINAL, 32, 
			PropModeReplace, reinterpret_cast<unsigned char*>(&data), 1);

	data = 0;
	XChangeProperty(_display, _root, _netAtoms.currentDesktop, XA_CARDINAL, 32,
			PropModeReplace, reinterpret_cast<unsigned char*>(&data), 1);

	//IPC event table
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
				_running = false;
			},
			[&](long *arg) {	//Focus next
				LogDebug << "Focus next\n";
				focusNext();
			},
			[&](long *arg) {	//Focus prev
				LogDebug << "Focus prev\n";
				focusPrev();
			}
		};

	LogDebug << "All clear, wm starting\n";
	/*	Loop	*/
	while(_running) {
		XEvent e;
		XNextEvent(_display, &e);

		LogDebug << "Clients:\n";
		for(const auto &c : _clients) {
			LogDebug << '\t' << c.window << '\n';
		}
		LogDebug << "Total clients: " << _clients.size() << '\n';

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

	for(const auto &c : _clients) {
		unframe(c);
	}
}

int WindowManager::onXError(Display *display, XErrorEvent *e) { 
	std::string str(256, '\0');
	XGetErrorText(display, e->error_code, str.data(), str.size( ));
	LogDebug << "X error: " << static_cast<int>(e->error_code) 
		<< " : " << static_cast<int>(e->request_code)
		<< " : " << static_cast<int>(e->resourceid) << '\n';
	LogDebug << "XGetErrorText: " << str << '\n';
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

	if(auto client = find(e.window); client != _clients.end() ) {
		client->size = Vector2{e.width, e.height};
		client->position = Vector2{e.x, e.y};
		XConfigureWindow(_display, e.window, e.value_mask, &changes);
		LogDebug << "Resize " << e.window << " to w:" 
			<< e.width << " h:" << e.height << '\n';

	}
}

void WindowManager::onMapRequest(const XMapRequestEvent &e) {
	LogDebug << "Attempting to map " << e.window << '\n';
	bool framed = frame(e.window, false);
	XMapWindow(_display, e.window);
	if(framed) {
		focus(*find(e.window) );
	}
}

void WindowManager::onUnmapNotify(const XUnmapEvent &e) {
	auto client = find(e.window);
	if(client == _clients.end() ) {
		LogDebug << "Ignore UnmapNotify for non-client window " << e.window << '\n';
		return;
	}

	unframe(*client);
}

void WindowManager::onButtonPress(const XButtonEvent &e) {
	auto client = find(e.window);
	LogDebug << "Click in window " << e.window << '\n';

	startCursorPos = {e.x_root, e.y_root};

	Window returned;
	Vector2 pos;
	unsigned int w, h, border, depth;

	XGetGeometry(
			_display,
			client->window,
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
	if(_focused && _focused->fullscreen) {
		return;
	}
	focus(*find(e.window));
}

void WindowManager::onMotionNotify(const XMotionEvent &e) {
	const Vector2 cursorPos = {e.x_root, e.y_root};
	auto client = find(e.window);

	if(client->fullscreen) return;

	if(e.state & Button1Mask) {	//Move window
		const Vector2 delta = cursorPos - startCursorPos;
		const Vector2 newPos = startWindowPos + delta;
		client->position = Vector2(newPos.x, newPos.y);
		XMoveWindow(
				_display,
				client->window,
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
	}
}

void WindowManager::focus(Client &client) {
	XDeleteProperty(_display, _root, _netAtoms.activeWindow);
	LogDebug << "Deleting activeWindow property\n";
	_focused = &client;
	XChangeProperty(_display, _root, _netAtoms.activeWindow, XA_WINDOW, 32, PropModeReplace,
			reinterpret_cast<unsigned char*>(&client.window), 1);
	LogDebug << "Changing activeWindow property\n";
	XRaiseWindow(_display, client.window);
	XSetInputFocus(_display, client.window, RevertToParent, CurrentTime);
}

void WindowManager::focusLast() {
	for(auto it = _clients.rbegin(); it != _clients.rend(); it++) {
		if(it->workspace == _currentWorkspace) {
			focus(*it);
			return;
		}
	}

	_focused = nullptr;
	XDeleteProperty(_display, _root, _netAtoms.activeWindow);
	XSetInputFocus(_display, _root, RevertToPointerRoot, CurrentTime);
}

void WindowManager::focusNext() {
	if(!_focused) return;

	int diff = _focused - &*_clients.begin();
	for(auto it = _clients.begin() + diff + 1; true; it++) {
		if(it >= _clients.end() ) it = _clients.begin();
		if(it->workspace == _currentWorkspace) {
			focus(*it);
			return;
		}
	}
}

void WindowManager::focusPrev() {
	if(!_focused) return;

	int diff = _focused - &*_clients.begin();
	for(auto it = _clients.begin() + diff - 1; true ; it--) {
		if(it < _clients.begin() ) it = _clients.end() - 1;
		if(it->workspace == _currentWorkspace) {
			focus(*it);
			return;
		}
	}

}

bool WindowManager::frame(Window w, bool createdBefore) {

	XWindowAttributes attrs;
	assert(XGetWindowAttributes(_display, w, &attrs) );

	if(createdBefore) {
		if(attrs.override_redirect || attrs.map_state != IsViewable) {
			LogDebug << "Window " << w << " not managed\n";
			return false;
		}
	}

	Atom* prop = getWindowProperty(w);

	if(prop) {
		LogDebug << "Window " << w << " is _NET_WM_WINDOW_DOCK: " << std::boolalpha <<
			(*prop == _netAtoms.WMWindowDock) << '\n';
		LogDebug << "Window " << w << " is _NET_WM_WINDOW_TOOLBAR: " << std::boolalpha <<
			(*prop == _netAtoms.WMWindowToolbar) << '\n';
		LogDebug << "Window " << w << " is _NET_WM_WINDOW_UTILITY: " << std::boolalpha <<
			(*prop == _netAtoms.WMWindowUtility) << '\n';
		LogDebug << "Window " << w << " is _NET_WM_WINDOW_MENU: " << std::boolalpha <<
			(*prop == _netAtoms.WMWindowMenu) << '\n';

		if(*prop == _netAtoms.WMWindowDock) {
				registerDock(w);
		}

		if(*prop == _netAtoms.WMWindowDock ||
				*prop == _netAtoms.WMWindowToolbar ||
				*prop == _netAtoms.WMWindowUtility ||
				*prop == _netAtoms.WMWindowMenu) {
			LogDebug << "Window " << w << " not managed\n";
			XFree(prop);
			return false;	//Do not manage the window
		}

		XFree(prop);	//Only free what is not nullptr
	} 

	if(attrs.y < _upperBorder) {
		attrs.y = _upperBorder;
	}

	if(const int dy = attrs.height + _lowerBorder + attrs.y - _screen->height; 
			dy > 0) {
		attrs.height -= dy;
	}

	XMoveWindow(
		_display,
		w,
		attrs.x,
		attrs.y);

	XResizeWindow(
		_display, 
		w, 
		attrs.width, 
		attrs.height);


	XSelectInput(
			_display,
			w,
			SubstructureRedirectMask | SubstructureNotifyMask);

	XSelectInput(
			_display,
			w,
			EnterWindowMask);

	_clients.push_back({
		w, 
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

	LogDebug << "Framed window: " << w << '\n';
	return true;
}

void WindowManager::unframe(const Client &client) {
	erase(client.window);
	focusLast();
	LogDebug << "Unframed Window: " << client.window << '\n';
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
	
	unsigned long data = static_cast<unsigned long>(workspace);
	XChangeProperty(_display, _root, _netAtoms.currentDesktop, XA_CARDINAL, 32,
			PropModeReplace, reinterpret_cast<unsigned char*>(&data), 1);

	focusLast();
	printLayout();
}

void WindowManager::hide(Client &client) {
	XWindowAttributes xattr;
	XGetWindowAttributes(_display, client.window, &xattr);

	client.restore = {xattr.x, xattr.y};

	//Big brain window hide
	XMoveWindow(
		_display,
		client.window,
		xattr.x + _screen->width,
		xattr.y + _screen->height);
}

void WindowManager::show(const Client &client) {
	XMoveWindow(
		_display,
		client.window,
		client.restore.x,
		client.restore.y);
}

void WindowManager::kill(const Client &client) {
	XEvent ev;
    ev.type = ClientMessage;
    ev.xclient.window = client.window;
    ev.xclient.message_type = _iccAtoms.WMProtocols;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = _iccAtoms.DeleteWindow;
    ev.xclient.data.l[1] = CurrentTime;
    XSendEvent(_display, client.window, False, NoEventMask, &ev);
	focusLast();
}

void WindowManager::moveClient(Client &client, int workspace) {
	if(client.workspace == workspace) return;
	client.workspace = workspace;
	hide(client);
	focusLast();
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
			client.window, 
			size.x, 
			size.y);

	XMoveWindow(
			_display,
			client.window,
			position.x,
			position.y);

	client.fullscreen ^= 1;
}

Atom* WindowManager::getWindowProperty(Window w) const {
	//Data ptr
	unsigned char *propStr = nullptr;
	//Dummy variables
	int di;
	unsigned long dl;
	Atom da;

	bool status = XGetWindowProperty(_display, w, _netAtoms.WMWindowType, 0, sizeof(Atom), 
			False, XA_ATOM, &da, &di, &dl, &dl, &propStr) == Success;

	if(status && propStr) {
		//Don't forget to free!
		return reinterpret_cast<Atom*>(propStr);
	} 

	return nullptr;
}

void WindowManager::registerDock(Window w) {
	XWindowAttributes xattr;
	XGetWindowAttributes(_display, w, &xattr);

	if(xattr.y == 0) {
		_upperBorder = xattr.height;
	} else {
		_lowerBorder = xattr.height;
	}
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

Clients::iterator WindowManager::find(Window w) {
	return std::find_if(_clients.begin(), _clients.end(), [&](const Client &client) {
				return client.window == w;
		});
}

void WindowManager::erase(Window w) {
	_clients.erase(std::remove_if(_clients.begin(), _clients.end(), [&](const Client &client) {
					return client.window == w;
		}));
}
