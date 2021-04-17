#!/usr/bin/env bash
set -ex

scripts_path=$(dirname "$(readlink -f "$0")")
source "${scripts_path}/common.sh"
source "${scripts_path}/deps"

if [ ! -z "$USE_STATIC" ]
then
  MI_STATIC=""
fi

cd $CROSS_ROOT

echo
echo "Downloading mimalloc, version: ${MIMALLOC_VERSION}"
if [ ! -f "v${MIMALLOC_VERSION}.tar.gz" ]; then
  wget -q https://github.com/microsoft/mimalloc/archive/refs/tags/v${MIMALLOC_VERSION}.tar.gz
fi
rm -rf mimalloc
tar -xzf v${MIMALLOC_VERSION}.tar.gz
rm v${MIMALLOC_VERSION}.tar.gz

echo
echo "Compiling mimalloc, version: ${MIMALLOC_VERSION}"
cd mimalloc-${MIMALLOC_VERSION}
find ./ -type f -exec sed -i -e 's/Windows.h/windows.h/i' {} \;
if [[ $MI_CC == *"686"* ]]; then
  sed -i -e 's/-redirect/-redirect32/i' CMakeLists.txt
fi

CC="${MI_CC}" CXX="${MI_CXX}" \
run cmake -B cmake-build-dir/release -DMI_BUILD_TESTS=OFF \
  -DMI_BUILD_SHARED=OFF \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_STANDARD=14 \
  -DCMAKE_C_FLAGS="${MI_FLAGS}" -DCMAKE_CXX_FLAGS="${MI_FLAGS}" \
  -DCMAKE_INSTALL_PREFIX=${CROSS_ROOT} \
  $CMAKE_FLAGS
run cmake --build cmake-build-dir/release --parallel $(cat /proc/cpuinfo | grep processor | wc -l)
run cmake --install cmake-build-dir/release
rm -rf `pwd`