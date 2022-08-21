#pragma once

#include "synthizer/compiler_specifics.hpp"
#include "synthizer/types.hpp"

#include <cassert>

namespace synthizer {
namespace channel_mixing_detail {

/*
 * Mix by dropping extra channels from in or adding extra zeros to out.
 * */
FLATTENED void truncateChannels(unsigned int length, float *in, unsigned int inChannelCount, float *out,
                                unsigned int outChannelCount) {
  unsigned int minChannelCount = inChannelCount < outChannelCount ? inChannelCount : outChannelCount;
  for (unsigned int i = 0; i < length; i++) {
    for (unsigned int ch = 0; ch < minChannelCount; ch++) {
      out[i * outChannelCount + ch] += in[i * inChannelCount + ch];
    }
  }
}

FLATTENED void upmixMono(unsigned int length, float *in, float *out, unsigned int outChannelCount) {
  for (unsigned int i = 0; i < length; i++) {
    float *frame = out + outChannelCount * i;
    for (unsigned int j = 0; j < outChannelCount; j++) {
      frame[j] += in[i];
    }
  }
}

FLATTENED void downmixMono(unsigned int length, float *in, unsigned int inChannelCount, float *out) {
  float normfactor = 1.0f / inChannelCount;
  for (unsigned int i = 0; i < length; i++) {
    float *frame = in + i * inChannelCount;
    float sum = 0.0f;
    for (unsigned int j = 0; j < inChannelCount; j++) {
      sum += frame[j];
    }
    out[i] += sum * normfactor;
  }
}

} // namespace channel_mixing_detail

/*
 * Mix a specified length of audio from in with inChannelCount to out with outChannelCount.
 *
 * This function is used to handle i.e. generator processing for mono to stereo, etc.
 *
 * As is the Synthizer convention, this adds to output, not replaces.
 * */
inline void mixChannels(unsigned int length, float *in, unsigned int inChannelCount, float *out,
                        unsigned int outChannelCount) {
  assert(inChannelCount != 0);
  assert(outChannelCount != 0);
  if (inChannelCount == outChannelCount) {
    for (unsigned int i = 0; i < length * inChannelCount; i++) {
      out[i] += in[i];
    }
    return;
  } else if (inChannelCount == 1) {
    return channel_mixing_detail::upmixMono(length, in, out, outChannelCount);
  } else if (outChannelCount == 1) {
    return channel_mixing_detail::downmixMono(length, in, inChannelCount, out);
  } else {
    return channel_mixing_detail::truncateChannels(length, in, inChannelCount, out, outChannelCount);
  }
}

} // namespace synthizer
