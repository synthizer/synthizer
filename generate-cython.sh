#!/usr/bin/env bash
set -ex

# This generates cython .pxd files in bindings/python/synthizer.
# To use, pip install autopxd2 in  virtualenv then run the script.
# We don't run this as part of CI or the build process.
# You can ignore pragma once in main file warnings.

cd include
autopxd synthizer.h > ../bindings/python/synthizer/synthizer.pxd
autopxd synthizer_constants.h > ../bindings/python/synthizer/synthizer_constants.pxd
