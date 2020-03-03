#include <functional>
#include <vector>
#include <tuple>
#include <memory>
#include <cstddef>

#include "error.hpp"
#include "decoding.hpp"

namespace synthizer {

class ByteStreamError: public Error {
	public:
	ByteStreamError(std::string message): Error(message) {}
};


class UnsupportedByteStreamOperationError: public ByteStreamError {
	public:
	UnsupportedByteStreamOperationError(): ByteStreamError(std::string("Unsupported stream operation")) {}
	UnsupportedByteStreamOperationError(std::string message): ByteStreamError(message) {}
};

/*
 * A class representing a stream of bytes.
 * 
 * All kinds of streams that we might care about have a lot of functionality in common. The only difference is whether or not they support seek.
 * */
class ByteStream {
	public:
	virtual std::string getName() = 0;
	/*
	 * Fill destination with count bytes.
	 * 
	 * The only circumstance in which this function should return less than the number of requested bytes is at the end of the stream.
	 * 
	 * Calls after the stream is finished should return 0 to indicate stream end.
	 * */
	virtual std::size_t read(std::size_t count, char *destination) = 0;
	/* If seek is supported, override this to return true. */
	virtual bool supportsSeek() { return false; }
	/* Get the position of the stream in bytes. */
	virtual std::size_t getPosition() = 0;
	virtual void seek(std::size_t position) {
		throw new UnsupportedByteStreamOperationError("Streams of type " + this->getName() + " don't support seek");
	}
	/* If specified, we know the approximate underlying encoded format and will try that first when attempting to find a decoder. */
	virtual AudioFormat getFormatHint() { return AudioFormat::Unknown; }
};

/*
 * Used for audio format detection. Supports a reset() method, which resets to the beginning of the stream.
 * 
 * This works by caching bytes in memory if necessary. Reset always works, even if the underlying stream doesn't support seeking.
 * */
class LookaheadByteStream: public ByteStream {
	public:
	/* Reset to the beginning of the stream. */
	virtual void reset();
	/* Reset to the beginning of the stream, permanently disabling lookahead functionality. */
	void resetFinal();
};

/*
 * Synthizer has a concept of protocol, a bit like URL but pre-parsed.
 * 
 * This function finds a stream for a given protocol and instantiates it with the provided options.
 * 
 * Protocol: the protocol, which must be registered.
 * Path: A protocol-specific path, i.e. a file, etc.
 * Options: space-separated string of key-value pairs specific to a given protocol.
 * */
std::shared_ptr<ByteStream> getStreamForProtocol(const std::string &protocol, const std::string &path, const std::string &options);

/*
 * Register a protocol handler, a factory function for getting a protocol.
 * 
 * It is not possible to register a protocol twice.
 * 
 * Throws UnsupportedByteStreamOperationError in the event of duplicate registration.
 * */
void registerByteStreamProtocol(std::string &name, std::function<std::shared_ptr<ByteStream>(const std::string &, std::vector<std::tuple<std::string, std::string>>)> factory);

std::shared_ptr<ByteStream> getLookaheadByteStream(std::shared_ptr<ByteStream> stream);

}
