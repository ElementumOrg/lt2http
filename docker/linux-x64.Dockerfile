# Build from the repository root: make env PLATFORM=linux-x64
FROM elementumorg/cross-compiler:linux-x64

ENV TARGET_SYSTEM="Linux" \
    TARGET_PROCESSOR="x86_64" \
    TARGET_CC="${CROSS_TRIPLE}-gcc" \
    TARGET_CXX="${CROSS_TRIPLE}-g++" \
    BOOST_CONFIG="using gcc : : ${CROSS_TRIPLE}-g++ ;" \
    BOOST_OPTS="target-os=linux" \
    OPENSSL_OPTS="linux-x86_64" \
    CMAKE_TOOLCHAIN_FILE="/usr/share/lt2http/cross.cmake"

COPY cmake/toolchains/cross.cmake /usr/share/lt2http/
COPY scripts/install_dependencies.sh scripts/versions.env /usr/share/lt2http/

# One layer per dependency, so a failing step does not rebuild the ones before it
RUN /usr/share/lt2http/install_dependencies.sh cmake backtrace
RUN /usr/share/lt2http/install_dependencies.sh boost
RUN /usr/share/lt2http/install_dependencies.sh openssl
RUN /usr/share/lt2http/install_dependencies.sh oatpp oatpp-swagger
RUN /usr/share/lt2http/install_dependencies.sh libtorrent
