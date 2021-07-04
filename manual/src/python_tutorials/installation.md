# Installation

We support Python 3.8 and later.  To install the Python bindings:

```bash
pip install synthizer
```

Note the following platform specific information:

On Windows, wheels for both 32-bit and 64-bit Python are uploaded to Pypi and no
build step is required.

On Linux, and mac, the Python bindings build themselves from source using a
vendored copy of Synthizer. Additionally, Linux requires the latest version of
gcc and g++, and Mac requires xcode.  CI currently tests against the latest
version of oSX supported by GitHub Actions, Ubuntu 18, and Ubuntu 20.
