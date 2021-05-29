#!/usr/bin/env bash
#
# Build the Synthizer CI for Linux. REspects all the usual environment variables.
# Also takes PYVERSIONS=python3.8 python3.9. If present, will build Python against
# the specified versions. This is passed in because not all of the platforms we run on have the same ones.
set -ex

sudo apt update

# GitHub doesn't give us Ninja
sudo apt-fast install -y ninja-build virtualenv

# And we probably need the Python dev packages
for p in $PYVERSIONS;do
	sudo apt-fast install -y $p-dev
done

mkdir -p build
cd build
cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Release
ninja
cd ../bindings/python
mkdir -p venvs
for py in $PYVERSIONS; do
	virtualenv -p $py ./venvs/$py
	source venvs/$py/bin/activate
	pip install -U cython wheel
	BUILDING_SYNTHIZER=1 python setup.py bdist_wheel
	deactivate
done
