# Build from the repository root: make env PLATFORM=linux-armv6
# The armv6 toolchain of elementumorg/cross-compiler:linux-armv6 is GCC 4.8 (no C++14), so this image uses
# a Bootlin toolchain instead: GCC 9.3, glibc 2.31 (as Raspberry Pi OS Bullseye) and an armv6 libstdc++.
FROM elementumorg/cross-compiler:base

ARG TOOLCHAIN=armv6-eabihf--glibc--stable-2020.08-1
ARG TOOLCHAIN_SHA256=539e31b3bdcf76c3d6710951c74fbe682e2de8da4d110b981e9108b656890f4a
RUN wget -qO /tmp/toolchain.tar.bz2 "https://toolchains.bootlin.com/downloads/releases/toolchains/armv6-eabihf/tarballs/${TOOLCHAIN}.tar.bz2" && \
    echo "${TOOLCHAIN_SHA256}  /tmp/toolchain.tar.bz2" | sha256sum -c - && \
    tar -xjf /tmp/toolchain.tar.bz2 -C /usr && \
    mv "/usr/${TOOLCHAIN}" /usr/armv6-eabihf && \
    /usr/armv6-eabihf/relocate-sdk.sh && \
    rm /tmp/toolchain.tar.bz2

ENV CROSS_TRIPLE="arm-buildroot-linux-gnueabihf" \
    CROSS_ROOT="/usr/armv6-eabihf" \
    PATH="/usr/armv6-eabihf/bin:${PATH}" \
    TARGET_SYSTEM="Linux" \
    TARGET_PROCESSOR="arm" \
    TARGET_CC="arm-buildroot-linux-gnueabihf-gcc" \
    TARGET_CXX="arm-buildroot-linux-gnueabihf-g++" \
    BOOST_CONFIG="using gcc : : arm-buildroot-linux-gnueabihf-g++ ;" \
    BOOST_OPTS="target-os=linux" \
    OPENSSL_OPTS="linux-armv4" \
    CMAKE_TOOLCHAIN_FILE="/usr/share/lt2http/cross.cmake"

COPY cmake/toolchains/cross.cmake /usr/share/lt2http/
COPY scripts/install_dependencies.sh scripts/versions.env /usr/share/lt2http/

# One layer per dependency, so a failing step does not rebuild the ones before it
RUN /usr/share/lt2http/install_dependencies.sh cmake backtrace
RUN /usr/share/lt2http/install_dependencies.sh boost
RUN /usr/share/lt2http/install_dependencies.sh openssl
RUN /usr/share/lt2http/install_dependencies.sh oatpp oatpp-swagger
RUN /usr/share/lt2http/install_dependencies.sh libtorrent
