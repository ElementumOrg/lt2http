#!/usr/bin/env bash
set -ex

scripts_path=$(dirname "$(readlink -f "$0")")
source "${scripts_path}/common.sh"
source "${scripts_path}/deps"

cd $CROSS_ROOT

echo
echo "Downloading CMake, version: ${CMAKE_VERSION}"
if [ ! -f "cmake-${CMAKE_VERSION}.tar.gz" ]; then
  wget -O cmake-${CMAKE_VERSION}.tar.gz -q https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz
fi
rm -rf cmake
tar -xzf cmake-${CMAKE_VERSION}.tar.gz
rm cmake-${CMAKE_VERSION}.tar.gz
cp -Rf cmake-${CMAKE_VERSION}-linux-x86_64/* /usr/
rm -rf cmake-${CMAKE_VERSION}-linux-x86_64
