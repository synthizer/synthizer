#!/usr/bin/env bash
#
# Build the Synthizer CI for Linux. REspects all the usual environment variables.
# Also takes PYVERSIONS=python3.8 python3.9. If present, will build Python against
# the specified versions. This is passed in because not all of the platforms we run on have the same ones.
set -ex

sudo apt update

# GitHub doesn't give us Ninja
sudo apt-fast install -y ninja-build virtualenv

git submodule update --init --recursive
mkdir -p build
cd build
cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Release
ninja

