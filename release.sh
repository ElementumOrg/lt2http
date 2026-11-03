#!/bin/bash

set -e

TAG=$(git describe --tags)

git submodule update --init --recursive --depth=1

# Compile all binaries
mkdir -p build/binaries/
make all

# Run binaries uploads if we are on the tag
if [[ $TAG != *-* ]]
then
	make binaries
fi