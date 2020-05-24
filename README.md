# Synthizer

[![Build status](https://ci.appveyor.com/api/projects/status/1rm9gb8cythqc14q/branch/master?svg=true)](https://ci.appveyor.com/project/camlorn/synthizer/branch/master)

This will be 3D environmental audio for headphones eventually. There may also be surround sound support. Watch this space.

I'm doing this around work as time allows.

## Building

The obvious CMake stuff.  Requires Clang on Windows because we use the GCC/Clang vector extensions.

## Licensing

Licensed under the Unlicense (i.e. public domain, i.e. do anything you want with this code).  Credit is always nice but not required.

If you send me a patch I will require you to confirm:

- You have sole copyright of the code.
- You didn't write the code for an employer, or using an employer's resources.
- You are placing your contribution under the unlicense.
- To the best of your knowledge, your patch doesn't violate patents.

If you want to write contributions for this library on behalf of an employer, we should talk.  This is a good problem to have, but i'm waiting until I actually have that problem to solve it.  Likewise if you want to license patents for use by this library.

Third party dependencies (i.e. things which aren't under the unlicense) are in the `third_party` directory and may require attribution in source form.  It is a goal of this project to avoid third party dependencies which require attribution in binary form.  Should this prove impractical, any such exceptions will be tracked.
