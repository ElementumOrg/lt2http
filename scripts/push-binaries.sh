#!/bin/sh
# Push binaries to lt2http-binaries repo
set -x

version=$(git describe --tags)
make binaries
cd build/binaries && \
    git tag -f $version && \
    git push --delete origin $version 2>&1 1>/dev/null && \
    git push origin master --tags

if [ $? -ne 0 ]; then
  cd .. && rm -rf build/binaries
  exit 1
fi
