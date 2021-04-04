cmake_minimum_required(VERSION 3.15.0)

set(CMAKE_VERBOSE_MAKEFILE ON)

# In perfect way this should be the same as the one inside of docker images, used for cross-compile
set(CMAKE_C_COMPILER "gcc-6")
set(CMAKE_CXX_COMPILER "g++-6")

# Example of setting compiler to clang-9
# set(CMAKE_C_COMPILER "clang-9")
# set(CMAKE_CXX_COMPILER "clang++-9")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(lt2http
    DESCRIPTION "libtorrent2http"
    HOMEPAGE_URL "https://github.com/ElementumOrg/lt2http"
    LANGUAGES CXX
)

set(Boost_USE_STATIC_LIBS ON)
set(OPENSSL_USE_STATIC_LIBS TRUE)

# Dependencies version requirements - older vesions may work, but you are on your own
set(minLibtorrentVersion 1.2.1)
set(minOpenSSLVersion 1.1.1)
set(minBoostVersion 1.66)
set(minOatppVersion 1.2.5)
set(minOatppSwaggerVersion 1.2.5)
set(minMimallocVersion 1.4)

include(CheckIPOSupported)
include(CheckCXXSourceCompiles)
include(FeatureSummary)

check_ipo_supported(RESULT ipo_supported OUTPUT error)

include_directories(
    src/external/cpp-httplib/
    src/external/json_struct/include/

    src/
)

add_subdirectory(src)
