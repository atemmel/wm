#define LOG_DEBUG 0
#define LOG_ERROR 1
#include "logger.hpp"
#include "window_manager.hpp"

int main(int argc, char **argv) {

	auto wm = WindowManager::create();
	std::cout << LOG_ERROR << '\n';

	if(!wm) {
		LoggerError << "Failed to initialize window manager.";
		return EXIT_FAILURE;
	}

	wm->run();

	return EXIT_SUCCESS;
}
