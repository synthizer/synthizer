#include "synthizer/channel_mixing.hpp"

#include "synthizer/types.hpp"

#include <algorithm>
#include <cassert>

namespace synthizer {

/*
 * Mix by dropping extra channels from in or adding extra zeros to out.
 * */
static void truncateChannels(unsigned int length, float *in, unsigned int inChannelCount, float *out,
                             unsigned int outChannelCount) {
  unsigned int minChannelCount = inChannelCount < outChannelCount ? inChannelCount : outChannelCount;
  for (unsigned int i = 0; i < length; i++) {
    for (unsigned int ch = 0; ch < minChannelCount; ch++) {
      out[i * outChannelCount + ch] += in[i * inChannelCount + ch];
    }
  }
}

static void upmixMono(unsigned int length, float *in, float *out, unsigned int outChannelCount) {
  for (unsigned int i = 0; i < length; i++) {
    float *frame = out + outChannelCount * i;
    for (unsigned int j = 0; j < outChannelCount; j++) {
      frame[j] += in[i];
    }
  }
}

static void downmixMono(unsigned int length, float *in, unsigned int inChannelCount, float *out) {
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

void mixChannels(unsigned int length, float *in, unsigned int inChannelCount, float *out,
                 unsigned int outChannelCount) {
  assert(inChannelCount != 0);
  assert(outChannelCount != 0);
  if (inChannelCount == outChannelCount) {
    for (unsigned int i = 0; i < length * inChannelCount; i++) {
      out[i] += in[i];
    }
    return;
  } else if (inChannelCount == 1) {
    return upmixMono(length, in, out, outChannelCount);
  } else if (outChannelCount == 1) {
    return downmixMono(length, in, inChannelCount, out);
  } else {
    truncateChannels(length, in, inChannelCount, out, outChannelCount);
  }
}

} // namespace synthizer