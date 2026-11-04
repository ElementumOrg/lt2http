#!/bin/bash
# Development helper: builds lt2http, installs it into a local Kodi's service.lt2http and restarts it there.
#
#   test_build.sh <build> [kodi]
#     build: local, sanitize (native, needs 'make local-env'), docker, docker-library
#     kodi:  linux (default, ~/.kodi) or wsl (the Windows Kodi of $WSL_USER, docker builds only)
set -e

build=$1
kodi=${2:-linux}
port=${LT2HTTP_PORT:-65225}

case "$kodi" in
linux)
  kodi_dir="$HOME/.kodi"
  platform=linux-x64
  binary=lt2http library=lt2http.so
  ;;
wsl)
  kodi_dir="/mnt/c/Users/${WSL_USER:?WSL_USER must be set to the Windows user name}/AppData/Roaming/Kodi"
  platform=windows-x64
  binary=lt2http.exe library=lt2http.dll
  ;;
*)
  build=
  ;;
esac

case "$build" in
local | sanitize)
  if [ "$platform" != linux-x64 ]; then
    echo "Native builds are for Linux only, use 'docker' or 'docker-library' for Windows"
    exit 1
  fi
  if [ "$build" = sanitize ]; then
    make local BUILD_TYPE=RelWithDebInfo CMAKE_FLAGS=-Denable_sanitizers=ON
  else
    make local CMAKE_FLAGS=-Denable_sanitizers=OFF
  fi
  files=(build/cmake/local/lt2http)
  ;;
docker)
  make "$platform"
  files=("build/$platform/$binary")
  ;;
docker-library)
  make "$platform-shared"
  files=("build/$platform/$library")
  ;;
*)
  echo "Usage: $0 local|sanitize|docker|docker-library [linux|wsl]"
  exit 1
  ;;
esac

addon_dir="$kodi_dir/addons/service.lt2http"
if [ ! -f "$addon_dir/addon.xml" ]; then
  echo "service.lt2http is not installed in $kodi_dir, install it first (service.lt2http's lt2http-copy task)"
  exit 1
fi

# The add-on copies resources/bin/<platform> into its profile folder when the files change. It downloads the release
# binaries over them when the version stamp is older than the add-on, so stamp them with the add-on version.
bin_dir="$addon_dir/resources/bin/$platform"
mkdir -p "$bin_dir"
for file in "${files[@]}"; do
  # Windows refuses to overwrite a loaded file but allows renaming it
  mv -f "$bin_dir/$(basename "$file")" "$bin_dir/$(basename "$file").old" 2>/dev/null || true
  rm -f "$bin_dir/$(basename "$file").old" 2>/dev/null || true
  cp -f "$file" "$bin_dir/"
  echo "Installed $file into $bin_dir"
done
version=$(grep -o '<addon [^>]*' "$addon_dir/addon.xml" | grep -o ' version="[^"]*"' | cut -d'"' -f2)
echo "v$version" >"$bin_dir/version"

# The add-on starts lt2http again after it exits, with the new binary. WSL reaches Windows' 127.0.0.1 with mirrored networking.
if curl -s -m 2 -o /dev/null "http://127.0.0.1:$port/shutdown"; then
  echo "Stopped lt2http on port $port, the add-on starts the new one (in its client mode, start it yourself)"
else
  echo "lt2http is not running on port $port, it starts with Kodi"
fi
if [ "$build" = docker-library ]; then
  echo "Kodi keeps a loaded library until it exits: restart Kodi to use the new one"
fi
