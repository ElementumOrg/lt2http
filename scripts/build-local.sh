#!/usr/bin/env bash

set -ex

scripts_path=$(dirname "$(readlink -f "$0")")

BT_CC=${CROSS_TRIPLE}-gcc
BT_CXX=${CROSS_TRIPLE}-g++
${scripts_path}/build-backtrace.sh

BOOST_CC=${CROSS_TRIPLE}-gcc \
BOOST_CXX=${CROSS_TRIPLE}-g++ \
BOOST_OS=linux \
BOOST_TARGET_OS=linux \
${scripts_path}/build-boost.sh

OPENSSL_OPTS=linux-x86_64 \
${scripts_path}/build-openssl.sh

OATPP_CC=${CROSS_TRIPLE}-gcc \
OATPP_CXX=${CROSS_TRIPLE}-g++ \
${scripts_path}/build-oatpp.sh

# Install Mimalloc
MI_CC=${CROSS_TRIPLE}-gcc \
MI_CXX=${CROSS_TRIPLE}-g++ \
${scripts_path}/build-mimalloc.sh

# Install libtorrent
LT_CC=${CROSS_TRIPLE}-gcc \
LT_CXX=${CROSS_TRIPLE}-g++ \
LT_CXXFLAGS=-Wno-psabi \
${scripts_path}/build-libtorrent.sh

echo ">>> Installed local environment to ${CROSS_ROOT}"