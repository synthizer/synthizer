#include <thread>
#include <vector>
#include <shared_mutex>
#include <mutex>
#include <iostream>
#include <sstream>
#include <iterator>
#include <map>

#include "synthizer/byte_stream.hpp"
#include "synthizer/threadsafe_initializer.hpp"

namespace synthizer {

/*
 * Infrastructure for the registry.
 * */
static ThreadsafeInitializer<std::shared_mutex> byte_stream_registry_lock{};
static std::map<std::string, std::function<ByteStream *(const std::string &, std::vector<std::tuple<std::string, std::string>>)>> byte_stream_registry;

void registerByteStreamProtocol(std::string &name, std::function<ByteStream*(const std::string &, std::vector<std::tuple<std::string, std::string>>)> factory) {
	auto guard = std::lock_guard(byte_stream_registry_lock.get());
	if (byte_stream_registry.count(name))
		throw new UnsupportedByteStreamOperationError("Attempted duplicate registry of protocol "+name);

	byte_stream_registry[name] = factory;
}

static std::vector<std::tuple<std::string, std::string>> parseOptions(const std::string & options) {
	std::istringstream opts_stream{options};
	std::vector<std::tuple<std::string, std::string>> parsed;
	std::string current_opt;
	while (std::getline(opts_stream, current_opt, ' ')) {
		std::istringstream inner{current_opt};
		std::string k;
		std::getline(inner, k, '=');
		if (k.size() ==  0)
			continue;
		std::istream_iterator<char> begin {inner};
		std::istream_iterator<char> end{};
		std::string v{begin, end};
		parsed.emplace_back(k, v);
	}
	return parsed;
}

ByteStream* getStreamForProtocol(const std::string &protocol, const std::string &path, const std::string &options) {
	auto parsed = parseOptions(options);
	auto l = std::shared_lock{byte_stream_registry_lock.get()};
	std::lock_guard guard{l};
	if (byte_stream_registry.count(protocol) == 0)
		throw UnsupportedByteStreamOperationError("Unregistered protocol " + protocol);
	auto &f = byte_stream_registry[protocol];
	return f(path, parsed);
}

}
