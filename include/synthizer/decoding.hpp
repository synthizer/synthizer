#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <memory>

#include "types.hpp"
#include "error.hpp"

namespace synthizer {

class ByteStream;
class LookaheadByteStream;


/* An unsupported format. */
class UnsupportedFormatError: public Error {
	public:
	UnsupportedFormatError(std::string protocol = "", std::string path = "", std::string options = ""): Error("Unsupported audio format."), protocol(protocol), path(path), options(options) {}
	std::string protocol, path, options;
};

/* The audio formats we support. */
enum class AudioFormat: int {
	Unknown,
	Ogg,
	Wav,
	Flac,
	Mp3,
};

/*
 * An audio decoder.
 * 
 * Operations on this class are supported if the underlying stream supports them. Note that ot all formats can perform sample accurate seeking.
 * */
class AudioDecoder {
	public:
	virtual ~AudioDecoder() {}
	/*
	 * If channels is <1 (the default) then the number of output channels is equal to this asset. Otherwise channels are either truncated or filled with zero.
	 * 
	 * This class isn't responsible for channel conversion. Only audio I/O.
	 * */
	virtual std::int64_t writeSamplesInterleaved(std::int64_t num, AudioSample *samples, std::int64_t channels = 0) = 0;
	/* Seek to a specific PCM frame.
	 *
	 * If this is past the end, then the position of the underlying stream is set to the end, and no samples are output.
	 * 
	 * IMPORTANT: not all formats support sample-accurate seeking. If you need sample-accurate seeking, decode the asset in memory.
	 * */
	virtual void seekPcm(std::int64_t frame) = 0;
	/* We only support seek if the underlying stream will allow it, and the decoded format supports seek. */
	virtual bool supportsSeek() = 0;
	/* Some formats only support approximate seeking. */
	virtual bool supportsSampleAccurateSeek() { return false; }
	/* If seeking is supported, return the size of this asset. */
	virtual std::int64_t getLength() = 0;
	/* Converts to PCM frame for you. */
	virtual void seekSeconds(double seconds) {
		seekPcm(seconds*this->getSr());
	}

	virtual int getSr() = 0;
	virtual int getChannels() = 0;
	virtual AudioFormat getFormat() = 0;
};

/*
 * Given protocol/path/options, get an audio decoder.
 * 
 * This will be disk-backed.  We find the format required either by (1) a hint from the ByteStream, if any, or (2) trying all the decoders we support.
 * 
 * The underlying stream will only be opened once. Internal machinery caches the bytes at the head of the stream.
 * */
std::shared_ptr<AudioDecoder> getDecoderForProtocol(std::string protocol, std::string path, std::string options);

/*
 * Decode an audio asset, converting it into an in-memory representation.
 * 
 * If encoded is false, the default, the resulting asset will be stored in memory in decoded form.
 * 
 * otherwise a best-effort attempt is made to keep the asset in encoded form.
 * 
 * */
std::shared_ptr<AudioDecoder> convertAudioDecoderToInMemory(std::shared_ptr<AudioDecoder> input, bool encoded = false);

/*
 * Semi-private infrastructure to decoders that we need to have somewhere for getDecoderForProtocol.
 * 
 * getDecoderForProtocol tries all of these and takes the first which doesn't error and doesn't return nullptr.
 * */
std::shared_ptr<AudioDecoder> decodeFlac(std::shared_ptr<LookaheadByteStream> stream);
std::shared_ptr<AudioDecoder> decodeOgg(std::shared_ptr<LookaheadByteStream> stream);
std::shared_ptr<AudioDecoder> decodeMp3(std::shared_ptr<LookaheadByteStream> stream);
std::shared_ptr<AudioDecoder> decodeWav(std::shared_ptr<LookaheadByteStream> stream);

}
