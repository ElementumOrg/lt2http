#!/bin/bash

CWD=$(pwd)
# BUILD_TYPE="Debug"
# BUILD_TYPE="RelWithDebInfo"
BUILD_TYPE="Release"
MAKE_PARALLEL="-j$(nproc)"
CMAKE_PARALLEL="$(nproc)"
BUILD_DIR="${CWD}/build"
LOCAL_DIR="cmake-build-dir/${BUILD_TYPE}"
INST_DIR="/usr"

CXX_STANDARD=17

BOOST_CC="gcc"
CC="gcc-6"
CXX="g++-6"

# Example of using Clang, instead of GCC
# BOOST_CC="clang"
# CC="clang-9"
# CXX="clang++-9"

export CC=$CC
export CXX=$CXX

set -x

echo "Building Boost"
CURRENT_BUILD_DIR="dependencies/boost/"
(
  mkdir -p $CURRENT_BUILD_DIR && \
  cd $CURRENT_BUILD_DIR && \
  ./bootstrap.sh --prefix=$INST_DIR && \
  echo "using ${BOOST_CC} ;" > ${HOME}/user-config.jam && \
  sudo ./b2 --with-date_time --with-system --with-chrono --with-random --with-program_options \
    --prefix=$INST_DIR link=static variant=release threading=multi cxxflags=-std=gnu++${CXX_STANDARD} install \
)
[ $? -eq 0 ] || exit 1

echo "Building Openssl"
CURRENT_BUILD_DIR="dependencies/openssl/"
(
  mkdir -p $CURRENT_BUILD_DIR && \
  cd $CURRENT_BUILD_DIR && \
  ./Configure threads no-shared linux-x86_64 --prefix=${INST_DIR} && \
  make clean && \
  make ${MAKE_PARALLEL} && \
  sudo make install \
)
[ $? -eq 0 ] || exit 1

echo "Building libtorrent"
CURRENT_BUILD_DIR="dependencies/libtorrent/"
(
  mkdir -p $CURRENT_BUILD_DIR && \
    cd $CURRENT_BUILD_DIR && \
    cmake -B ${LOCAL_DIR} -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DCMAKE_CXX_STANDARD=${CXX_STANDARD} -DBUILD_SHARED_LIBS=OFF -DCMAKE_INSTALL_PREFIX=${INST_DIR} && \
    cmake --build ${LOCAL_DIR} --parallel ${CMAKE_PARALLEL} && \
    sudo cmake --install ${LOCAL_DIR} \
)
[ $? -eq 0 ] || exit 1

echo "Building oatpp"
CURRENT_BUILD_DIR="dependencies/oatpp/"
(
  mkdir -p $CURRENT_BUILD_DIR && \
    cd $CURRENT_BUILD_DIR && \
    cmake -B ${LOCAL_DIR} -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=${CXX_STANDARD} -DBUILD_SHARED_LIBS=OFF -DOATPP_BUILD_TESTS=OFF -DCMAKE_INSTALL_PREFIX=${INST_DIR} && \
    cmake --build ${LOCAL_DIR} --parallel ${CMAKE_PARALLEL} && \
    sudo cmake --install ${LOCAL_DIR} \
)
[ $? -eq 0 ] || exit 1

echo "Building oatpp-swagger"
CURRENT_BUILD_DIR="dependencies/oatpp-swagger/"
(
  mkdir -p $CURRENT_BUILD_DIR && \
    cd $CURRENT_BUILD_DIR && \
    cmake -B ${LOCAL_DIR} -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=${CXX_STANDARD} -DBUILD_SHARED_LIBS=OFF -DOATPP_BUILD_TESTS=OFF -DCMAKE_INSTALL_PREFIX=${INST_DIR} && \
    cmake --build ${LOCAL_DIR} --parallel ${CMAKE_PARALLEL} && \
    sudo cmake --install ${LOCAL_DIR} \
)
[ $? -eq 0 ] || exit 1

echo "Building mimalloc"
CURRENT_BUILD_DIR="dependencies/mimalloc/"
(
  mkdir -p $CURRENT_BUILD_DIR && \
    cd $CURRENT_BUILD_DIR && \
    cmake -B ${LOCAL_DIR} -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=${CXX_STANDARD} -DBUILD_SHARED_LIBS=OFF -DOATPP_BUILD_TESTS=OFF -DCMAKE_INSTALL_PREFIX=${INST_DIR} && \
    cmake --build ${LOCAL_DIR} --parallel ${CMAKE_PARALLEL} && \
    sudo cmake --install ${LOCAL_DIR} \
)
[ $? -eq 0 ] || exit 1
