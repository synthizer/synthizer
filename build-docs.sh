#!/bin/bash
set -ex
cd manual
mdbook build
cd ..
rm -rf docs/*
cp -r manual/book/html/* docs/
cd docs
git add -A
git commit -m "Build site at $(date)" --allow-empty
git push
