# Release Notes for 0.0.x to 0.8.x

Starting at 0.8 and later, Synthizer maintains separate pages for every
incompatible release with more details on compatibility breakage and what
specifically changed. The following were early development versions, and should
no longer be used.

## 0.7.7

- Identical to 0.7.5.  0.7.6 introduced a major performance regression that
  makes Synthizer unusable (probably issue #32, but investigations are ongoing).
  After this is fixed, the next version will contain all the nice 0.7.6 things.

## 0.7.6

- Synthizer now builds on Linux.  This is preliminary.  If you experience
  issues, please report them, as Linux isn't my primary platform.  That said, I
  have received multiple confirmations that it works.
- Python now publishes source distributions, which are capable of building
  without any extra intervention on your part, except for Windows which requires
  being in an appropriate MSVC shell.  On Linux, you can install Synthizer into
  virtualenvs with a simple `pip install synthizer`, assuming a supported C and
  C++ compiler.
  - Note that git is currently required in order to clone the Synthizer
    repository. We might include Synthizer inline in future.
- Prebuilt C artifacts for Windows now link against the multithreaded dynamic
  CRT. This was done to speed up Python builds and because there is no obvious
  right choice. If you need a different configuration, please build from source
  specifying `CMAKE_MSVC_RUNTIME_LIBRARY` and `SYNTHIZER_LIB_TYPE` to
  appropriate values.
- The audio generation all runs inline in the audio callback. This has the
  knock-on effect of making. property reads even slower than they were already,
  but is necessary to work well on Linux. See issue #32 for tracking fast
  property reads.

## 0.7.5

- Introduce [reverb](../object_reference/global_fdn_reverb.md).
- Start building Python 3.9 wheels.
- Sources now fade gain changes to prevent clicks, especially when feeding
  effects.
- Expose `syz_resetEffect` for interactive experimentation purposes, to clear
  the internal state of effects. In python this is `.reset()` on any effect.
- All effects now have a `SYZ_P_GAIN` property to set the overall gain of the
  effect.
- internally fix the bitsets, which manifested as weird issues with allocating
  sources to panner lanes, for example issue #16 where PannedSource could become
  silent.  Note that this may introduce other issues in exchange: it was assumed
  that the bitsets were working this entire time.

## 0.7.4

- Document our [versioning and stability guarantees](../concepts/stability.md).
- Introduce [effect routing support](../concepts/effects.md).
- Introduce an [Echo](../object_reference/global_echo.md) effect.

## 0.7.3

- Fix pitch bend support on BufferGenerator when looping is enabled. For the
  morbidly curious there is indeed a difference between `std::fmod` and
  `std::remainder`
- Maybe fix bugs in stereo panning for the same reason. If you had issues with
  stereo panning and didn't report them, you should probably try this release.

## 0.7.2

- New property on BufferGenerator: `SYZ_P_PITCH_BEND`.
- We will adopt pre-1.0 semantic versioning going forward in order to be
  compatible with Rust-style `^` dependencies: `0.major.minor`, with minor
  incrementing for features and major incrementing for incompatible API updates.
  This should enable at least some compatibility in version numbers across
  package managers.

## 0.7.1

- Attempt releasing 32-bit Python wheels.

## 0.7.0

- Add a noise generator.
- Internal fixes to filters.

## 0.6.4

- Python bindings now release the GIL in from_stream.

## 0.6.3

- Re-enable dr_flac SIMD.
- Fix a bug with resampling when decoding to buffers.

## 0.6.1

This release temporarily disables dr_flac SIMD support until resolution of [this
upstream issue](https://github.com/mackron/dr_libs/issues/143).

## 0.6.0

### Features

- Major improvements to StreamingGenerator:
  - Position and looping are now exposed.
  - Streaming now happens in a background thread.
  - Streaming now builds up a buffer which prevents underruns.
  - Note that this uses a bunch of as-yet-untested threading/concurrency stuff
    that hasn't been exercised heavily, so if you notice bugs please open
    issues.
- Introduce a stereo panning strategy. Note that this will become the default
  panning strategy in the future, because it's the only one that's safe on all
  speaker arrangements. If you want HRTF, request it by setting
  `SYZ_P_PANNER_STRATEGY` on the context and/or sources.

### Bugs Fixed

- Throw an exception instead of silently crashing on invalid audio files.
- Fix the fundamentally broken DirectSource mixing logic. This probably still
  needs some improvement but is no longer fundamentally broken.
- Fix Stream seeking when using LookaheadByteStream internally. This
  fixes/allows for StreamingGenerator to seek, and may also fix  loading of
  audio files into buffers in some cases.
  ## 0.5.0

- Roll out a deferred freeing strategy, which uses C++ custom allocators to move
  freeing pointers to a background thread that wakes up on a period. This
  doesn't move all freeing, but gets a vast majority of it. The impact is that
  it will take a little while for any large memory allocations to actually free,
  but that freeing will (mostly) not happen on any thread but the freeing
  thread, including threads from users of the public API.  This is the first
  step to eventually decreasing latency below 20MS or so, though the upshot of
  that work won't be seen for a while.
- Decouple Buffer from context. The manual already claimed that we did, but the
  library itself didn't. Calls to create buffers no longer needa context passed
  in.
- Gains are now scalars instead of DB. This is because it is valuable to be able
  to know which combinations of gains sum to 1 in order to prevent clipping.
  - For reference, you can get a scalar gain from DB as `10**(db / 20)` if you
    need it.
- Fix creating buffers of files which require resampling.

## 0.4.1

- Fixes to allocating multiple panner lanes from a PannerBank.
  - Manifested as the inability to play multiple sources.
- Fix HRTF computation for angles outside the built-in HRTF dataset, but which
  are still in range for elevation.

## 0.4.0

- Introduce DirectSource, for music and other non-panned assets.
- All sources now have gain, as part of refactoring for DirectSource.
- Introduce `syz_bufferGetChannels`, `syz_bufferGetLengthInSamples`, and
  `syz_bufferGetLengthInSeconds`, and obvious Python equivalents.
- Source add/remove generators needed to happen in the audio thread in order to
  not break. For now this is using `Context.call`, but in future it'll probably
  use a ConcurrentQueue to enqueue updates and avoid the overhead, or perhaps an
  abstracted PropertyRing.
- Move to
  [moodycamel::ConcurrentQueue](https://github.com/cameron314/concurrentqueue).
  This removes mutexes in a lot of places
  - As part of this, get rid of our custom lockfree queues. ConcurrentQueue is
    better; it's MPMC and not intrusive.
- Get rid of some dead code.
- Fix double properties that use P_DOUBLE_MIN to mean minimum double, not 0.
  This bug brought to you by surprising C++ numeric_limits behavior.
- Fix looping in `BufferGenerator`.
- Offer additional guarantees around `syz_getX` behavior with respect to
  properties that Synthizer modifies, like generator positions.
- CONTRIBUTING.md and pull request templates.

## 0.3.0

- Added support for Flac and MP3 via
  [dr_libs](https://github.com/mackron/dr_libs).
- Extended byte streams to be able to advertise their size.
