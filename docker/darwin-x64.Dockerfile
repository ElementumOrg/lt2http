FROM elementumorg/cross-compiler:darwin-x64

ENV CMAKE_FLAGS -DCMAKE_TOOLCHAIN_FILE=/build/toolchains/darwin.cmake -DCROSS_TRIPLE=${CROSS_TRIPLE} -DCROSS_ROOT=${CROSS_ROOT} 

RUN mkdir -p /build
WORKDIR /build

ARG BOOST_VERSION
ARG BOOST_SHA256
ARG OPENSSL_VERSION
ARG OPENSSL_SHA256
ARG SWIG_VERSION
ARG SWIG_SHA256
ARG LIBTORRENT_VERSION
ARG OATPP_VERSION
ARG OATPP_SWAGGER_VERSION
ARG MIMALLOC_VERSION

COPY deps /build/
COPY common.sh /build/
COPY toolchains /build/toolchains

# Install Backtrace
COPY build-backtrace.sh /build/
ENV BT_CC ${CROSS_TRIPLE}-gcc
ENV BT_CXX ${CROSS_TRIPLE}-g++
RUN ./build-backtrace.sh

# Install Boost.System
COPY build-boost.sh /build/
ENV BOOST_CC gcc
ENV BOOST_CXX g++
ENV BOOST_OS darwin
ENV BOOST_TARGET_OS darwin
ENV BOOST_FLAGS -fvisibility=hidden -fvisibility-inlines-hidden
RUN ./build-boost.sh

# Install OpenSSL
COPY build-openssl.sh /build/
ENV OPENSSL_OPTS darwin64-x86_64-cc
RUN ./build-openssl.sh

# Install CMake
COPY build-cmake.sh /build/
RUN ./build-cmake.sh

# Install Oatpp
COPY build-oatpp.sh /build/
ENV OATPP_CC ${CROSS_TRIPLE}-cc
ENV OATPP_CXX ${CROSS_TRIPLE}-g++
RUN ./build-oatpp.sh

# Install Mimalloc
COPY build-mimalloc.sh /build/
ENV MI_CC ${CROSS_TRIPLE}-cc
ENV MI_CXX ${CROSS_TRIPLE}-c++
RUN ./build-mimalloc.sh

# Install libtorrent
COPY build-libtorrent.sh /build/
ENV LT_CC ${CROSS_TRIPLE}-gcc
ENV LT_CXX ${CROSS_TRIPLE}-g++
ENV LT_CXXFLAGS -Wno-c++11-extensions -Wno-c++11-long-long
ENV LT_CFLAGS -DCMAKE_SYSTEM_NAME=Darwin
RUN ./build-libtorrent.sh
