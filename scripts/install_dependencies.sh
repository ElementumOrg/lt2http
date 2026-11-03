#!/usr/bin/env bash
# Builds and installs lt2http dependencies as static libraries into ${PREFIX}.
#
# Usage: install_dependencies.sh [cmake|backtrace|boost|openssl|oatpp|oatpp-swagger|libtorrent ...]
# Without arguments everything is installed. Behaviour is driven by environment variables,
# which the docker/<platform>.Dockerfile images set:
#   PREFIX            install prefix (default: ${CROSS_ROOT}, then /usr/local)
#   CROSS_TRIPLE      target triple, used as tool prefix
#   TARGET_CC/CXX     target C/C++ compilers (default: ${CROSS_TRIPLE}-gcc/g++, or cc/c++)
#   TARGET_FLAGS      compiler flags for every dependency (CMake ones get them from the toolchain file)
#   BOOST_CONFIG      b2 user-config line (default: "using gcc : : ${TARGET_CXX} ;")
#   BOOST_OPTS        extra b2 options, e.g. "target-os=windows address-model=64"
#   OPENSSL_OPTS      OpenSSL Configure target and options, e.g. "linux-x86_64" (default: host, via ./config)
#   LIBTORRENT_FLAGS  extra compiler flags for libtorrent
#   CMAKE_TOOLCHAIN_FILE  toolchain for every CMake based dependency
set -eo pipefail

scripts_path=$(dirname "$(readlink -f "$0")")
# shellcheck source=versions.env
source "${scripts_path}/versions.env"

: "${PREFIX:=${CROSS_ROOT:-/usr/local}}"
if [ -n "${CROSS_TRIPLE}" ]; then
  : "${TARGET_CC:=${CROSS_TRIPLE}-gcc}" "${TARGET_CXX:=${CROSS_TRIPLE}-g++}"
else
  : "${TARGET_CC:=cc}" "${TARGET_CXX:=c++}"
fi
: "${BOOST_CONFIG:=using gcc : : ${TARGET_CXX} ;}"
jobs=$(nproc)

