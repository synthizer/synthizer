# Release Notes

## 0.4.0 (WIP)

- Introduce DirectSource, for music and other non-panned assets.
- All sources now have gain, as part of refactoring for DirectSource.
- Source add/remove generators needed to happen in the audio thread in order to not break. For now this is using `Context.call`, but in future
  it'll probably use a ConcurrentQueue to enqueue updates and avoid the overhead, or perhaps an abstracted PropertyRing.
- Move to [moodycamel::ConcurrentQueue](https://github.com/cameron314/concurrentqueue). This removes mutexes in a lot of places
  - As part of this, get rid of our custom lockfree queues. ConcurrentQueue is better; it's MPMC and not intrusive.
- Get rid of some dead code.
- Fix double properties that use P_DOUBLE_MIN to mean minimum double, not 0. This bug brought to you by surprising C++ numeric_limits behavior.
- CONTRIBUTING.md and pull request templates.

## 0.3.0

- Added support for Flac and MP3 via [dr_libs](https://github.com/mackron/dr_libs).
- Extended byte streams to be able to advertise their size.
