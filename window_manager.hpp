#pragma once
#ifndef WINDOW_MANAGER_HPP
#define WINDOW_MANAGER_HPP

extern "C" {
	#include <X11/Xlib.h>
}

#include "vector2.hpp"

#include <unordered_map>
#include <functional>
#include <memory>
#include <vector>


struct Client {
	Window window;
	Window border;	//Handle to border
	int workspace;
	Vector2 restore;
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

		void focus(Client &client);
		void focusNext();
		void frame(Window w, bool createdBefore);
		void unframe(const Client &client);
		void switchWorkspace(int index);
		void hide(Client &meta);
		void show(const Client &meta);
		void initKeys();

		void printLayout() const;

		enum Direction {
			Left = 0,
			Right,
			Up,
			Down
		};

		enum Ws {
			Center = 0, 
			West, 
			East, 
			North, 
			South,
			N
		};

		constexpr static int nWorkspaces = static_cast<int>(Ws::N);

		constexpr int workspaceMap(Direction dir) const;

		std::vector<Client> _clients;
		//Helper functions
		bool exists(Window w) const;
		Client &find(Window w);
		void erase(Window w);

		std::unordered_map<KeyCode, std::function<void(unsigned int)> > _binds;

		Vector2 startCursorPos, startWindowPos, startWindowSize;

		Display *_display;
		Screen *_screen;
		const Window _root;
		static bool _wmDetected;

		int _currentWorkspace = 0;

		constexpr static auto ModifierMask = Mod1Mask;
		constexpr static unsigned int borderWidth = 3;
		constexpr static unsigned long borderColor = 0xff00ff;
		constexpr static unsigned long bgColor = 0x000000;
};

#endif