deps=("$@")
[ ${#deps[@]} -eq 0 ] && deps=(cmake backtrace boost openssl oatpp oatpp-swagger libtorrent)

tmp_dir=$(mktemp -d)
trap 'rm -rf "${tmp_dir}"' EXIT

# download <url> [sha256]: unpacks the archive into a clean source directory and enters it
function download() {
  local archive="${tmp_dir}/archive"
  echo "- Downloading $1"
  wget -qO "${archive}" "$1"
  [ -z "$2" ] || echo "$2  ${archive}" | sha256sum -c -
  rm -rf "${tmp_dir}/src"
  mkdir "${tmp_dir}/src"
  tar -C "${tmp_dir}/src" --strip-components=1 -xf "${archive}"
  rm "${archive}"
  cd "${tmp_dir}/src"
}

function build_cmake() {
  # Boost (b2) and libtorrent hide their symbols, ld64 warns about every weak symbol whose visibility differs.
  # CMP0063: oatpp asks for an old CMake, which ignores the visibility of static libraries.
  local visibility=()
  [ "${TARGET_SYSTEM}" = "Darwin" ] && visibility=(-DCMAKE_CXX_VISIBILITY_PRESET=hidden -DCMAKE_VISIBILITY_INLINES_HIDDEN=ON
    -DCMAKE_POLICY_DEFAULT_CMP0063=NEW)
  cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=17 -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX="${PREFIX}" -DCMAKE_PREFIX_PATH="${PREFIX}" \
    ${CMAKE_TOOLCHAIN_FILE:+"-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}"} "${visibility[@]}" "$@"
  cmake --build build -j"${jobs}"
  cmake --install build
}

for dep in "${deps[@]}"; do
  echo "- Installing ${dep}"
  case "${dep}" in
  cmake)
    # Prebuilt host binary, the base images have none or an old one
    download "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz" "${CMAKE_SHA256}"
    cp -R bin share /usr/local/
    ;;
  backtrace)
    download "https://github.com/ianlancetaylor/libbacktrace/archive/${BACKTRACE_VERSION}.tar.gz"
    CC="${TARGET_CC}" CFLAGS="-O2 ${TARGET_FLAGS}" ./configure --prefix="${PREFIX}" ${CROSS_TRIPLE:+--host="${CROSS_TRIPLE}"} --with-pic --disable-shared
    make -j"${jobs}"
    make install
    ;;
  boost)
    download "https://archives.boost.io/release/${BOOST_VERSION}/source/boost_${BOOST_VERSION//./_}.tar.bz2" "${BOOST_SHA256}"
    ./bootstrap.sh --prefix="${PREFIX}"
    echo "${BOOST_CONFIG}" >user-config.jam
    # b2 calls plain ar/ranlib (libtool for the darwin toolset), so point those to the cross tools
    mkdir -p "${tmp_dir}/bin"
    for tool in ar ranlib libtool; do
      if command -v "${CROSS_TRIPLE}-${tool}" >/dev/null; then ln -sf "$(command -v "${CROSS_TRIPLE}-${tool}")" "${tmp_dir}/bin/${tool}"; fi
    done
    # statx is killed by seccomp on Android < 11 and some old kernels, the plain stat fallback works everywhere
    # shellcheck disable=SC2086
    PATH="${tmp_dir}/bin:${PATH}" ./b2 -j"${jobs}" --user-config=user-config.jam --prefix="${PREFIX}" \
      toolset="$(echo "${BOOST_CONFIG}" | awk '{print $2}')" \
      --with-system --with-program_options --with-filesystem --with-date_time --with-chrono --with-random \
      link=static variant=release threading=multi cxxflags="-std=c++17 -fPIC" cflags=-fPIC \
      define=BOOST_FILESYSTEM_DISABLE_STATX ${TARGET_FLAGS:+cxxflags="${TARGET_FLAGS}" cflags="${TARGET_FLAGS}"} ${BOOST_OPTS} install
    ;;
  openssl)
    download "https://github.com/openssl/openssl/releases/download/OpenSSL_${OPENSSL_VERSION//./_}/openssl-${OPENSSL_VERSION}.tar.gz" "${OPENSSL_SHA256}"
    # shellcheck disable=SC2086
    CC="${TARGET_CC}" AR="${CROSS_TRIPLE:+${CROSS_TRIPLE}-}ar" RANLIB="${CROSS_TRIPLE:+${CROSS_TRIPLE}-}ranlib" \
      "$([ -n "${OPENSSL_OPTS}" ] && echo ./Configure || echo ./config)" ${OPENSSL_OPTS} ${TARGET_FLAGS} threads no-shared no-tests -fPIC --prefix="${PREFIX}" --openssldir="${PREFIX}/ssl"
    # Only the libraries: the apps need windres on Windows and are not used
    make -j"${jobs}" build_libs
    make install_dev
    ;;
  oatpp | oatpp-swagger)
    version_var="$(echo "${dep}" | tr '[:lower:]-' '[:upper:]_')_VERSION"
    download "https://github.com/oatpp/${dep}/archive/refs/tags/${!version_var}.tar.gz"
    # mingw headers are lowercase and Linux file systems are case sensitive
    { grep -rlZ -e 'WinSock2.h' -e 'WS2tcpip.h' . || true; } | xargs -0r sed -i -e 's/WinSock2.h/winsock2.h/' -e 's/WS2tcpip.h/ws2tcpip.h/'
    if [ -f cmake/compiler-flags.cmake ]; then
      # oatpp always adds -g3, which only bloats the binaries
      sed -i '/"-g3"/d' cmake/compiler-flags.cmake
      # -fstack-clash-protection crashes mingw gcc 10 (internal compiler error in seh_emit_stackalloc)
      [ "${TARGET_SYSTEM}" != "Windows" ] || sed -i '/stack-clash-protection/d' cmake/compiler-flags.cmake
    fi
    build_cmake -DOATPP_BUILD_TESTS=OFF -DOATPP_LINK_ATOMIC=OFF
    ;;
  libtorrent)
    download "https://github.com/arvidn/libtorrent/releases/download/${LIBTORRENT_VERSION}/libtorrent-rasterbar-${LIBTORRENT_VERSION#v}.tar.gz"
    # Iconv is not needed (and not available for most targets)
    sed -i 's/find_public_dependency(Iconv)//' CMakeLists.txt
    build_cmake -DCMAKE_CXX_FLAGS="${LIBTORRENT_FLAGS}" -DCMAKE_C_FLAGS="${LIBTORRENT_FLAGS}"
    # libtorrent exports "Iphlpapi" for Windows, mingw only has lowercase libiphlpapi.a
    find "${PREFIX}/lib/cmake/LibtorrentRasterbar" -type f -exec sed -i 's/Iphlpapi/iphlpapi/g' {} +
    ;;
  *)
    echo "Unknown dependency: ${dep}" >&2
    exit 1
    ;;
  esac
  cd "${tmp_dir}" && rm -rf "${tmp_dir:?}/src"
done
