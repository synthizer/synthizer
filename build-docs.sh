#!/bin/bash
set -ex
cd manual
mdbook build
cd ..
cp -r manual/book/html docs
