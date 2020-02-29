#include <functional>
#include <vector>
#include <tuple>

#include "error.hpp"

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
	virtual std::string &getName() = 0;
	/*
	 * Fill destination with count bytes.
	 * 
	 * The only circumstance in which this function should return less than the number of requested bytes is at the end of the stream.
	 * 
	 * Calls after the stream is finished should return 0 to indicate stream end.
	 * */
	virtual unsigned int read(unsigned int count, char *destination) = 0;
	/* If seek is supported, override this to return true. */
	virtual bool supportsSeek() { return false; }
	/* Get the position of the stream in bytes. */
	virtual unsigned int getPosition() = 0;
	virtual void seek(unsigned int position) {
		throw new UnsupportedByteStreamOperationError("Streams of type " + this->getName() + " don't support seek");
	}
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
ByteStream* getStreamForProtocol(const std::string &protocol, const std::string &path, const std::string &options);

/*
 * Register a protocol handler, a factory function for getting a protocol.
 * 
 * It is not possible to register a protocol twice.
 * 
 * Throws UnsupportedByteStreamOperationError in the event of duplicate registration.
 * */
void registerByteStreamProtocol(std::string &name, std::function<ByteStream*(const std::string &, std::vector<std::tuple<std::string, std::string>>)> factory);

}
