# Build from the repository root: make env PLATFORM=windows-x64
FROM elementumorg/cross-compiler:windows-x64

ENV TARGET_SYSTEM="Windows" \
    TARGET_PROCESSOR="x86_64" \
    TARGET_CC="${CROSS_TRIPLE}-gcc-posix" \
    TARGET_CXX="${CROSS_TRIPLE}-g++-posix" \
    TARGET_FLAGS="-D_WIN32_WINNT=0x0600 -DWIN32_LEAN_AND_MEAN" \
    BOOST_CONFIG="using gcc : : ${CROSS_TRIPLE}-g++-posix ;" \
    BOOST_OPTS="target-os=windows address-model=64 architecture=x86 threadapi=win32" \
    OPENSSL_OPTS="mingw64" \
    LIBTORRENT_FLAGS="-DIPV6_TCLASS=39 -DUNICODE -D_UNICODE" \
    CMAKE_TOOLCHAIN_FILE="/usr/share/lt2http/cross.cmake"

COPY cmake/toolchains/cross.cmake /usr/share/lt2http/
COPY scripts/install_dependencies.sh scripts/versions.env /usr/share/lt2http/

# One layer per dependency, so a failing step does not rebuild the ones before it
RUN /usr/share/lt2http/install_dependencies.sh cmake backtrace
RUN /usr/share/lt2http/install_dependencies.sh boost
RUN /usr/share/lt2http/install_dependencies.sh openssl
RUN /usr/share/lt2http/install_dependencies.sh oatpp oatpp-swagger
RUN /usr/share/lt2http/install_dependencies.sh libtorrent
