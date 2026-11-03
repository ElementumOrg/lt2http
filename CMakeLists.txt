cmake_minimum_required(VERSION 3.17)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules)
include(Lt2httpMacros)

project(lt2http
        DESCRIPTION "libtorrent2http - torrent streaming engine with an HTTP API"
        HOMEPAGE_URL "https://github.com/ElementumOrg/lt2http"
        LANGUAGES C CXX)

# Boost is found through its own BoostConfig.cmake, also from LibtorrentRasterbarConfig.cmake
if (POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif ()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

feature_option(build_library "Build lt2http as a shared library, started with startWithLogAndArgs()" OFF)
feature_option(static_runtime "Link the C++ runtime statically" ON)
feature_option(enable_sanitizers "Build with address, leak and undefined behavior sanitizers" OFF)
feature(lt2http_version STRING ON "Version reported by lt2http" "0.0.1")

set(Boost_USE_STATIC_LIBS ON)
set(OPENSSL_USE_STATIC_LIBS ON)

find_package(OpenSSL 1.1.1 REQUIRED)
find_package(Boost 1.72 CONFIG REQUIRED COMPONENTS program_options filesystem)
find_package(oatpp 1.3.0 REQUIRED)
find_package(oatpp-swagger 1.3.0 REQUIRED)
find_package(LibtorrentRasterbar 1.2.14 REQUIRED COMPONENTS torrent-rasterbar)
find_library(backtrace_library backtrace)

set(lt2http_sources
        src/app/application.cpp
        src/app/config.cpp
        src/app/main.cpp
        src/bittorrent/file.cpp
        src/bittorrent/memory_storage.cpp
        src/bittorrent/reader.cpp
        src/bittorrent/session.cpp
        src/bittorrent/torrent.cpp
        src/utils/empty_body.cpp
        src/utils/logger.cpp
        src/utils/stream_body.cpp
        src/web/basic_auth.cpp
        src/web/error_handler.cpp)

if (build_library)
    add_library(lt2http SHARED ${lt2http_sources})
    target_compile_definitions(lt2http PRIVATE LT2HTTP_LIBRARY)
    # lt2http.so / lt2http.dll on every platform, the add-on loads it by that name
    # Export only startWithLogAndArgs: Kodi may also load Elementum, which carries its own libtorrent
    set_target_properties(lt2http PROPERTIES PREFIX "" C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN ON)
    if (APPLE)
        set_target_properties(lt2http PROPERTIES SUFFIX ".so")
        target_link_options(lt2http PRIVATE -Wl,-exported_symbol,_startWithLogAndArgs)
    elseif (NOT WIN32)
        # The version script also hides "unique" template statics (like boost::asio's), which visibility does not.
        # --no-undefined reports missing symbols at build time instead of when Kodi loads the library.
        file(WRITE "${CMAKE_BINARY_DIR}/exports.map" "{ global: startWithLogAndArgs; local: *; };\n")
        target_link_options(lt2http PRIVATE -Wl,--no-undefined "-Wl,--version-script=${CMAKE_BINARY_DIR}/exports.map")
    endif ()
else ()
    add_executable(lt2http ${lt2http_sources})
endif ()

target_include_directories(lt2http PRIVATE
        src
        src/external/cpp-httplib
        src/external/json_struct/include)

target_compile_definitions(lt2http PRIVATE LH_VERSION="${lt2http_version}")

# changelog.md is served by /changelog: embed it, and configure again when it changes
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/changelog.md" lt2http_changelog)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS changelog.md)
configure_file(cmake/changelog.h.in "${CMAKE_BINARY_DIR}/generated/changelog.h" @ONLY)
target_include_directories(lt2http PRIVATE "${CMAKE_BINARY_DIR}/generated")

# Android before API 24 has no getifaddrs(), which cpp-httplib uses on non-Windows systems
if (NOT WIN32)
    include(CheckSymbolExists)
    check_symbol_exists(getifaddrs "ifaddrs.h" have_getifaddrs)
    if (NOT have_getifaddrs)
        target_sources(lt2http PRIVATE src/external/ifaddrs/ifaddrs.c)
        target_include_directories(lt2http BEFORE PRIVATE src/external/ifaddrs)
    endif ()
endif ()

target_link_libraries(lt2http PRIVATE
        oatpp::oatpp
        oatpp::oatpp-swagger
        LibtorrentRasterbar::torrent-rasterbar
        Boost::program_options
        Boost::filesystem
        OpenSSL::SSL OpenSSL::Crypto
        ${CMAKE_DL_LIBS})

if (WIN32)
    # oatpp is built with -fstack-protector-strong, mingw has __stack_chk_fail in libssp
    target_link_libraries(lt2http PRIVATE ssp)
endif ()

if (backtrace_library)
    target_compile_definitions(lt2http PRIVATE BOOST_STACKTRACE_USE_BACKTRACE)
    target_link_libraries(lt2http PRIVATE ${backtrace_library})
endif ()

if (static_runtime AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # libtorrent links libatomic on some 32-bit ARM targets, which devices may not have: link it statically too
    get_target_property(libtorrent_libs LibtorrentRasterbar::torrent-rasterbar INTERFACE_LINK_LIBRARIES)
    list(TRANSFORM libtorrent_libs REPLACE "^atomic$" "-l:libatomic.a")
    set_target_properties(LibtorrentRasterbar::torrent-rasterbar PROPERTIES INTERFACE_LINK_LIBRARIES "${libtorrent_libs}")
endif ()

if (static_runtime AND NOT APPLE)
    target_link_options(lt2http PRIVATE -static-libgcc -static-libstdc++)
    if (WIN32)
        # Also embeds winpthread, so the binary needs no mingw DLLs
        target_link_options(lt2http PRIVATE -static)
    endif ()
endif ()

if (enable_sanitizers)
    target_compile_options(lt2http PRIVATE -g -fsanitize=address,leak,undefined)
    target_link_options(lt2http PRIVATE -fsanitize=address,leak,undefined)
endif ()

feature_summary(DEFAULT_DESCRIPTION WHAT ALL)
