cmake_minimum_required(VERSION 3.15.0)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# In perfect way this should be the same as the one inside of docker images, used for cross-compile
# set(CMAKE_C_COMPILER "gcc-6")
# set(CMAKE_CXX_COMPILER "g++-6")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(lt2http
    DESCRIPTION "libtorrent2http"
    HOMEPAGE_URL "https://github.com/ElementumOrg/lt2http"
    LANGUAGES CXX
)

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
include(FeatureOptionsSetup)

check_ipo_supported(RESULT ipo_supported OUTPUT error)

feature_option(USE_MIMALLOC "Enable use of MIMALLOC" OFF)
feature_option(USE_STATIC "Enable STATIC binary creation" OFF)
feature_option(USE_SANITIZERS "Enable sanitizers" OFF)
feature_summary(WHAT ALL)

if (USE_STATIC)
    set(Boost_USE_STATIC_RUNTIME ON)
endif()

set(Boost_USE_STATIC_LIBS ON)
set(OPENSSL_USE_STATIC_LIBS TRUE)

include_directories(
    src/external/cpp-httplib/
    src/external/json_struct/include/

    src/
)

add_subdirectory(src)
