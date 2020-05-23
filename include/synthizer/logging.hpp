#pragma once
#include <string>

namespace synthizer {

void logToStderr();
void disableLogging();
bool isLoggingEnabled();

/*
 * Overloads which know how to log a specific thing to whatever destination is currently set.
 * 
 * Don't use these directly.
 * */
void logFragment(const char *message);
void logFragment(const std::string &message);

template <typename FIRST>
void logInner(FIRST &&f) {
	logFragment(f);
	logFragment("\n");
}

template<typename FIRST, typename... T>
void logInner(FIRST &&f, T&& ...args) {
	logFragment(f);
	logFragment(" ");
	logInner(args...);
}

/* Log set of space-separated fragments. */
template<typename... T>
void log(T&& ...args) {
	if(isLoggingEnabled())
		logInner(args...);
}

}
