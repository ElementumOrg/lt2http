#!/usr/bin/env bash
set -ex

scripts_path=$(dirname "$(readlink -f "$0")")
source "${scripts_path}/common.sh"
source "${scripts_path}/deps"

if [ ! -z "$USE_STATIC" ]
then
  BT_STATIC=""
fi

cd $CROSS_ROOT

echo
echo "Downloading backtrace, version: ${BT_VERSION}"
if [ ! -f "${BT_VERSION}.tar.gz" ]; then
  wget -q https://github.com/ianlancetaylor/libbacktrace/archive/${BT_VERSION}.tar.gz
fi
rm -rf libbacktrace
tar -xzf ${BT_VERSION}.tar.gz
rm ${BT_VERSION}.tar.gz

echo
echo "Compiling backtrace, version: ${BT_VERSION}"
cd libbacktrace-${BT_VERSION}

CC="${BT_CC}" CXX="${BT_CXX}" \
CFLAGS="${CFLAGS} ${BT_FLAGS}" \
CXXFLAGS="${CXXFLAGS} ${BT_CXXFLAGS} ${CFLAGS}" \
LIBS="${BT_LIBS}" \
run ./configure --prefix=${CROSS_ROOT} --host=${CROSS_TRIPLE}
run make -j$(cat /proc/cpuinfo | grep processor | wc -l)
run make install
rm -rf `pwd`