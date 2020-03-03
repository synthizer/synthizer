#include <cstddef>
#include <string>

#include "types.hpp"

namespace synthizer {

class BuyteStream;

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
	 * Like streams, should only return less than num samples if at the end and 0 samples thereafter.
	 * 
	 * If channels is 0 (the default) then the number of output channels is equal to this asset. Otherwise channels are either truncated or filled with zero.
	 * 
	 * This class isn't responsible for channel conversion. Only audio I/O.
	 * */
	virtual std::size_t writeSamplesInterleaved(std::size_t num, AudioSample *samples, std::size_t channels = 0);
	/* Seek to a specific PCM frame.
	 *
	 * If this is past the end, then the position of the underlying stream is set to the end, and no samples are output.
	 * 
	 * IMPORTANT: not all formats support sample-accurate seeking. If you need sample-accurate seeking, decode the asset in memory.
	 * */
	virtual void seekPCM(std::size_t frame);
	/* Converts to PCM frame for you. */
	virtual void seekSeconds(double seconds);
	virtual unsigned int getSr();
	virtual unsigned int getChannels();
	virtual AudioFormat getFormat();
	virtual bool supportsSampleAccurateSeek() { return false; }

	protected:
	AudioDecoder *convertToEncodedInMemory() { return nullptr; }

	friend AudioDecoder *convertAudioDecoderToInMemory(AudioDecoder *input, bool encoded);
};

/*
 * Given protocol/path/options, get an audio decoder.
 * 
 * This will be disk-backed.  We find the format required either by (1) a hint from the ByteStream, if any, or (2) trying all the decoders we support.
 * 
 * The underlying stream will only be opened once. Internal machinery caches the bytes at the head of the stream.
 * */
AudioDecoder *getDecoderForProtocol(std::string protocol, std::string path, std::string options);

/*
 * Decode an audio asset, converting it into an in-memory representation.
 * 
 * If encoded is false, the default, the resulting asset will be stored in memory in decoded form.
 * 
 * Otherwise, if encoded is true and the AudioDecoder supports sample-accurate seeking and its convertToEncodedInMemory method returns non-null,
 * The asset is stored in memory in encoded form.
 * */
AudioDecoder *convertAudioDecoderToInMemory(AudioDecoder *input, bool encoded = false);

}
