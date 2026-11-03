#!/bin/bash
# Development helper: builds lt2http for linux-x64 and copies it into the local Kodi installation.
set -e

KODI_BIN_DIRS="$HOME/.kodi/addons/service.lt2http/resources/bin/linux-x64 $HOME/.kodi/userdata/addon_data/service.lt2http/bin/linux-x64"

case "$1" in
local)
  # Native build, needs dependencies from 'make local-env'
  make local
  files=build/cmake/local/lt2http
  ;;
sanitize)
  make local BUILD_TYPE=RelWithDebInfo CMAKE_FLAGS=-Denable_sanitizers=ON
  files=build/cmake/local/lt2http
  ;;
docker)
  make linux-x64 linux-x64-shared
  files="build/linux-x64/lt2http build/linux-x64/lt2http.so"
  ;;
*)
  echo "Usage: $0 local|sanitize|docker"
  exit 1
  ;;
esac

for dir in ${KODI_BIN_DIRS}; do
  mkdir -p "${dir}"
  cp -f ${files} "${dir}/"
done
