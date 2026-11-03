# Build from the repository root: make env PLATFORM=android-x64
FROM elementumorg/cross-compiler:android-x64

ENV TARGET_SYSTEM="Linux" \
    TARGET_PROCESSOR="x86_64" \
    TARGET_CC="${CROSS_TRIPLE}-clang" \
    TARGET_CXX="${CROSS_TRIPLE}-clang++" \
    BOOST_CONFIG="using clang : : ${CROSS_TRIPLE}-clang++ ;" \
    BOOST_OPTS="target-os=android" \
    OPENSSL_OPTS="linux-generic64" \
    LIBTORRENT_FLAGS="-Wno-macro-redefined" \
    CMAKE_TOOLCHAIN_FILE="/usr/share/lt2http/cross.cmake"

COPY cmake/toolchains/cross.cmake /usr/share/lt2http/
COPY scripts/install_dependencies.sh scripts/versions.env /usr/share/lt2http/

# One layer per dependency, so a failing step does not rebuild the ones before it
RUN /usr/share/lt2http/install_dependencies.sh cmake backtrace
RUN /usr/share/lt2http/install_dependencies.sh boost
RUN /usr/share/lt2http/install_dependencies.sh openssl
RUN /usr/share/lt2http/install_dependencies.sh oatpp oatpp-swagger
RUN /usr/share/lt2http/install_dependencies.sh libtorrent
