#!/usr/bin/env bash
set -ex

scripts_path=$(dirname "$(readlink -f "$0")")
source "${scripts_path}/common.sh"
source "${scripts_path}/deps"

if [ ! -z "$USE_STATIC" ]
then
  OATPP_STATIC=""
fi

cd $CROSS_ROOT

echo ""
echo "Downloading Oatpp, version: ${OATPP_VERSION}"
if [ ! -f "${OATPP_VERSION}.tar.gz" ]; then
  wget -q https://github.com/oatpp/oatpp/archive/refs/tags/${OATPP_VERSION}.tar.gz
fi
rm -rf oatpp
tar -xzf ${OATPP_VERSION}.tar.gz
rm ${OATPP_VERSION}.tar.gz

echo ""
echo "Compiling Oatpp, version: ${OATPP_VERSION}"
cd oatpp-${OATPP_VERSION}
find ./ -type f -exec sed -i -e 's/WinSock2.h/winsock2.h/i' {} \;
find ./ -type f -exec sed -i -e 's/WS2tcpip.h/ws2tcpip.h/i' {} \;

CC=${OATPP_CC} CXX=${OATPP_CXX} \
run cmake -B cmake-build-dir/release -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_STANDARD=14 -DBUILD_SHARED_LIBS=OFF -DOATPP_BUILD_TESTS=OFF \
  -DCMAKE_INSTALL_PREFIX=${CROSS_ROOT} \
  -DCMAKE_C_FLAGS="${OATPP_FLAGS}" -DCMAKE_CXX_FLAGS="${OATPP_FLAGS}" \
  $CMAKE_FLAGS
run cmake --build cmake-build-dir/release --parallel $(cat /proc/cpuinfo | grep processor | wc -l)
run cmake --install cmake-build-dir/release
rm -rf `pwd`

cd $CROSS_ROOT

echo ""
echo "Downloading Oatpp-Swagger, version: ${OATPP_SWAGGER_VERSION}"
if [ ! -f "${OATPP_SWAGGER_VERSION}.tar.gz" ]; then
  wget -q https://github.com/oatpp/oatpp-swagger/archive/refs/tags/${OATPP_SWAGGER_VERSION}.tar.gz
fi
rm -rf oatpp-swagger
tar -xzf ${OATPP_SWAGGER_VERSION}.tar.gz
rm ${OATPP_SWAGGER_VERSION}.tar.gz

echo ""
echo "Compiling Oatpp-Swagger, version: ${OATPP_SWAGGER_VERSION}"
cd oatpp-swagger-${OATPP_SWAGGER_VERSION}
CC=${OATPP_CC} CXX=${OATPP_CXX} \
run cmake -B cmake-build-dir/release -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_STANDARD=14 -DBUILD_SHARED_LIBS=OFF -DOATPP_BUILD_TESTS=OFF \
  -DCMAKE_INSTALL_PREFIX=${CROSS_ROOT} \
  -DCMAKE_C_FLAGS="${OATPP_FLAGS}" -DCMAKE_CXX_FLAGS="${OATPP_FLAGS}" \
  $CMAKE_FLAGS
run cmake --build cmake-build-dir/release --parallel $(cat /proc/cpuinfo | grep processor | wc -l)
run cmake --install cmake-build-dir/release
rm -rf `pwd`