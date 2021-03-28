#include <exception>
#include <array>
#include <algorithm>

#include "synthizer/byte_stream.hpp"
#include "synthizer/decoding.hpp"
#include "synthizer/logging.hpp"
#include "synthizer/make_static_array.hpp"

namespace synthizer {

typedef std::shared_ptr<AudioDecoder> (*DecoderFunc)(std::shared_ptr<LookaheadByteStream> stream);
struct DecoderDef {
	std::string name;
	DecoderFunc func;
};

static auto decoders = makeStaticArray(
	DecoderDef{"dr_wav", decodeWav},
	DecoderDef{"dr_flac", decodeFlac},
	DecoderDef{"dr_mp3", decodeMp3}
);

std::shared_ptr<AudioDecoder> getDecoderForProtocol(const char *protocol, const char *path, void *param) {
	auto stream = getStreamForProtocol(protocol, path, param);
	auto lookahead_stream = getLookaheadByteStream(stream);
	for(auto d: decoders) {
		try {
			lookahead_stream->reset();
			auto tmp = d.func(lookahead_stream);
			if (tmp == nullptr) {
				logDebug("Path %s: handler %s returned nullptr. Skipping", path, d.name.c_str());
				continue;
			}
			logDebug("Handling path %s with handler %s", path, d.name.c_str());
			return tmp;
		} catch(std::exception &e) {
			logDebug("Path %s: Format %s threw error %s", path, d.name.c_str(), e.what());
			continue;
		}
	}

	logDebug("Path %s: unable to decode", path);
	throw UnsupportedFormatError(protocol, path);
}

}
