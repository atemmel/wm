#include "window_manager.hpp"
#include <glog/logging.h>

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

	/*	Loop	*/
	while(true) {
		XEvent e;
		XNextEvent(_display, &e);
		LOG(INFO) << "Recieved event: " << std::to_string(e.type);

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
			case CreateNotify:
			case DestroyNotify:
			case ReparentNotify:
			case ConfigureNotify:
			case MapNotify:
			default:
				LOG(WARNING) << "Ignored event";
		}
	}
}

int WindowManager::onXError(Display *display, XErrorEvent *e) { return 0; }

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
		const Window frame = _clients[e.window];
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
	const Window frame = _clients[e.window];
	LOG(INFO) << "Click in window " << std::to_string(e.window);

	clickStart = {e.x_root, e.y_root};

	Window returned;
	Vector2 pos;
	unsigned int w, h, border, depth;

	XGetGeometry(
			_display,
			frame,
			&returned,
			&pos.x, &pos.y,
			&w, &h,
			&border,
			&depth);

	focus(e.window);
}

void WindowManager::onFocusIn(const XFocusChangeEvent &e) {
	std::cerr << "Focus changed" << e.window << '\n';
}

void WindowManager::onEnterNotify(const XEnterWindowEvent &e) {
	LOG(INFO) << "Entered window " << std::to_string(e.window);
	focus(e.window);
}

void WindowManager::focus(Window w) {
	XRaiseWindow(_display, w);
	XSetInputFocus(_display, w, RevertToParent, CurrentTime);
}

void WindowManager::frame(Window w, bool createdBefore) {
	constexpr unsigned int borderWidth = 3;
	constexpr unsigned long borderColor = 0xff00ff;
	constexpr unsigned long bgColor = 0x000000;

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
	_clients[w] = frame;

	XGrabButton(
			_display,
			Button1,
			None,
			w,
			false,
			ButtonPressMask,
			GrabModeAsync,
			GrabModeAsync,
			None,
			None);

	LOG(INFO) << "Framed window " << w << " [" << frame << ']';
}

void WindowManager::unframe(Window w) {
	const Window frame = _clients[w];

	XUnmapWindow(_display, frame);

	XReparentWindow(
			_display,
			w,
			_root,
			0, 0);
	XRemoveFromSaveSet(_display, w);
	XDestroyWindow(_display, frame);
	_clients.erase(w);

	LOG(INFO) << "Unframed Window" << w << " [" << frame << ']';
}
