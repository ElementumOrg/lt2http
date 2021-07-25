FROM elementumorg/cross-compiler:musl

RUN wget -q https://musl.cc/aarch64-linux-musl-cross.tgz -O cross.tgz && \
    tar -xzf cross.tgz -C /usr/ && \
    rm cross.tgz

ENV CROSS_TRIPLE aarch64-linux-musl
ENV CROSS_ROOT /usr/${CROSS_TRIPLE}-cross
ENV PATH ${CROSS_ROOT}/bin:${PATH}
ENV LD_LIBRARY_PATH ${CROSS_ROOT}/lib:${LD_LIBRARY_PATH}
ENV PKG_CONFIG_PATH ${CROSS_ROOT}/lib/pkgconfig:${PKG_CONFIG_PATH}
ENV USE_STATIC ON

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
COPY boost-statx-disable.patch /build/
ENV BOOST_CC gcc
ENV BOOST_CXX g++
ENV BOOST_OS linux
ENV BOOST_TARGET_OS linux
RUN ./build-boost.sh

# Install OpenSSL
COPY build-openssl.sh /build/
ENV OPENSSL_OPTS linux-aarch64
RUN ./build-openssl.sh

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
ENV LT_CC ${CROSS_TRIPLE}-gcc
ENV LT_CXX ${CROSS_TRIPLE}-g++
ENV LT_CXXFLAGS -Wno-psabi
RUN ./build-libtorrent.sh
