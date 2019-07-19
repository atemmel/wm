#pragma once
#ifndef WINDOW_MANAGER_HPP
#define WINDOW_MANAGER_HPP

extern "C" {
	#include <X11/Xlib.h>
}

#include <unordered_map>
#include <memory>

//TODO: Move into separate implementation
struct Vector2 {
	constexpr Vector2(int _x = 0, int _y = 0) 
		: x(_x), y(_y) {}

	constexpr Vector2 operator+(const Vector2 lhs) const {
		return {x + lhs.x, y + lhs.y};
	}

	constexpr Vector2 operator-(const Vector2 lhs) const {
		return {x - lhs.x, y - lhs.y};
	}

	int x, y;
};

class WindowManager {
	public:
		static std::unique_ptr<WindowManager> create();
		~WindowManager();
		void run();
	private:
		WindowManager(Display *display);
		static int onXError(Display *display, XErrorEvent *e);
		static int onWmDetected(Display *display, XErrorEvent *e);

		void onConfigureRequest(const XConfigureRequestEvent &e);
		void onMapRequest(const XMapRequestEvent &e);
		void onUnmapNotify(const XUnmapEvent &e);
		void onButtonPress(const XButtonEvent &e);
		void onFocusIn(const XFocusChangeEvent &e);
		void onEnterNotify(const XEnterWindowEvent &e);
		void onMotionNotify(const XMotionEvent &e);

		void focus(Window w);
		void frame(Window w, bool createdBefore);
		void unframe(Window w);

		//Map from each window to their respective border
		std::unordered_map<Window, Window> _clients;

		Vector2 startCursorPos, startWindowPos, startWindowSize;

		Display *_display;
		const Window _root;
		static bool _wmDetected;

};

#endif
