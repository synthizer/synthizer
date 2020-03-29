#pragma once

namespace synthizer {

/* Usage, from inside the synthizer namespace, is config::THING. */
namespace config {

/*
 * Sample rate of the library.
 * 
 * In order to be maximally efficient, both this and BLOCK_SIZE are fixed at compile-time.
 * */
const int SR = 44100;

/*
 * Number of samples to process in one block. This should be a multiple of 16 for future-proofing.
 * 
 * 256 samples is 172updatesper second, fast enough for anything rasonable.
 * 
 * We may have to raise this later for performance.
 * */
const int BLOCK_SIZE = 256;

/*
 * The minimum size of the ringbuffers used for audio output are BLOCK_SIZE * BLOCKS_PER_RING frames.
 * 
 * Must be at least 1, should probably never be below 2.
 * 
 * Essentially, this is the minimum allowed maximum latency: if the audio backend would let us use a smaller buffer, we don't.
 * 
 * Algorithms upstream of audio outputs will adapt their latency to be lower or higher depending on other factors.
 * */
const int BLOCKS_PER_RING = 2;

/*
 * The fundamental alignment, in bytes, of arrays holding samples.
 * 
 * SSE2 requires 16 byte alignment. Note that a float is 4 bytes, so we don't waste much.
 * */
const int ALIGNMENT = 16;

}
}