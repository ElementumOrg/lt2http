#!/usr/bin/env bash
set -ex

scripts_path=$(dirname "$(readlink -f "$0")")
source "${scripts_path}/common.sh"
source "${scripts_path}/deps"

if [ ! -z "$USE_STATIC" ]
then
  OPENSSL_STATIC=""
fi

cd $CROSS_ROOT

echo
echo "Downloading OpenSSL, version: ${OPENSSL_VERSION}"
if [ ! -f "openssl-${OPENSSL_VERSION}.tar.gz" ]; then
  wget -q https://www.openssl.org/source/openssl-${OPENSSL_VERSION}.tar.gz
fi
echo "$OPENSSL_SHA256  openssl-${OPENSSL_VERSION}.tar.gz" | sha256sum -c -
rm -rf openssl
tar -xzf openssl-${OPENSSL_VERSION}.tar.gz
rm openssl-${OPENSSL_VERSION}.tar.gz

echo
echo "Compiling OpenSSL, version: ${OPENSSL_VERSION}"
cd openssl-${OPENSSL_VERSION}
# CROSS_COMPILE=${CROSS_TRIPLE}- run ./Configure -static --static threads no-shared ${OPENSSL_OPTS} --prefix=${CROSS_ROOT}
CROSS_COMPILE=${CROSS_TRIPLE}- run ./Configure threads no-shared ${OPENSSL_OPTS} --prefix=${CROSS_ROOT}
run make -j $(cat /proc/cpuinfo | grep processor | wc -l) 
run make install 
rm -rf `pwd`
