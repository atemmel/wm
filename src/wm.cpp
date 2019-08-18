#include "window_manager.hpp"
#include <glog/logging.h>

int main(int argc, char **argv) {
	::google::InitGoogleLogging(argv[0]);

	auto wm = WindowManager::create();

	if(!wm) {
		LOG(ERROR) << "Failed to initialize window manager.";
		return EXIT_FAILURE;
	}

	wm->run();

	return EXIT_SUCCESS;
}
