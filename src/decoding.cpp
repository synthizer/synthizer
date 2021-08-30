#include <algorithm>
#include <array>
#include <exception>

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

static auto decoders = makeStaticArray(DecoderDef{"libsndfile", decodeLibsndfile}, DecoderDef{"dr_wav", decodeWav},
                                       DecoderDef{"dr_flac", decodeFlac}, DecoderDef{"dr_mp3", decodeMp3});

std::shared_ptr<AudioDecoder> getDecoderForStream(std::shared_ptr<ByteStream> stream) {
  auto lookahead_stream = getLookaheadByteStream(stream);
  for (auto d : decoders) {
    try {
      lookahead_stream->reset();
      auto tmp = d.func(lookahead_stream);
      if (tmp == nullptr) {
        logDebug("Handler %s returned nullptr. Skipping", d.name.c_str());
        continue;
      }
      logDebug("Handling stream with handler %s", d.name.c_str());
      return tmp;
    } catch (const std::exception &e) {
      logDebug("Format %s threw error %s", d.name.c_str(), e.what());
      continue;
    }
  }

  logDebug("unable to decode");
  throw UnsupportedFormatError();
}

std::shared_ptr<AudioDecoder> getDecoderForStreamParams(const char *protocol, const char *path, void *param) {
  logDebug("Trying to decode %s:%s", protocol, path);
  auto stream = getStreamForStreamParams(protocol, path, param);
  return getDecoderForStream(stream);
}

} // namespace synthizer
