#include <string>
#include <cstdio>
#include <atomic>

#include "synthizer/logging.hpp"

namespace synthizer {

/* This implementation is not sufficient for production usage; TODO: log from a thread once we have a lockfree/waitfree queue. */

static std::atomic<int> logging_enabled{0};

bool isLoggingEnabled() {
	return logging_enabled.load() == 1;
}

void disableLogging() {
	logging_enabled.store(0);
}

void logToStderr() {
	logging_enabled.store(1);
}

void logFragment(const char *message) {
	std::fprintf(stderr, "%s", message);
}

void logFragment(const std::string &message) {
	logFragment(message.c_str());
}

}