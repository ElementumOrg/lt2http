#!/usr/bin/env bash

set -ex

scripts_path=$(dirname "$(readlink -f "$0")")
source "${scripts_path}/common.sh"
source "${scripts_path}/deps"

if [ ! -z "$USE_STATIC" ]
then
  BOOST_STATIC="runtime-link=static"
fi

cd $CROSS_ROOT

echo
echo "Downloading Boost, version: ${BOOST_VERSION}"
if [ ! -f "boost_${BOOST_VERSION_FILE}.tar.bz2" ]; then
  wget -q https://netcologne.dl.sourceforge.net/project/boost/boost/${BOOST_VERSION}/boost_${BOOST_VERSION_FILE}.tar.bz2
fi
if [ ! -f "boost_${BOOST_VERSION_FILE}.tar.bz2" ]; then
  wget -q https://dl.bintray.com/boostorg/release/${BOOST_VERSION}/source/boost_${BOOST_VERSION_FILE}.tar.bz2
fi
echo "$BOOST_SHA256  boost_${BOOST_VERSION_FILE}.tar.bz2" | sha256sum -c -
rm -rf boost
tar -xjf boost_${BOOST_VERSION_FILE}.tar.bz2
rm boost_${BOOST_VERSION_FILE}.tar.bz2

echo
echo "Compiling Boost, version: ${BOOST_VERSION}"
cd boost_${BOOST_VERSION_FILE}
echo "Patching Boost"
patch -p1 < ${scripts_path}/boost-statx-disable.patch
./bootstrap.sh --prefix=${CROSS_ROOT} ${BOOST_BOOTSTRAP_OPTS}
echo "using ${BOOST_CC} : ${BOOST_OS} : ${CROSS_TRIPLE}-${BOOST_CXX} ${BOOST_FLAGS} ;" > ${HOME}/user-config.jam
export PATH="$CROSS_ROOT/bin:$PATH"
run ./b2 --with-date_time --with-system --with-chrono --with-random --with-program_options --with-filesystem --prefix=${CROSS_ROOT} \
  toolset=${BOOST_CC}-${BOOST_OS} ${BOOST_OPTS} link=static variant=release ${BOOST_STATIC} threading=multi \
  target-os=${BOOST_TARGET_OS} install
rm -rf `pwd`
