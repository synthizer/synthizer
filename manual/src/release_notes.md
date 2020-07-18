# Release Notes

## 0.5.0 (WIP)

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
