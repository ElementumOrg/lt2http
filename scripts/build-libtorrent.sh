#!/usr/bin/env bash
set -ex

scripts_path=$(dirname "$(readlink -f "$0")")
source "${scripts_path}/common.sh"
source "${scripts_path}/deps"

if [ ! -z "$USE_STATIC" ]
then
  LT_STATIC="-Dstatic_runtime=ON"
fi

cd $CROSS_ROOT

echo
echo "Downloading Libtorrent, version: ${LIBTORRENT_VERSION}"
rm -rf libtorrent-`echo ${LIBTORRENT_VERSION} | sed 's/\\./_/g'`/
if [ ! -f "${LIBTORRENT_VERSION}.tar.gz" ]; then
  wget -q https://github.com/arvidn/libtorrent/archive/`echo ${LIBTORRENT_VERSION} | sed 's/\\./_/g'`.tar.gz
fi
rm -rf libtorrent
tar -xzf ${LIBTORRENT_VERSION}.tar.gz
rm ${LIBTORRENT_VERSION}.tar.gz
# git clone --no-tags --single-branch --branch "${LIBTORRENT_BRANCH}" --shallow-submodules --recurse-submodules -j"$(nproc)" --depth 1 "https://github.com/arvidn/libtorrent.git" libtorrent-`echo ${LIBTORRENT_VERSION} | sed 's/\\./_/g'`/

echo
echo "Compiling Libtorrent, version: ${LIBTORRENT_VERSION}"
cd libtorrent-`echo ${LIBTORRENT_VERSION} | sed 's/\\./_/g'`/

# Removing inclusion of Iconv
sed -i 's/find_public_dependency(Iconv)//' ./CMakeLists.txt
CC=${LT_CC} CXX=${LT_CXX} \
run cmake -B cmake-build-dir/release -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_STANDARD=14 \
  -DBUILD_SHARED_LIBS=OFF ${LT_STATIC} \
  -DCMAKE_C_FLAGS="${LT_FLAGS}" -DCMAKE_CXX_FLAGS="${LT_FLAGS}" \
  -DCMAKE_INSTALL_PREFIX=${CROSS_ROOT} \
  $CMAKE_FLAGS
run cmake --build cmake-build-dir/release --parallel $(cat /proc/cpuinfo | grep processor | wc -l)
run cmake --install cmake-build-dir/release

# Modify Cmake file to name include properly
sed -i 's/lIphlpapi/liphlpapi/' ${CROSS_ROOT}/lib/pkgconfig/libtorrent-rasterbar.pc
find ${CROSS_ROOT}/lib/cmake/LibtorrentRasterbar/ -type f -exec sed -i 's/Iphlpapi/iphlpapi/' {} \;
rm -rf `pwd`
