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
	Window window;		//Handle to window
	Window border;		//Handle to border
	int workspace;		//Workspace index
	Vector2 restore;	//"Old" coordinates
	Vector2 size;		//Dimension
	Vector2 position;	//Positon
	bool fullscreen = false;
};

class WindowManager {
	public:
		static std::unique_ptr<WindowManager> create();
		~WindowManager();
		void run();

		//Enums
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
	private:

		//Constants
		constexpr static int nWorkspaces = static_cast<int>(Ws::N);
		constexpr static auto modifierMask = Mod1Mask;
		constexpr static unsigned int borderWidth = 5;
		constexpr static unsigned long borderColor = 0x7D78A3;
		constexpr static unsigned long bgColor = 0x000000;

		//Init
		WindowManager(Display *display);
		static int onXError(Display *display, XErrorEvent *e);
		static int onWmDetected(Display *display, XErrorEvent *e);

		//Event handlers
		void onConfigureRequest(const XConfigureRequestEvent &e);
		void onMapRequest(const XMapRequestEvent &e);
		void onUnmapNotify(const XUnmapEvent &e);
		void onButtonPress(const XButtonEvent &e);
		void onFocusIn(const XFocusChangeEvent &e);
		void onEnterNotify(const XEnterWindowEvent &e);
		void onMotionNotify(const XMotionEvent &e);

		//Basic functions
		void focus(Client &client);
		void focusNext();
		void frame(Window w, bool createdBefore);
		void unframe(const Client &client);
		void switchWorkspace(int workspace);
		void hide(Client &client);
		void show(const Client &client);
		void kill(const Client &client);
		void moveClient(Client &client, int workspace);
		void zoomClient(Client &client);
		constexpr int workspaceMap(Direction dir) const;

		//Helper functions
		void printLayout() const;
		bool exists(Window w) const;
		void erase(Window w);
		Client &find(Window w);

		//Containers
		std::vector<Client> _clients;

		//Atoms
		Atom _atomDeleteWindow;
		Atom _atomWMProtocols;

		Atom _atomWMWindowType;
		Atom _atomWMWindowDock;
		Atom _atomWMWindowToolbar;
		Atom _atomWMWindowUtility;
		Atom _atomWMWindowDialog;
		Atom _atomWMWindowMenu;

		//Near-primitives
		Vector2 startCursorPos, startWindowPos, startWindowSize;
		Display *_display;
		const Window _root;
		Screen *_screen;
		Client *_focused;
		static bool _wmDetected;
		int _currentWorkspace = 0;
};

#endif
