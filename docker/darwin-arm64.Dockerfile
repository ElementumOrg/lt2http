# Build from the repository root: make env PLATFORM=darwin-arm64
FROM elementumorg/cross-compiler:darwin-arm64

ENV TARGET_SYSTEM="Darwin" \
    TARGET_PROCESSOR="arm64" \
    TARGET_CC="${CROSS_TRIPLE}-clang" \
    TARGET_CXX="${CROSS_TRIPLE}-clang++" \
    BOOST_CONFIG="using darwin : : ${CROSS_TRIPLE}-clang++ ;" \
    BOOST_OPTS="target-os=darwin architecture=arm address-model=64" \
    OPENSSL_OPTS="darwin64-arm64-cc" \
    CMAKE_TOOLCHAIN_FILE="/usr/share/lt2http/cross.cmake"

COPY cmake/toolchains/cross.cmake /usr/share/lt2http/
COPY scripts/install_dependencies.sh scripts/versions.env /usr/share/lt2http/

# One layer per dependency, so a failing step does not rebuild the ones before it
RUN /usr/share/lt2http/install_dependencies.sh cmake backtrace
RUN /usr/share/lt2http/install_dependencies.sh boost
RUN /usr/share/lt2http/install_dependencies.sh openssl
RUN /usr/share/lt2http/install_dependencies.sh oatpp oatpp-swagger
RUN /usr/share/lt2http/install_dependencies.sh libtorrent
