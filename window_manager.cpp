#include "window_manager.hpp"
#include <glog/logging.h>

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
	/*	Init	*/
	_wmDetected = false;
	//Set temporary errror handler
	XSetErrorHandler(&WindowManager::onWmDetected);
	XSelectInput(
			_display,
			_root,
			SubstructureRedirectMask | SubstructureNotifyMask);
	XSync(_display, false);	//Flush errors
	if(_wmDetected) {
		LOG(ERROR) << "Detected another window manager on display " << XDisplayString(_display);
		return;
	}
	//Set regular error handler
	XSetErrorHandler(&WindowManager::onXError);

	/*	Loop	*/
	while(true) {
		XEvent e;
		XNextEvent(_display, &e);
		LOG(INFO) << "Recieved event: " << ToString(e);

		switch(e.type) {
			case CreateNotify:
				onCreateNotify(e.xcreatewindow);
				break;
			case DestroyNotify:
				onDestroyNotify(e.xdestroywindow);
				break;
			case ReparentNotify:
				onReparentNotify(e.xreparent);
				break;
			case ConfigureRequest:
				onConfigureRequest(e.xconfigurerequest);
				break;
			case ConfigureNotify:
				onConfigureNotify(e.xconfigure);
				break;
			case MapRequest:
				onMapRequest(e.xmaprequest);
				break;
			case MapNotify:
				onMapNotify(e.xmap);
				break;
			case UnmapNotify:
				onUnmapNotify(e.xunmap);
				break;
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

void WindowManager::onCreateNotify(const XCreateWindowEvent &e) {
	
}

void WindowManager::onReparentNotify(const XCreateWindowEvent &e) {

}

void WindowManager::onConfigureRequest(const XConfigureRequestEvent &e) {
	XWindowChanges changes;

	changes.x = e.x;
	changes.y = e.y;
	changes.width = e.width;
	changes.height = e.height;
	changes.sibling = e.abobe;
	changes.stack_mode = e.detail;

	if(_clients.count(e.window)) {
		const Window frame = _clients[e.window];
		XConfigureWindow(_display, e.window, e.value_mask, &changes);
		LOG(INFO) << "Resize " << e.window << " to " << Size<int>(e.width, e.height);
	}
}

void WindowManager::onConfigureNotify(const XConfigureEvent &e) {

}

void WindowManager::onMapRequest(const XMapRequestEvent &e) {
	Frame(e.window);
	XMapWindow(_display, e.window);
}

void WindowManager::onMapNotify(const XMapEvent &e) {

}

void WindowManager::onUnmapNotify(const XUnmapEvent &e) {
	if(!_clients.count(e.window) ) {
		LOG(INFO) << "Ignore UnmapNotify for non-client window " << e.window;
		return;
	}

	unframe(e.window);
}

void WindowManager::frame(Window w) {
	constexpr unsigned int borderWidth = 3;
	constexpr unsigned long borderColor = 0xff0000;
	constexpr unsigned long bgColor = 0x0000ff;

	XWindowAttributes attrs;
	CHECK(XGetWindowAttributes(_display, w, &attrs) );

	const Window frame = XCreateSimpleWindow(
			_display,
			_root,
			attrs.x,
			attrs.y,
			attrs.w,
			attrs.h,
			borderWidth,
			borderColor,
			bgColor);

	XSelectInput(
			_display,
			frame,
			SubstructureRedirectMask | SubstructureNotifyMask);

	XAddToSaveSet(_display, w);
	_clients[w] = frame;

	//XGrabButton();
	//XGrabButtin();
	//XGrabKey();
	//XGrabKey();

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

