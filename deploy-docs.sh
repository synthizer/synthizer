#!/bin/bash
#
# Deploy docs manually. Normally done through CI.\
rm -rf ./synthizer.github.io
rm -rf ./manual/book
cd manual
mdbook build
cd ..
git clone git@github.com:synthizer/synthizer.github.io
rm -rf ./synthizer.github.io/*
cp -r ./manual/book/html/* ./synthizer.github.io/
cd synthizer.github.io
git add -A
git commit -m "Docs deployed at $(date)"
git push
cd ..
rm -rf ./synthizer.github.io
