#include <thread>
#include <array>
#include <vector>
#include <shared_mutex>
#include <mutex>
#include <memory>
#include <iostream>
#include <sstream>
#include <iterator>
#include <map>
#include <list>
#include <assert.h>

#include "synthizer/byte_stream.hpp"
#include "synthizer/threadsafe_initializer.hpp"

namespace synthizer {

/*
 * Infrastructure for the registry.
 * */
static ThreadsafeInitializer<std::shared_mutex> byte_stream_registry_lock{};
static std::map<std::string, std::function<std::shared_ptr<ByteStream> (const std::string &, std::vector<std::tuple<std::string, std::string>>)>> byte_stream_registry;

void registerByteStreamProtocol(std::string &name, std::function<std::shared_ptr<ByteStream> (const std::string &, std::vector<std::tuple<std::string, std::string>>)> factory) {
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

std::shared_ptr<ByteStream> getStreamForProtocol(const std::string &protocol, const std::string &path, const std::string &options) {
	auto parsed = parseOptions(options);
	auto l = std::shared_lock{byte_stream_registry_lock.get()};
	std::lock_guard guard{l};
	if (byte_stream_registry.count(protocol) == 0)
		throw UnsupportedByteStreamOperationError("Unregistered protocol " + protocol);
	auto &f = byte_stream_registry[protocol];
	return f(path, parsed);
}

template<typename T>
class ForwardingStream: public T {
	public:
	ForwardingStream(std::shared_ptr<ByteStream> stream): stream(stream) {}

	std::string getName() { return this->stream->getName(); }
	std::size_t read(std::size_t count, char *destination) { return this->stream->read(count, destination); }
	bool supportsSeek() { return this->stream->supportsSeek(); }
	virtual std::size_t getPosition() { return this->stream->getPosition(); }
	virtual void seek(std::size_t position) { return this->stream->seek(position); }
	AudioFormat getFormatHint() { return this->stream->getFormatHint(); }

	protected:
	std::shared_ptr<ByteStream> stream;
};

/* A LookaheadByteStream for when the underlying stream supports seeking. */
class DirectLookaheadStream: public ForwardingStream<LookaheadByteStream> {
	public:
	DirectLookaheadStream(std::shared_ptr<ByteStream> stream): ForwardingStream(stream) {}
	void reset();
	void resetFinal();
};

void DirectLookaheadStream::reset() {
	this->stream->seek(0);
}

/* Reset and resetFinal are the same, since this stream type supported seeking. */
void DirectLookaheadStream::resetFinal() {
	this->reset();
}

/* A LookaheadByteStream for circumstances in which the underlying stream doesn't support seeking. */
class MemoryLookaheadStream: public ForwardingStream<LookaheadByteStream> {
	public:
	MemoryLookaheadStream(std::shared_ptr<ByteStream> stream): ForwardingStream(stream) {
		this->block_iter = this->blocks.begin();
	}

	void reset();
	void resetFinal();
	std::size_t read(std::size_t count, char *destination);

	private:
	static const int LOOKAHEAD_BLOCK_SIZE = 1024;
	struct LookaheadBytes {
		std::array<char, LOOKAHEAD_BLOCK_SIZE> data;
		std::size_t count;
	};
	std::list<std::shared_ptr<LookaheadBytes>> blocks;
	decltype(blocks)::iterator block_iter;
	std::shared_ptr<LookaheadBytes> current_block = nullptr;
	std::size_t current_block_pos = 0;
	bool recording = true;
};

std::size_t MemoryLookaheadStream::read(std::size_t count, char *destination) {
	std::size_t got = 0;
	while (got < count) {
		if (this->current_block && this->current_block_pos < this->current_block->count) {
			char *d = this->current_block->data.data();
			std::size_t needed = std::min(got-count, this->current_block->count-this->current_block_pos);
			std::copy(d, d+needed, destination);
			destination += needed;
			this->current_block_pos += needed;
		} else if (this->block_iter != this->blocks.end()) {
			this->block_iter ++;
			if (this->block_iter != this->blocks.end()) this->current_block = *this->block_iter;
			continue;
		} else {
			/* No more blocks are recorded, so we need to do a read. */
			std::array<char, LOOKAHEAD_BLOCK_SIZE> data;
			std::size_t count;
			count = this->read(LOOKAHEAD_BLOCK_SIZE, data.data());
			if (count == 0) break; // we reached the end.
			auto block = std::make_shared<LookaheadBytes>();
			block->count = count;
			block->data = data;
			this->current_block = block;
			this->blocks.push_back(this->current_block);
			this->block_iter = this->blocks.end();
			this->block_iter --;
		}
	}
	return got;
}

void MemoryLookaheadStream::reset() {
	assert(this->recording == false);
	this->block_iter = this->blocks.begin();
	if (blocks.empty() != false) {
		this->current_block = blocks.front();
		this->current_block_pos = 0;
	}
	/* Otherwise we didn't get anything yet. */
}

void MemoryLookaheadStream::resetFinal() {
	this->reset();
	this->recording = false;
}

std::shared_ptr<ByteStream> getLookaheadByteStream(std::shared_ptr<ByteStream> stream) {
	if (stream->supportsSeek()) {
		return std::make_shared<DirectLookaheadStream>(stream);
	} else {
		return std::make_shared<MemoryLookaheadStream>(stream);
	}
}

}
