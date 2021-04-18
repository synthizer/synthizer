#pragma once

#include <functional>
#include <vector>
#include <tuple>
#include <memory>
#include <cstddef>

#include "error.hpp"
#include "decoding.hpp"

struct syz_CustomStreamDef;

namespace synthizer {

/**
 * Byte streaming functionality, to abstract over file and otherI/O.
 * 
 * Note that this infrastructure uses unsigned long long instead of size_t.  This is to support custom
 * streams, which need to play nice with FFI, not all of which is able to
 * conveniently handle size_t.
 * */

class EByteStream: public Error {
	public:
	EByteStream(std::string message): Error(message) {}
};

#define BERROR3(name, msg, base) \
class name: public base { \
	public: \
	name(): base(msg) {}\
	name(std::string message): base(message){}\
}
#define BERROR(name, msg) BERROR3(name, msg, EByteStream)

BERROR(EByteStreamUnsupportedOperation, "Unsupported byte stream operation");
BERROR(EByteStreamNotFound, "Resource not found");
BERROR(EByteStreamCustom, "custom byte stream callback error");

/*
 * A class representing a stream of bytes.
 * 
 * All kinds of streams that we might care about have a lot of functionality in common. The only difference is whether or not they support seek.
 * */
class ByteStream {
	public:
	virtual ~ByteStream() {}

	virtual std::string getName() = 0;
	/*
	 * Fill destination with count bytes.
	 * 
	 * The only circumstance in which this function should return less than the number of requested bytes is at the end of the stream.
	 * 
	 * Calls after the stream is finished should return 0 to indicate stream end.
	 * */
	virtual unsigned long long read(unsigned long long count, char *destination) = 0;
	/* If seek is supported, override this to return true. */
	virtual bool supportsSeek() { return false; }
	/* Get the position of the stream in bytes. */
	virtual unsigned long long getPosition() = 0;
	virtual void seek(unsigned long long position) {
		(void)position;
		throw EByteStreamUnsupportedOperation("Streams of type " + this->getName() + " don't support seek");
	}
	/* If seek is supported, return the length. Otherwise, return an undefined value. */
	virtual unsigned long long  getLength() {
		return 0;
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
	virtual void reset() = 0;
	/* Reset to the beginning of the stream, permanently disabling lookahead functionality. */
	virtual void resetFinal() = 0;
};

/**
 * Inherit from T, and implement the forwarding interface on the underlying stream.
 * 
 * This is used primarily in two places: to implement lookahead streams using the CRTP, and to
 * expose stream handles (by inheriting from a StreamHandle class with the CRTP).
 * */
template<typename T>
class ForwardingStream: public T {
	public:
	ForwardingStream(std::shared_ptr<ByteStream> stream): stream(stream) {}

	std::string getName() override {
		return this->stream->getName();
	}

	unsigned long long read(unsigned long long count, char *destination) override {
		return this->stream->read(count, destination);
	}

	bool supportsSeek() override {
		return this->stream->supportsSeek();
	}

	virtual unsigned long long  getPosition() override {
		return this->stream->getPosition();
	}

	virtual unsigned long long getLength() override {
		return this->stream->getLength();
	}

	virtual void seek(unsigned long long position) override {
		return this->stream->seek(position);
	}

	AudioFormat getFormatHint() override {
		return this->stream->getFormatHint();
	}

	protected:
	std::shared_ptr<ByteStream> stream;
};

/*
 * Synthizer has a concept of protocol, a bit like URL but pre-parsed.
 * 
 * This function finds a stream for a given protocol and instantiates it with the provided options.
 * 
 * Protocol: the protocol, which must be registered.
 * Path: A protocol-specific path, i.e. a file, etc.
 * param: NULL for built-in streams. Reserved for user-defined streams.
 * */
std::shared_ptr<ByteStream> getStreamForStreamParams(const std::string &protocol, const std::string &path, void *param);

/*
 * Register a protocol handler, a factory function for getting a protocol.
 * 
 * It is not possible to register a protocol twice.
 * 
 * Throws EByteStreamUnsupportedOperation in the event of duplicate registration.
 * */
typedef std::function<std::shared_ptr<ByteStream>(const char *path, void *param)> ByteStreamFactory;
void registerByteStreamProtocol(std::string &name, ByteStreamFactory factory);

std::shared_ptr<LookaheadByteStream> getLookaheadByteStream(std::shared_ptr<ByteStream> stream);

/*
 * Some audio formats (namely stb_ogg) use libraries which aren't flexible enough to be driven via callbacks, but we can't assume files.
 * 
 * This function builds a buffer in memory for those formats. Using it is an anti-pattern, but for i.e. ogg, it makes sense to go ahead and do it this way,
 * since we might as well push the performance problem down the road and ogg is usually pretty small even for large assets.
 * 
 * The alternative is modify upstream libraries, which we'd rather not do for the time being.
 * 
 * The returned buffer should be be freed with delete.
 * */
char *byteStreamToBuffer(std::shared_ptr<ByteStream> stream);

/*
 * Kinds of stream. Probably self-explanatory.
 * */

/* data must outlive the stream. */
std::shared_ptr<ByteStream> memoryStream(std::size_t size, const char *data);

/* file stream. Throws EByteStreamNotFound. */
std::shared_ptr<ByteStream> fileStream(const char *path, void *param);

std::shared_ptr<ByteStream> customStream(struct syz_CustomStreamDef *def);

}
