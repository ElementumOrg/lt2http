FROM elementumorg/cross-compiler:android-x64

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
ENV BOOST_CC clang
ENV BOOST_CXX clang++
ENV BOOST_OS android
ENV BOOST_TARGET_OS linux
ENV BOOST_OPTS cxxflags=-fPIC cflags=-fPIC
RUN ./build-boost.sh

# Install OpenSSL
COPY build-openssl.sh /build/
ENV OPENSSL_OPTS linux-generic64 -fPIC
RUN ./build-openssl.sh

# Install CMake
COPY build-cmake.sh /build/
ENV CMAKE_CC gcc
ENV CMAKE_CXX g++
RUN ./build-cmake.sh

# Install Oatpp
COPY build-oatpp.sh /build/
ENV OATPP_CC ${CROSS_TRIPLE}-gcc
ENV OATPP_CXX ${CROSS_TRIPLE}-g++
RUN ./build-oatpp.sh

# Install Mimalloc
COPY build-mimalloc.sh /build/
ENV MI_CC ${CROSS_TRIPLE}-gcc
ENV MI_CXX ${CROSS_TRIPLE}-g++
RUN ./build-mimalloc.sh

# Install libtorrent
COPY build-libtorrent.sh /build/
ENV LT_CC ${CROSS_TRIPLE}-clang
ENV LT_CXX ${CROSS_TRIPLE}-clang++
ENV LT_PTHREADS TRUE
ENV LT_FLAGS -fPIC -fPIE
ENV LT_CXXFLAGS -Wno-macro-redefined -Wno-c++11-extensions
RUN ./build-libtorrent.sh
