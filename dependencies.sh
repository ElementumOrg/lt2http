#!/bin/bash

echo "Downloading dependencies"

CWD=$(pwd)
DESTDIR=${CWD}/dependencies/

# Include dependencies versions
source ${CWD}/deps

# Create global temporary directory
mkdir -p ${DESTDIR}
cd ${DESTDIR}
[ $? -eq 0 ] || { echo "Cannot cd into ${DESTDIR}"; exit 1; }

echo "Cleaning dependencies directory"
sudo rm -rf ${DESTDIR}/*
[ $? -eq 0 ] || { echo "Cannot cleanup directory ${DESTDIR}"; exit 1; }

echo
echo "Downloading Boost, version: ${BOOST_VERSION}"
if [ ! -f "boost_${BOOST_VERSION_FILE}.tar.bz2" ]; then
  wget -q https://dl.bintray.com/boostorg/release/${BOOST_VERSION}/source/boost_${BOOST_VERSION_FILE}.tar.bz2
fi
if [ ! -f "boost_${BOOST_VERSION_FILE}.tar.bz2" ]; then
  wget -q https://netcologne.dl.sourceforge.net/project/boost/boost/${BOOST_VERSION}/boost_${BOOST_VERSION_FILE}.tar.bz2
fi
echo "$BOOST_SHA256  boost_${BOOST_VERSION_FILE}.tar.bz2" | sha256sum -c -
rm -rf boost
tar -xjf boost_${BOOST_VERSION_FILE}.tar.bz2
rm boost_${BOOST_VERSION_FILE}.tar.bz2
mv boost_${BOOST_VERSION_FILE} boost


echo
echo "Downloading OpenSSL, version: ${OPENSSL_VERSION}"
if [ ! -f "openssl-${OPENSSL_VERSION}.tar.gz" ]; then
  wget -q https://www.openssl.org/source/openssl-${OPENSSL_VERSION}.tar.gz
fi
echo "$OPENSSL_SHA256  openssl-${OPENSSL_VERSION}.tar.gz" | sha256sum -c -
rm -rf openssl
tar -xzf openssl-${OPENSSL_VERSION}.tar.gz
rm openssl-${OPENSSL_VERSION}.tar.gz
mv openssl-${OPENSSL_VERSION} openssl


echo
echo "Downloading Libtorrent, version: ${LIBTORRENT_VERSION}"
if [ ! -f "${LIBTORRENT_VERSION}.tar.gz" ]; then
  wget -q https://github.com/arvidn/libtorrent/archive/`echo ${LIBTORRENT_VERSION} | sed 's/\\./_/g'`.tar.gz
fi
rm -rf libtorrent
tar -xzf ${LIBTORRENT_VERSION}.tar.gz
rm ${LIBTORRENT_VERSION}.tar.gz
mv libtorrent-`echo ${LIBTORRENT_VERSION} | sed 's/\\./_/g'`/ libtorrent


echo
echo "Downloading Oatpp, version: ${OATPP_VERSION}"
if [ ! -f "${OATPP_VERSION}.tar.gz" ]; then
  wget -q https://github.com/oatpp/oatpp/archive/refs/tags/${OATPP_VERSION}.tar.gz
fi
rm -rf oatpp
tar -xzf ${OATPP_VERSION}.tar.gz
rm ${OATPP_VERSION}.tar.gz
mv oatpp-${OATPP_VERSION} oatpp


echo
echo "Downloading Oatpp-Swagger, version: ${OATPP_SWAGGER_VERSION}"
if [ ! -f "${OATPP_SWAGGER_VERSION}.tar.gz" ]; then
  wget -q https://github.com/oatpp/oatpp-swagger/archive/refs/tags/${OATPP_VERSION}.tar.gz
fi
rm -rf oatpp-swagger
tar -xzf ${OATPP_SWAGGER_VERSION}.tar.gz
rm ${OATPP_SWAGGER_VERSION}.tar.gz
mv oatpp-swagger-${OATPP_SWAGGER_VERSION} oatpp-swagger


echo
echo "Finished downloading dependencies."
echo "Now run ./build.sh to compile and install them."
