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
 * When doing various internal crossfades (i.e. HRTF), how many samples do we use?
 * 
 * Must be a multiple of 4 and less than the block size; ideally keep this as a mutliple of 8.
 * */
const int CROSSFADE_SAMPLES = 64;

/*
 * The fundamental alignment, in bytes, of arrays holding samples.
 * 
 * SSE2 requires 16 byte alignment. Note that a float is 4 bytes, so we don't waste much.
 * */
const int ALIGNMENT = 16;

/*
 * The maximum delay for the ITD, in samples.
 * 
 * Must be at least 2.
 * 
 * This default comes from the woodworth ITD formula's maximum value for a 0.15 CM radius: (0.15/343)*(math.pi/2+1)*44100
 * Rounded up to a power of 2.
 * */
const int HRTF_MAX_ITD = 64;

}
}