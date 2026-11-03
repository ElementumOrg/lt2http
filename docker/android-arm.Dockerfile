# Build from the repository root: make env PLATFORM=android-arm
FROM elementumorg/cross-compiler:android-arm

ENV TARGET_SYSTEM="Linux" \
    TARGET_PROCESSOR="arm" \
    TARGET_CC="${CROSS_TRIPLE}-clang" \
    TARGET_CXX="${CROSS_TRIPLE}-clang++" \
    BOOST_CONFIG="using clang : : ${CROSS_TRIPLE}-clang++ ;" \
    BOOST_OPTS="target-os=android" \
    OPENSSL_OPTS="linux-armv4" \
    LIBTORRENT_FLAGS="-Wno-macro-redefined -DINT64_MAX=0x7fffffffffffffffLL -DINT16_MAX=32767 -DINT16_MIN=-32768" \
    CMAKE_TOOLCHAIN_FILE="/usr/share/lt2http/cross.cmake"

COPY cmake/toolchains/cross.cmake /usr/share/lt2http/
COPY scripts/install_dependencies.sh scripts/versions.env /usr/share/lt2http/

# One layer per dependency, so a failing step does not rebuild the ones before it
RUN /usr/share/lt2http/install_dependencies.sh cmake backtrace
RUN /usr/share/lt2http/install_dependencies.sh boost
RUN /usr/share/lt2http/install_dependencies.sh openssl
RUN /usr/share/lt2http/install_dependencies.sh oatpp oatpp-swagger
RUN /usr/share/lt2http/install_dependencies.sh libtorrent
