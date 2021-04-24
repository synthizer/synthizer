#include "synthizer/byte_stream.hpp"
#include "synthizer/threadsafe_initializer.hpp"

#include <array>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <shared_mutex>
#include <sstream>
#include <thread>
#include <vector>

namespace synthizer {

/*
 * Infrastructure for the registry.
 * */
static std::shared_mutex byte_stream_registry_lock{};
static std::map<std::string, ByteStreamFactory> byte_stream_registry{
	{ "file", fileStream },
};

void registerByteStreamProtocol(std::string &name, ByteStreamFactory factory) {
	auto guard = std::lock_guard(byte_stream_registry_lock);
	if (byte_stream_registry.count(name))
		throw EByteStreamUnsupportedOperation("Attempted duplicate registry of protocol "+name);

	byte_stream_registry[name] = factory;
}

std::shared_ptr<ByteStream> getStreamForStreamParams(const std::string &protocol, const std::string &path, void *param) {
	auto l = std::shared_lock{byte_stream_registry_lock};
	if (byte_stream_registry.count(protocol) == 0)
		throw EByteStreamUnsupportedOperation("Unregistered protocol " + protocol);
	auto &f = byte_stream_registry[protocol];
	auto o = f(path.c_str(), param);
	if (o == nullptr)
		throw EByteStream("Protocol " + protocol + " returned nullptr. No further information available.");
	return o;
}

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
		this->blocks.reserve(5);
	}

	virtual void reset() override;
	virtual void resetFinal() override;
	virtual unsigned long long read(unsigned long long count, char *destination) override;

	private:
	static const int LOOKAHEAD_BLOCK_SIZE = 1024;
	struct LookaheadBytes {
		std::array<char, LOOKAHEAD_BLOCK_SIZE> data;
		std::size_t count;
	};
	std::vector<std::shared_ptr<LookaheadBytes>> blocks;
	std::size_t current_block = 0, current_block_pos = 0;
	bool recording = true;
};

unsigned long long MemoryLookaheadStream::read(unsigned long long count, char *destination) {
	unsigned long long got = 0;
	while (got < count) {
		if (this->blocks.size() > this->current_block) {
			auto &cur = this->blocks[this->current_block];
			char *d = cur->data.data();
			unsigned long long needed = std::min<unsigned long long>(got-count, cur->count-this->current_block_pos);
			std::copy(d, d+needed, destination);
			destination += needed;
			this->current_block_pos += needed;
			if (this->current_block_pos == LOOKAHEAD_BLOCK_SIZE) this->current_block ++;
			got += needed;
		} else {
			/* No more blocks are recorded, so we need to do a read. */
			std::array<char, LOOKAHEAD_BLOCK_SIZE> data;
			unsigned long long got_this_time;
			got_this_time = this->read(LOOKAHEAD_BLOCK_SIZE, data.data());
			if (got_this_time == 0) break; // we reached the end.
			auto block = std::make_shared<LookaheadBytes>();
			block->count = got_this_time;
			block->data = data;
			this->blocks.push_back(block);
			this->current_block_pos = 0;
			got += got_this_time;
		}
	}
	return got;
}

void MemoryLookaheadStream::reset() {
	assert(this->recording == false);
	this->current_block = 0;
	this->current_block_pos = 0;
}

void MemoryLookaheadStream::resetFinal() {
	this->reset();
	this->recording = false;
}

std::shared_ptr<LookaheadByteStream> getLookaheadByteStream(std::shared_ptr<ByteStream> stream) {
	if (stream->supportsSeek()) {
		return std::make_shared<DirectLookaheadStream>(stream);
	} else {
		return std::make_shared<MemoryLookaheadStream>(stream);
	}
}

char *byteStreamToBuffer(std::shared_ptr<ByteStream> stream) {
	static const std::size_t BLOCK_SIZE = 8192;
	std::list<std::vector<char>> blocks{};
	std::size_t got = 0;
	std::size_t total_size = 0;

	do {
		blocks.emplace_back(BLOCK_SIZE);
		got = stream->read(BLOCK_SIZE, blocks.back().data());
		blocks.back().resize(got);
		total_size += got;
	} while(got);

	char *buffer = new char[total_size];

	char *cur = buffer;
	for(auto &b: blocks) {
		std::copy(b.begin(), b.end(), cur);
		cur += b.size();
	}

	return buffer;
}

}
