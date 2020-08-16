# Release Notes

## 0.6.4 (WIP)

- Python bindings now release the GIL in from_stream.

## 0.6.3

- Re-enable dr_flac SIMD.
- Fix a bug with resampling when decoding to buffers.

## 0.6.1

This release temporarily disables dr_flac SIMD support until resolution of [this upstream issue](https://github.com/mackron/dr_libs/issues/143).

## 0.6.0

### Features

- Major improvements to StreamingGenerator:
  - Position and looping are now exposed.
  - Streaming now happens in a background thread.
  - Streaming now builds up a buffer which prevents underruns.
  - Note that this uses a bunch of as-yet-untested threading/concurrency stuff that hasn't been exercised heavily, so if you notice bugs
    please open issues.
- Introduce a stereo panning strategy. Note that this will become the default panning strategy 
  in the future, because it's the only one that's safe on all speaker arrangements.
  If you want HRTF, request it by setting `SYZ_P_PANNER_STRATEGY` on the context and/or sources.

### Bugs Fixed

- Throw an exception instead of silently crashing on invalid audio files.
- Fix the fundamentally broken DirectSource mixing logic. This probably still needs some improvement but is no longer fundamentally broken.
- Fix Stream seeking when using LookaheadByteStream internally. This fixes/allows for StreamingGenerator to seek,
  and may also fix  loading of audio files into buffers in some cases.
## 0.5.0

- Roll out a deferred freeing strategy, which uses C++ custom allocators to
  move freeing pointers to a background thread that wakes up on a period. This doesn't move all freeing, but gets a vast
  majority of it. The impact is that it will take a little while for any large memory allocations to actually free,
  but that freeing will (mostly) not happen on any thread but the freeing thread, including
  threads from users of the public API.  This is the first step to eventually decreasing
  latency below 20MS or so, though the upshot of that work won't be seen for a while.
- Decouple Buffer from context. The manual already claimed that we did, but the library itself didn't. Calls to create buffers
  no longer needa context passed in.
- Gains are now scalars instead of DB. This is because it is valuable to be able to know which combinations of gains sum to 1 in order to prevent clipping.
  - For reference, you can get a scalar gain from DB as `10**(db / 20)` if you need it.
- Fix creating buffers of files which require resampling.

## 0.4.1

- Fixes to allocating multiple panner lanes from a PannerBank.
  - Manifested as the inability to play multiple sources.
- Fix HRTF computation for angles outside the built-in HRTF dataset, but which are still in range for elevation.

## 0.4.0

- Introduce DirectSource, for music and other non-panned assets.
- All sources now have gain, as part of refactoring for DirectSource.
- Introduce `syz_bufferGetChannels`, `syz_bufferGetLengthInSamples`, and `syz_bufferGetLengthInSeconds`, and obvious Python equivalents.
- Source add/remove generators needed to happen in the audio thread in order to not break. For now this is using `Context.call`, but in future
  it'll probably use a ConcurrentQueue to enqueue updates and avoid the overhead, or perhaps an abstracted PropertyRing.
- Move to [moodycamel::ConcurrentQueue](https://github.com/cameron314/concurrentqueue). This removes mutexes in a lot of places
  - As part of this, get rid of our custom lockfree queues. ConcurrentQueue is better; it's MPMC and not intrusive.
- Get rid of some dead code.
- Fix double properties that use P_DOUBLE_MIN to mean minimum double, not 0. This bug brought to you by surprising C++ numeric_limits behavior.
- Fix looping in `BufferGenerator`.
- Offer additional guarantees around `syz_getX` behavior with respect to properties that Synthizer modifies, like generator positions.
- CONTRIBUTING.md and pull request templates.

## 0.3.0

- Added support for Flac and MP3 via [dr_libs](https://github.com/mackron/dr_libs).
- Extended byte streams to be able to advertise their size.
