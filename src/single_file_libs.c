/* Implement all the header-only single file libraries. */

/* See dr_flac issue #143. */
#define DR_FLAC_NO_SIMD
#define DR_FLAC_IMPLEMENTATION
#include "dr_flac.h"

#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_ENCODING
#define MA_NO_DECODING
#define MA_NO_GENERATION
#include "miniaudio.h"

/* Yes, really. */
#include "stb_vorbis.c"