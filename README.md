# Synthizer

![GitHub Actions](https://github.com/synthizer/synthizer/actions/workflows/ci.yaml/badge.svg)

[Documentation](https://synthizer.github.io/)

[Python bindings](https://pypi.org/project/synthizer/)

[GitHub Sponsors](https://github.com/sponsors/ahicks92)

##WARNING: UNMAINTAINED

This is unmaintained software. I am slowly moving toward a Rust only version of the library, but my primary focus is on other projects at this time.  PRs will not be accepted at this time.


Synthizer is a library for game/VR audio applications.  The goal is that you statically link it and it does everything
you need from file decoding and asset caching all the way down the stack.  Current features include:

- MP3, Wav, and Flac decoding
- Support for Libsndfile
- HRTF and stereo panning
- An FDN reverb model.
- Noise generators
- Fast, though benchmarks haven't been produced yet.  Suffice it to say that debug builds are fast enough for practical
  usage.
- Calls on the hot path don't block, and in general effort is made to prevent both allocation and other forms of
  syscall/kernel transitions.
- Public domain.  Credit is appreciated, but you can just drop it into your project without attribution if you want.

This is still beta-quality software, currently tracking a [1.0
milestone](https://github.com/synthizer/synthizer/milestone/2).  As of 0.9 most of what is intended to be in 1.0 exists:
you can certainly use this for a game or something.  But it's still pre-1.0 for a reason.

## Building and Platform Support

We currently support:

- Windows, using MSVC 2019 or Clang 9.
- Linux, using Clang 9 or GCC 9
- OSX catalina and later

For all platforms, a simple:

```
mkdir build
cd build
cmake -G Ninja ..
ninja
```

Suffices.  You may need manual intervention in order to set things such as the MSVC runtime, but all of that goes
through the standard CMake processes.

On Windows, you may need to run the above under an MSVC command prompt.

If you want to build a shared library:

```
cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Release -DSYNTHIZER_LIB_TYPE=SHARED
```

For windows, also add `-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL` if you want to statically link the MSVC runtime,
and avoid needing external dependencies.

For the Python bindings, one of the following applies:

- use the Windows wheels.  This is the easiest path for Windows.
- `pip install synthizer` will decide to use a source distribution on Linux and Mac, which will try to build Synthizer
  inline.  This requires that you have `git` but doesn't have any other external dependencies, not even CMake.


## Licensing

Licensed under the Unlicense (i.e. public domain, i.e. do anything you want with this code).  Credit is always nice but
not required.

If you want to contribute, see CONTRIBUTING.md for directions, especially w.r.t. licensing.  IIf you aren't an
individual or in the position to name/get sign off from all contributors, I consider that a good problem to have and we
can talk about how to accommodate your contributions.

Third party dependencies (i.e. things which aren't under the unlicense) are in the `third_party` directory and may
require attribution in source form.  It is a goal of this project to avoid third party dependencies which require
attribution in binary form.  Should this prove impractical, any such exceptions will be tracked.
