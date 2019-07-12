#pragma once
#ifndef WINDOW_MANAGER_HPP
#define WINDOW_MANAGER_HPP

extern "C" {
	#include <X11/Xlib.h>
}

#include <memory>

class WindowManager {
	public:
		static std::unique_ptr<WindowManager> create();
		~WindowManager();
		void run();
	private:
		WindowManager(Display *display);
		static int onXError(Display *display, XErrorEvent *e);
		static int onWmDetected(Display *display, XErrorEvent *e);

		void onCreateNotify(const XCreateWindowEvent &e);
		void onReparentNotify(const XReparentEvent &e);
		void onConfigureRequest(const XConfigureRequestEvent &e);
		void onConfigureNotify(const XConfigureEvent &e);
		void onMapRequest(const XMapRequestEvent &e);
		void onMapNotify(const XMapEvent &e);
		void onUnmapNotify(const XUnmapEvent &e);

		Display *_display;
		static bool _wmDetected;
		const Window _root;

};

#endif
