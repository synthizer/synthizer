#pragma once

#include "synthizer/types.hpp"

namespace synthizer {

/*
 * Mix a specified length of audio from in with inChannelCount to out with outChannelCount.
 * 
 * This function is used to handle i.e. generator processing for mono to stereo, etc.
 * 
 * As is the Synthizer convention, this adds to output, not replaces.
 * */
void mixChannels(unsigned int length, float *in, unsigned int inChannelCount, float *out, unsigned int outChannelCount);
}
