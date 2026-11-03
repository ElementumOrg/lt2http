#!/bin/sh
# Push binaries to lt2http-binaries repo
set -x

version=$(git describe --tags)
make binaries
# The tag is moved to this build: deleting the remote one fails when it does not exist yet, which is fine
if ! (cd build/binaries && git tag -f "$version" && { git push --delete origin "$version" >/dev/null 2>&1; git push origin master --tags; }); then
  rm -rf build/binaries
  exit 1
fi
