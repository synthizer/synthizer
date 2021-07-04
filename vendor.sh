#!/bin/bash
#
# This script vendors Synthizer by copying the minimal set of files to build
# the C library.  It is used to produce copies of Synthizer that exclude everything unnecessary, like HRTF datasets.
# Run from the root of the repo, passing a path to a vendoring directory.
#
# Assumes parallel is available.
set -e

if [ -z $1 ]; then
    echo "Must specify a destination path"
    exit 1
fi

files=$(find . \
    -type f \
    -not -path '*build*' \
    -not -path '.git*' \
    -not -path 'bindings/*' \
    \( -name '*.cpp' \
    -or -name '*.hpp' \
    -or -name '*.h' \
    -or -name '*.c' \
    -or -name '*.txt' \) \
)

commands=$(for f in $files;do
    d=$(dirname $f)
    echo "bash -c 'mkdir -p $1/$d; cp $f $1/$f'"
done
)

echo "$commands" | parallel -j 5000


