FROM elementumorg/cross-compiler:windows-x86

ENV CMAKE_FLAGS -DCMAKE_TOOLCHAIN_FILE=/build/toolchains/mingw.cmake

RUN mkdir -p /build
WORKDIR /build

ARG BOOST_VERSION
ARG BOOST_SHA256
ARG OPENSSL_VERSION
ARG OPENSSL_SHA256
ARG LIBTORRENT_VERSION
ARG OATPP_VERSION
ARG OATPP_SWAGGER_VERSION
ARG MIMALLOC_VERSION
ARG CMAKE_VERSION
ARG BT_VERSION

COPY deps /build/
COPY common.sh /build/
COPY toolchains /build/toolchains

# Install Backtrace
COPY build-backtrace.sh /build/
ENV BT_CC ${CROSS_TRIPLE}-gcc-posix
ENV BT_CXX ${CROSS_TRIPLE}-g++-posix
RUN ./build-backtrace.sh

# Install Boost.System
COPY build-boost.sh /build/
COPY boost-statx-disable.patch /build/
ENV BOOST_CC gcc
ENV BOOST_CXX g++
ENV BOOST_OS mingw32
ENV BOOST_TARGET_OS windows
ENV BOOST_OPTS address-model=32 architecture=x86 threadapi=win32
RUN ./build-boost.sh

# Install OpenSSL
COPY build-openssl.sh /build/
ENV OPENSSL_OPTS mingw
RUN ./build-openssl.sh

# Install CMake
COPY build-cmake.sh /build/
RUN ./build-cmake.sh

# Install Oatpp
COPY build-oatpp.sh /build/
ENV OATPP_CC ${CROSS_TRIPLE}-gcc-posix
ENV OATPP_CXX ${CROSS_TRIPLE}-g++-posix
RUN ./build-oatpp.sh

# Install Mimalloc
COPY build-mimalloc.sh /build/
ENV MI_CC ${CROSS_TRIPLE}-gcc-posix
ENV MI_CXX ${CROSS_TRIPLE}-g++-posix
RUN ./build-mimalloc.sh

# Install libtorrent
COPY build-libtorrent.sh /build/
ENV LT_CC ${CROSS_TRIPLE}-gcc-posix
ENV LT_CXX ${CROSS_TRIPLE}-g++-posix
ENV LT_FLAGS -lmswsock -DUNICODE -D_UNICODE -DIPV6_TCLASS=39
ENV LT_LDFLAGS -lmswsock
RUN ./build-libtorrent.sh
