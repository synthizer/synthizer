# Synthizer

[![Build status](https://ci.appveyor.com/api/projects/status/1rm9gb8cythqc14q/branch/master?svg=true)](https://ci.appveyor.com/project/camlorn/synthizer/branch/master)

[Documentation](https://synthizer.github.io/)
[Python bindings](https://pypi.org/project/synthizer/)

Synthizer is a library for game/VR audio applications.  The goal is that you statically link it and it does everything you need from file decoding and asset caching all the way down the stack.
A primary goal of this project is to also offer good, highly optimized HRTF.

This is still very much WIP, though we're just about at the point where you might seriously consider trying it.

## Building

The obvious CMake stuff.  Requires Clang on Windows because we use the GCC/Clang vector extensions.

## Licensing

Licensed under the Unlicense (i.e. public domain, i.e. do anything you want with this code).  Credit is always nice but not required.

If you want to contribute, see CONTRIBUTING.md for directions, especially w.r.t. licensing.  IIf you
aren't an individual or in the position to name/get sign off from all contributors, I consider that a good problem to have and we can talk about how to accommodate your contributions.

Third party dependencies (i.e. things which aren't under the unlicense) are in the `third_party` directory and may require attribution in source form.  It is a goal of this project to avoid third party dependencies which require attribution in binary form.  Should this prove impractical, any such exceptions will be tracked.
