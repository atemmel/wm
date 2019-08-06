#include "window_manager.hpp"

#include <glog/logging.h>	/* TODO: Remove dependency */
#include <X11/Xutil.h>

#include <iostream>
#include <string>

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
	: _display(display), _root(DefaultRootWindow(_display) ) {
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

	Screen *screen = XDefaultScreenOfDisplay(_display);

	KeyCode left = XKeysymToKeycode(_display, XK_H),
			right = XKeysymToKeycode(_display, XK_L);

	XGrabKey(_display,
			left,
			AnyModifier,
			_root,
			False,
			GrabModeAsync,
			GrabModeAsync);

	XGrabKey(_display,
			right,
			AnyModifier,
			_root,
			False,
			GrabModeAsync,
			GrabModeAsync);

	_binds.insert(
			{left, 
			[&]() {
				if(_currentWorkspace > 0) {
					std::cout << "Switching to workspace "
						<< _currentWorkspace - 1 << '\n';
					switchWorkspace(_currentWorkspace - 1);
				}
			}});

	_binds.insert(
			{right, 
			[&]() {
				if(_currentWorkspace < 3) {
					std::cout << "Switching to workspace "
						<< _currentWorkspace + 1 << '\n';
					switchWorkspace(_currentWorkspace + 1);
				}
			}});

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
				if(e.xkey.state & ModifierMask) {
					if(auto it = _binds.find(e.xkey.keycode); it != _binds.end() ) {
						it->second();
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
	std::cout << "X error: " << e->error_code << " : " << std::hex << e->error_code << '\n';
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

	if(_clients.count(e.window)) {
		//const Window frame = _clients[e.window];
		XConfigureWindow(_display, e.window, e.value_mask, &changes);
		LOG(INFO) << "Resize " << e.window << " to w:" 
			<< std::to_string(e.width) << " h:" << std::to_string(e.height);
	}
}

void WindowManager::onMapRequest(const XMapRequestEvent &e) {
	frame(e.window, false);
	XMapWindow(_display, e.window);
	focus(e.window);
}

void WindowManager::onUnmapNotify(const XUnmapEvent &e) {
	if(e.event == _root) {
		LOG(INFO) << "Ignore UnmapNotify for reparented pre-existing window " << e.window;
		return;
	}

	if(!_clients.count(e.window) ) {
		LOG(INFO) << "Ignore UnmapNotify for non-client window " << e.window;
		return;
	}

	unframe(e.window);
}

void WindowManager::onButtonPress(const XButtonEvent &e) {
	const WindowMeta meta = _clients[e.window];
	LOG(INFO) << "Click in window " << std::to_string(e.window);

	startCursorPos = {e.x_root, e.y_root};

	Window returned;
	Vector2 pos;
	unsigned int w, h, border, depth;

	XGetGeometry(
			_display,
			meta.border,
			&returned,
			&pos.x, &pos.y,
			&w, &h,
			&border,
			&depth);

	startWindowPos = pos;
	startWindowSize = {
		static_cast<int>(w), 
		static_cast<int>(h)};

	//focus(e.window);
}

void WindowManager::onFocusIn(const XFocusChangeEvent &e) {
	std::cerr << "Focus changed" << e.window << '\n';
}

void WindowManager::onEnterNotify(const XEnterWindowEvent &e) {
	LOG(INFO) << "Entered window " << std::to_string(e.window);
	focus(e.window);
}

void WindowManager::onMotionNotify(const XMotionEvent &e) {
	//LOG(INFO) << "Motion in window " << std::to_string(e.window);

	const Vector2 cursorPos = {e.x_root, e.y_root};
	const WindowMeta meta = _clients[e.window];

	if(e.state & Button1Mask) {	//Move window
		const Vector2 delta = cursorPos - startCursorPos;
		const Vector2 newPos = startWindowPos + delta;
		XMoveWindow(
				_display,
				meta.border,
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
				meta.border,
				newSize.x, newSize.y);
	}
}

void WindowManager::focus(Window w) {
	XRaiseWindow(_display, w);
	XSetInputFocus(_display, w, RevertToParent, CurrentTime);
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

	XAddToSaveSet(_display, w);

	XReparentWindow(
			_display,
			w,
			frame,
			0, 0);

	XMapWindow(_display, frame);
	_clients[w] = {frame, _currentWorkspace };

	//Grab Alt + LMB
	XGrabButton(
			_display,
			Button1,
			ModifierMask,
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
			ModifierMask,
			w,
			false,
			ButtonPressMask | ButtonMotionMask,
			GrabModeAsync,
			GrabModeAsync,
			None,
			None);

	LOG(INFO) << "Framed window " << w << " [" << frame << ']';
}

void WindowManager::unframe(Window w) {
	const WindowMeta meta = _clients[w];

	XUnmapWindow(_display, meta.border);

	XReparentWindow(
			_display,
			w,
			_root,
			0, 0);
	XRemoveFromSaveSet(_display, w);
	XDestroyWindow(_display, meta.border);
	_clients.erase(w);

	LOG(INFO) << "Unframed Window" << w << " [" << meta.border << ']';
}

void WindowManager::switchWorkspace(int index) {
	XWindowAttributes xattr;

	for(auto &pair : _clients) {
		if(pair.second.workspace == _currentWorkspace) {
			hide(pair.second);
		}
	}

	_currentWorkspace = index;

	for(auto &pair : _clients) {
		if(pair.second.workspace == _currentWorkspace) {
			show(pair.second);
		}
	}
}

void WindowManager::hide(WindowMeta &meta) {
	XWindowAttributes xattr;
	XGetWindowAttributes(_display, meta.border, &xattr);

	meta.restore = {xattr.x, xattr.y};

	//Big brain window hide
	XMoveWindow(
		_display,
		meta.border,
		_screen->width,
		xattr.y);
};

void WindowManager::show(WindowMeta &meta) {
	XWindowAttributes xattr;

	XMoveWindow(
			_display,
			meta.border,
			meta.restore.x,
			meta.restore.y);
}
