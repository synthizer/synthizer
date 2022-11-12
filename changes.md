## 0.11.11 (2022-11-12)

- Fix miniaudio configuration to no longer request insanely low latencies (#88).

## 0.11.10 (2022-09-24)

NOTE: 0.11.9 had a typo in CMakeLists.txt, and should be considered a bad release.

This is a major bugfix release and upgrading is recommended for all users.  Incompatibilities were deferred; see below
for intended changes that will likely arrive in the 0.12.x releases.

IMPORTANT: part of the motivation of this release is that avoiding breakage allows one final backport to Python to get
this batch of important bugfixes in.  But that's it.  If a Python contributor doesn't step up, those bindings will begin
falling behind in the 0.11.x series, expected in roughly 3 to 6 months.

Note that this Git repository now uses submodules.

### Changes

- `SYZ_P_PITCH_BEND`'s range is now restricted to `0.0 <= value <= 2.0`.  This is not considered a breaking change
  because it used to crash the program.
- Many crashes related to pitch been have been fixed.
- The command queue used internally to move user calls to the audio thread has been rewritten and can now be safely used
  in multi-threaded applications.
- FdnReverb was rewritten and improved internally to be faster and more reliable.
- Too many bugs to count identified by @ndarilek have been fixed.
- the library is now primarily header only, and as a consequence builds faster from scratch.
- Bindings authors wishing to increase build speed may use the new CMake option `SYZ_INTEGRATING` to disable tests and
  examples.  If using `vendor.py` to extract a Synthizer source tree, this option is mandatory.
- benchmarks and unit tests have been added.
- the HRTF implementation was refactored and optimized.  This should have no audible changes.
- Debug and release builds with debuginfo builds now contain more explicit instrumentation to detect out-of-bounds
  indexing.  This is controlled by the C++ `NDEBUG` macro, like `assert`.
- A new internal abstraction, `ModPointer`, allows avoiding modulus when working with delay lines in many cases.

### Anticipated Incompatible Changes

The following changes are expected in the 0.12.x series but have not yet been implemented:

- All objects will take a context and be bound to their context.  This includes decoding buffers.
  - As a consequence, `syz_initialize` and `syz_shutdown` will be removed, and multiple instances of the library will be
    able to coexist in the same project.
  - rationale: people keep asking for this and I keep needing it.  Also, we'll have context-wide metrics and simplified
    internals around handle management.
- Userdata will be removed.  Instead, implement it as a hashmap over handles.
  - If you need a truly unique id, open an issue.
  - Rationale: not many users and a giant concurrency headache. Simplifies many bindings.
- Synthizer will require Rust to build, possibly with a CMake option to install a local copy in the build tree.
  - Rationale: we can run build-time tools to generate datasets and things and it will play nice with cross compiling.
    Also, we can probably drop like a megabyte from the source code for the HRTF/prime numbers arrays, which is good for
    package managers that wish to embed via `vendor.py`.
- Libsndfile support will be dropped.
  - Rationale: This doesn't exactly work. See [#70](https://github.com/synthizer/synthizer/issues/70).
  - If you specifically need ogg, then I may be open to picking up a BSD-licensed dependency, but not until after 0.12
    is released.
  - I am not particularly interested in trying to support every codec under the sun.  If you are trying to write a media
    player,. you will have much better luck with ffmpeg.

Looking further out, the following changes may be implemented:

- Synthizer may pick up dependencies which require credit in binary distributions, for example MIT or BSD licenses.
  - Rationale: I liked the "let's not do this, you can just drop it in" stance.  But I am one person and it's time
    consuming.
  - We will maintain a compiled list of what these dependencies are and what their licenses are, and a file that you can
    just drop in to give credit where it's needed.
  - Note that source-credit-only dependencies (e.g. zlib license) handle themselves by having the license in the
    Synthizer source tree and, indeed, we already use some for that reason.
- Synthizer may begin requiring Boost.
  - Rationale: time again. We are currently extracting header-only subsets; we'll see if that lasts.  But if it crosses
    the line of needing a Boost library requiring building, we can't do it invisibly.

## 0.11.8 (2022-03-18)

- Fix linking libm to examples on Linux.

## 0.11.7 (2022-03-06)

- Fix MSVC 2022 support.
- Fix internal filter math.
- Fix bandpass filters, 

## 0.11.6 (2021-11-27)

- Introduce `FastSineBankGenerator` to synthesize chiptune/additive synth/etc waveforms.
- Fix very long automation timelines.

## 0.11.5

- Fix the ability to use non-seekable streams.
  - NOTE: contrary to docs (updated in this release), dr_wav doesn't support non-seekable wav streams; mp3 and flac work
    fine.
- Fix a race condition which could result in a crash when dropping the last reference to sources while audio is being
  generated.
- Small fixes to synthizer.h for const correctness and more valid C.

## 0.11.4 (2020-10-24)

- Add `syz_bufferGetSizeInBytes`, to get a buffer's size in bytes.  This is useful for caching/memory tracking purposes.

## 0.11.3 (2020-15-2021

- Fix a bug where adding points to automation timelines in the future would drop those points.

## 0.11.2 (2020-10-13)

- Fix seeking and other related feedback loops where Synthizer sets a property and gets stuck in an infinite loop

## 0.11.0 (2020-10-11)

## Changes in this Release

### Python Bindings Need Maintainers

I have moved on to Rust and don't have time to keep maintaining the Python bindings if I am going to ever be able to do
my own projects with Synthizer.  To that end, the Python bindings now live
[here](https://github.com/synthizer/synthizer-python) with instructions on how to update them.  I'll be keeping the
responsibility of PR review and release management, but won't necessarily keep them up to date.

They statically link a vendored version of Synthizer, so this only means that they'll fall behind unless someone steps
up to help.

### Other Changes in this Release

- An automation API is now available to change properties over time, e.g. for crossfades and fadeouts.
- `PannedSource` has been split into `ScalarPannedSource` and `AngularPannedSource`.
  - This prevents regressions with respect to being able to use panning scalars.
  - Use `AngularPannedSource` for azimuth/elevation and `ScalarPannedSource` for the panning scalar.
- All sources which do panning require their panning strategy to be set up front via their constructor function.
  - A new `SYZ_PANNER_STRATEGY_DELEGATE` value may be used to instruct a source to use the context's default strategy,
    and is probably what most applications will want to use.
- All sources which do panning require that their panning related values be passed in via their constructors as well.
  These are the initial values you would otherwise have to set in the related properties.
  - Makes it possible to tweak crossfading stuff later.
- All constructors for audio objects take a `config` argument, which is reserved primarily for use in the future to
  improve automation.
  # 0.10.0

## Changes in this Release

- Synthizer no longer deinitializes while another thread is calling into it and specifically chekcs for double init and
  a second init in the same process.
- Fix `syz_handleIncRef` and `syz_handleDecRef` to work concurrently while another thread is deinitializing the library.
- Make it possible to use `syz_handleIncRef` and `syz_handleDecRef` without the library being initialized.  Useful for
  Rust bindings.
- Add a script to vendor Synthizer, so that it can be embedded into bindings without cloning the entire repository.
- Make `syz_LibraryConfig.libsndfile_path` const-correct.
- Make `syz_initializeWithConfig` const-correct.
- Fix: Python bindings now again expose `get_channels` on `Buffer`.
- `synthizer.h` no longer makes any attempt to document.  The authoritative source of truth is this manual; trying to
  keep both in sync was only a recipe for disaster.
- Fix: Synthizer no longer crashes at program exit due to internal un-freed pointers that might try to go through the
  deferred freeing machinery.

## Incompatibilities in this Release

NOTE: the Python bindings are unchanged.

- The following interfaces are now const-correct:
  - `syz_LibraryConfig.libsndfile_path`
  - `syz_initializeWithConfig`
  - `syz_globalEchoSetTaps`
  - `syz_routingConfigRoute`
  - `syz_globalEchoSetTaps`
- Widen the fields of `syz_RouteConfig` and `syz_EchoTapConfig`  to double.
- Also widen the `fade_out` parameter to `syz_routingRemoveRoute`.  This removes all unnecessary `float` from
  synthizer.h.
- All construcctors take a userdata and userdata free callback as parameters; if you don't need this functionality, set
  them to NULL.
- Renamed `syz_getUserdata` and `syz_setUserdata` to `syz_handleGetUserdata` and `syz_handleSetUserdata`.

# 0.9.0

This release primarily focuses on being the final major API breakage before 1.0, supporting Mac, improving the story
around thread safety, and closing the gaps around loading audio from non-file sources.

It is anticipated that there will be no more major API breakage before 1.0, and hopefully no more API breakage at all.
In some ways, this release is a 1.0 RC, though there are yet a few features left before 1.0 can happen.

If you find this library useful, [I have a GitHub Sponsors](https://github.com/sponsors/ahicks92).  This is by no means
required. That said, this release required buying a Mac, so I have now put money into Synthizer in addition to time.

## Stability of this Release

This is a weekend project and this release contains a lot of manually tested new features, so it will be astonishing if
you don't find bugs.  I generally do a good job fixing them within a week, so please [report them against the Synthizer
repository](https://github.com/synthizer/synthizer/issues).  There will likely be a few point releases before all of the
bugs iron themselves out.

Automated testing is on the wishlist, but is infeasible due to lack of time and is generally a massive undertaking.

## New Features in this Release

### Mac and Arm Support

Synthizer now supports Os X, both the Python bindings and the C library.  Unlike other platforms, you will need to
install XCode for the Python bindings to work.

As a consequence, ARM is now also supported and testing is done locally on an M1 Mac.

Unfortunately, due to Apple and GitHub actions limitations, Synthizer is unable to support older versions of Mac
reliably.  Testing currently happens against Big Sur locally, and CI builds against catalina.  In general Synthizer will
make an effort to support the latest major release of OS X.

### A New Manual

This manual has been almost entirely rewritten, and the concepts and Python tutorial sections now contain a lot more
content.  You can now find more detailed information on common patterns, 3D audio, and more.  Additionally, many
sections have been split into more targeted and smaller sections, which should make things easier to find.

If you're using the Python bindings, you probably still need to read the source code, but there is now more than one
tutorial.

### Support for Custom Streams and In-memory Assets

Synthizer now supports:

- [Loading from custom streams](../concepts/custom_streams.md).
- Loading buffers from in-memory encoded data via `syz_createBufferFromEncodedData`.
- Loading buffers from application-generated float arrays via `syz_createBufferFromFloatArray`.

### Libsndfile Support

Synthizer can now [load Libsndfile](../concepts/libsndfile.md).  This adds support for all formats Libsndfile supports,
most notably Ogg, and works with all forms of getting data into Synthizer save custom streams that don't implement
seeking functionality.  See the linked section for more info.

Due to licensing incompatibilities, this can't be statically linked and you will need to distribute the shared object
alongside your app.

### Ability to Configure Objects to Play Until Finished

Synthizer now has a concept of [lingering](../concepts/lingering.md), which can be used to configure Synthizer
generators and sources to stick around until they've finished playing rather than dying immediately on deletion.  This
replaces home-grown solutions using events with a built-in solution for fire-and-forget situations where you simply want
to play one sound and don't want to have to track it yourself.

### Reference-counted Handles

For lower-level languages, Synthizer now supports reference counted handles. This allows for implementing things like
the C++ `shared_ptr` or Rust's `Arc`, where objects may be being used by multiple threads and must not be deleted until
all threads are finished.

### Thread-safe Event Handling

As explained in [the new section on events](../concepts/events.md), Synthizer events now take temporary references on
the handles they refer to.  This allows event handling loops to run concurrently with handle deletion without
invalidating handles out from under the event handler.  This increased lifetime only extends until `syz_eventDeinit`,
but it is possible to use `syz_handleIncRef` to hold onto the handle for longer if desired.

### No Property Type Overloading

Synthizer properties are always only of one type by constant.  For example `SYZ_P_POSITION` is always a d3.  This
required renaming `SYZ_P_POSITION` on generators to `SYZ_P_PLAYBACK_POSITION`.

### The Effects API Is No Longer Provisional

At this point, it's not going to change again, so we have removed the provisional notices in this manual.

## Incompatibilities in this Release

- You now destroy handles with `syz_handleDecRef` because they're reference counted.
  - In Python, `.destroy` is now `.dec_ref`.  Python will likely take advantage of reference counting in future.
- `syz_echoSetTaps` is now `syz_globalEchoSetTaps`.
- All of the `SYZ_P_XXX` properties on the context for configuring defaults for the 3D environment have a default
  prefix, e.g. `SYZ_P_DEFAULT_PANNER_STRATEGY`.  This was done to make room for future extensions.
- All generators with a position now use `SYZ_P_PLAYBACK_POSITION`.
- Configuring logging and libsndfile etc. is now done through a struct passed to a new `syz_initializeWithConfig`.  All
  functions for configuring these values have been dropped.
- The union containing unused/empty event payloads has been dropped from `syz_Event` because they're unused/empty
  payloads.
  - To bindings developers: Synthizer currently doesn't contain unions anymore, but they'll come back in future.
- It is now necessary to call `syz_eventDeinit` on events when you're done with them.  Failure to do so will leak
  handles

# Synthizer 0.8.0

## Highlights of this Release

This release is about performance, and undoing the Linux rollback in 0.7.7.  As of this release, all code paths from the
external interface don't block on the audio thread in the best case.  An internal refactor and introduction of a new
command architecture unify a bunch of ad-hoc interfaces for fast inter-thread synchronization.

Specifically:

- Property reads are now a couple of atomic operations as opposed to a semaphore and waiting on the next audio tick. Put
  another way, they're literally millions of times faster, and it's now possible to read things like
  `BufferGenerator.position` without issue.
- property writes are now roughly 5 times faster than they used to be
- object creation is essentially free
- adding and removing generators from sources doesn't block
- configuring routes for effects doesn't block

There is one exception to the lack of blocking: if code does around 10000 operations per audio tick, Synthizer will have
no choice but to block due to internal resource exhaustion. If this proves to be an issue in practice, it is trivial to
raise this limit further.

## A Note on Breakage

The above changes were a complex refactor.  In order to make them with as little chaos as possible Synthizer has begun
introducing some basic unit tests, but nonetheless it is nearly impossible to make a release like this one without
issues.  You are encouraged to report issues against [Synthizer's GitHub
repository](https://github.com/synthizer/synthizer/issues).

## Compatibility

Synthizer introduces the following compatibility breakages in this release:

In order to get the above performance increases, it was necessary to remove the ability to read object properties.  This
has the side effect of hiding details of object lifetime, which Synthizer may rely on in future.

`FdnReverb.input_filter_enabled` and `FdnReverb.input_filter_cutoff` were removed.  Synthizer will be replacing these
with a  dedicated filter property type in the near future and opted to remove them now as to avoid an excessive number
of releases that introduce backward-incompatible changes.

Property reads are now actually eventually consistent.  The manual preivously stated that it's possible for a read that
comes after a write to read a stale value, so code shouldn't have been relying on it.  In any case, Synthizer now uses
this reservation for the above performance increases, and may break code that incorrectly relied on the old behavior.

Synthizer has also changed the default panner strategy to `SYZ_PANNER_STRATEGY_STEREO`.  To re-enable HRTF, set this on
a per-source basis.  The ability to set this default on a per-context basis will be introduced in the 0.8.x series. This
change was made because HRTF is only useful for headphone users, and it is not possible for Synthizer to reliably detect
that case.  In general, stereo panning is safe on every audio configuration including surround sound systems.

## Patch Notes

### 0.8.0

- Initial release
- Undo the rollbacks in 0.7.7 and reintroduce MSVC and Linux support.
- Internal fix for wire filters, which now don't play silence.
- A number of small fixes and slight quality improvements to reverb.
- All of the massive performance increases from above.


## 0.8.1

- Miniaudio was improperly configured to use a conservative profile, rather than a low latency profile. Caused extreme
  audio latency.

## 0.8.3

- Fix: FdnReverb.t60 = 0.0 no longer internally divides by 0.
- fix: BufferGenerator again uses seconds for positions.  This was refactored incorrectly during 0.8.0's major changes.
- Objects should now die faster when their handles are freed, due to holding only weak references internally where
  possible when dispatching commands to the audio thread.


## 0.8.4

- Contexts and all generators now support `SYZ_P_GAIN`.
- Contexts, sources, and generators now support play/pause via `syz_pause` and `syz_play`.
  - In Python, this is `src.pause()` and similar.
- As the first part of the event system, all Synthizer objects may now have arbitrary userdata associated with them:
  - In C, this is `syz_getUserdata` and `syz_setUserdata`
  - In Python, this is `get_userdata` and `set_userdata`
  - This will be documented properly when the rest of the event system exists, but can be used to link Synthizer objects
    to non-Synthizer objects without having to maintain a custom mapping yourself.
- Slight memory savings.
- Fix: deleting contexts no longer crashes.

## 0.8.5

- New function `syz_getObjectType` which queries the object type. Primarily intended for bindings developers.
- Fix: Synthizer no longer leaks megabytes of memory per second (issue #44).


## 0.8.6

This release contains somewhat experimental code to make decoding buffers faster by approximately 2X.  Please report any
perceptible audio quality issues with `BufferGenerator`.

- `syz_handleFree` now no-ops when passed `handle = 0`.
- Fix: deleting sources in specific orders no longer causes audio artifacts (issue #46)

## 0.8.7

- Fix: the library no longer crashes due to internal races between handle deletion and execution of audio commands on
  the background thread
  - This fixes a number of crashes, most notably issue #50, which highlighted the issue due to effects relying heavily
    on commands.
- Fix: streaming generators no longer spuriously seek to the beginning of their audio after generating the first few MS
  of audio (issue #49)
- Fix: `panning_scalar` will once more properly take effect if set immediately after `PannedSource` creation (issue #52)


## 0.8.8

- An [event system](../concepts/events.md).  This is alpha, and changes are expected in 0.9.
- Add `SYZ_P_PANNER_STRATEGY` to Context to default panner strategies for new sources.

## 0.8.9

- Fix: non-looping BufferGenerator with a pitch bend is no longer silent
- Fix/improvement: In Python, Synthizer events inherit from a common base class.

## 0.8.10

This release is the long-awaited improved HRTF.  This might not be the final version, but it's much better in quality
and the scripts to generate it are now much easier to maintain.  Please leave any feedback
[here](https://github.com/synthizer/synthizer/issues/27).  in particular, it is not feasible to test the entire sphere,
so it may be possible to find azimuth/elevation combinations which do weird things.

## 0.8.11-0.8.16

These releases are a test series for moving CI to GitHub Actions. Though not user-facing, 0.8.14 now blocks on
successful Linux CI for Ubuntu 20 and thus gives Linux support which is on par with Windows.

## 0.8.17

The flagship feature of this release is [filters](../concepts/filters.md). Python users should see [the Python
tutorial](../python_tutorials/filters.md). Also:

- Synthizer now builds on Ubuntu 18.
- Python no longer eroniously exports top-level enums due to undocumented Cython behavior.
  - If you were using ints as property values instead of enums or relying on the topp-level definition, you will need to
    update your code.  This was never intended to be part of the public API.
- It is possible to set generator gain before adding it to a source without triggering a crossfade. Internal changes to
  the crossfade helpers should make bugs like this much rarer, and probably fixed others that were yet to be reported.

# Release Notes for 0.0.x to 0.8.x

Starting at 0.8 and later, Synthizer maintains separate pages for every incompatible release with more details on
compatibility breakage and what specifically changed. The following were early development versions, and should no
longer be used.

## 0.7.7

- Identical to 0.7.5.  0.7.6 introduced a major performance regression that makes Synthizer unusable (probably issue
  #32, but investigations are ongoing). After this is fixed, the next version will contain all the nice 0.7.6 things.

## 0.7.6

- Synthizer now builds on Linux.  This is preliminary.  If you experience issues, please report them, as Linux isn't my
  primary platform.  That said, I have received multiple confirmations that it works.
- Python now publishes source distributions, which are capable of building without any extra intervention on your part,
  except for Windows which requires being in an appropriate MSVC shell.  On Linux, you can install Synthizer into
  virtualenvs with a simple `pip install synthizer`, assuming a supported C and C++ compiler.
  - Note that git is currently required in order to clone the Synthizer repository. We might include Synthizer inline in
    future.
- Prebuilt C artifacts for Windows now link against the multithreaded dynamic CRT. This was done to speed up Python
  builds and because there is no obvious right choice. If you need a different configuration, please build from source
  specifying `CMAKE_MSVC_RUNTIME_LIBRARY` and `SYNTHIZER_LIB_TYPE` to appropriate values.
- The audio generation all runs inline in the audio callback. This has the knock-on effect of making. property reads
  even slower than they were already, but is necessary to work well on Linux. See issue #32 for tracking fast property
  reads.

## 0.7.5

- Introduce [reverb](../object_reference/global_fdn_reverb.md).
- Start building Python 3.9 wheels.
- Sources now fade gain changes to prevent clicks, especially when feeding effects.
- Expose `syz_resetEffect` for interactive experimentation purposes, to clear the internal state of effects. In python
  this is `.reset()` on any effect.
- All effects now have a `SYZ_P_GAIN` property to set the overall gain of the effect.
- internally fix the bitsets, which manifested as weird issues with allocating sources to panner lanes, for example
  issue #16 where PannedSource could become silent.  Note that this may introduce other issues in exchange: it was
  assumed that the bitsets were working this entire time.

## 0.7.4

- Document our [versioning and stability guarantees](../concepts/stability.md).
- Introduce [effect routing support](../concepts/effects.md).
- Introduce an [Echo](../object_reference/global_echo.md) effect.

## 0.7.3

- Fix pitch bend support on BufferGenerator when looping is enabled. For the morbidly curious there is indeed a
  difference between `std::fmod` and `std::remainder`
- Maybe fix bugs in stereo panning for the same reason. If you had issues with stereo panning and didn't report them,
  you should probably try this release.

## 0.7.2

- New property on BufferGenerator: `SYZ_P_PITCH_BEND`.
- We will adopt pre-1.0 semantic versioning going forward in order to be compatible with Rust-style `^` dependencies:
  `0.major.minor`, with minor incrementing for features and major incrementing for incompatible API updates. This should
  enable at least some compatibility in version numbers across package managers.

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

This release temporarily disables dr_flac SIMD support until resolution of [this upstream
issue](https://github.com/mackron/dr_libs/issues/143).

## 0.6.0

### Features

- Major improvements to StreamingGenerator:
  - Position and looping are now exposed.
  - Streaming now happens in a background thread.
  - Streaming now builds up a buffer which prevents underruns.
  - Note that this uses a bunch of as-yet-untested threading/concurrency stuff that hasn't been exercised heavily, so if
    you notice bugs please open issues.
- Introduce a stereo panning strategy. Note that this will become the default panning strategy in the future, because
  it's the only one that's safe on all speaker arrangements. If you want HRTF, request it by setting
  `SYZ_P_PANNER_STRATEGY` on the context and/or sources.

### Bugs Fixed

- Throw an exception instead of silently crashing on invalid audio files.
- Fix the fundamentally broken DirectSource mixing logic. This probably still needs some improvement but is no longer
  fundamentally broken.
- Fix Stream seeking when using LookaheadByteStream internally. This fixes/allows for StreamingGenerator to seek, and
  may also fix  loading of audio files into buffers in some cases.
  ## 0.5.0

- Roll out a deferred freeing strategy, which uses C++ custom allocators to move freeing pointers to a background thread
  that wakes up on a period. This doesn't move all freeing, but gets a vast majority of it. The impact is that it will
  take a little while for any large memory allocations to actually free, but that freeing will (mostly) not happen on
  any thread but the freeing thread, including threads from users of the public API.  This is the first step to
  eventually decreasing latency below 20MS or so, though the upshot of that work won't be seen for a while.
- Decouple Buffer from context. The manual already claimed that we did, but the library itself didn't. Calls to create
  buffers no longer needa context passed in.
- Gains are now scalars instead of DB. This is because it is valuable to be able to know which combinations of gains sum
  to 1 in order to prevent clipping.
  - For reference, you can get a scalar gain from DB as `10**(db / 20)` if you need it.
- Fix creating buffers of files which require resampling.

## 0.4.1

- Fixes to allocating multiple panner lanes from a PannerBank.
  - Manifested as the inability to play multiple sources.
- Fix HRTF computation for angles outside the built-in HRTF dataset, but which are still in range for elevation.

## 0.4.0

- Introduce DirectSource, for music and other non-panned assets.
- All sources now have gain, as part of refactoring for DirectSource.
- Introduce `syz_bufferGetChannels`, `syz_bufferGetLengthInSamples`, and `syz_bufferGetLengthInSeconds`, and obvious
  Python equivalents.
- Source add/remove generators needed to happen in the audio thread in order to not break. For now this is using
  `Context.call`, but in future it'll probably use a ConcurrentQueue to enqueue updates and avoid the overhead, or
  perhaps an abstracted PropertyRing.
- Move to [moodycamel::ConcurrentQueue](https://github.com/cameron314/concurrentqueue). This removes mutexes in a lot of
  places
  - As part of this, get rid of our custom lockfree queues. ConcurrentQueue is better; it's MPMC and not intrusive.
- Get rid of some dead code.
- Fix double properties that use P_DOUBLE_MIN to mean minimum double, not 0. This bug brought to you by surprising C++
  numeric_limits behavior.
- Fix looping in `BufferGenerator`.
- Offer additional guarantees around `syz_getX` behavior with respect to properties that Synthizer modifies, like
  generator positions.
- CONTRIBUTING.md and pull request templates.

## 0.3.0

- Added support for Flac and MP3 via [dr_libs](https://github.com/mackron/dr_libs).
- Extended byte streams to be able to advertise their size.
