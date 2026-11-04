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

feature_option(static_runtime "Link the C++ runtime statically" ON)
feature_option(enable_sanitizers "Build with address, leak and undefined behavior sanitizers" OFF)
feature_option(build_tests "Build the unit tests, run with ctest" OFF)
feature(lt2http_version STRING ON "Version reported by lt2http" "0.0.1")
feature(lt2http_commit STRING ON "Git commit reported by lt2http" "unknown")

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
        src/bittorrent/file.cpp
        src/bittorrent/memory_storage.cpp
        src/bittorrent/natpmp.cpp
        src/bittorrent/reader.cpp
        src/bittorrent/session.cpp
        src/bittorrent/torrent.cpp
        src/utils/empty_body.cpp
        src/utils/http_client.cpp
        src/utils/logger.cpp
        src/utils/stream_body.cpp
        src/web/basic_auth.cpp
        src/web/error_handler.cpp
        src/web/files.cpp
        src/web/misc.cpp
        src/web/session.cpp
        src/web/settings.cpp
        src/web/stream.cpp
        src/web/swagger.cpp
        src/web/torrents.cpp)

# The binary (lt2http) and the shared library (lt2http_library) link the same objects, compiled once,
# only main.cpp differs: the library entry point is startWithLogAndArgs().
# The library needs position independent code and hidden symbols, the binary gets the same, so the objects can be shared.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

add_library(lt2http_objects OBJECT ${lt2http_sources})

target_include_directories(lt2http_objects PUBLIC
        src
        src/external/cpp-httplib
        src/external/json_struct/include)

# Most of the compile time is parsing the libtorrent, oatpp and Boost headers, which almost every file includes:
# parse them once per build. Not the oatpp codegen macros, they must be included in a specific order.
target_precompile_headers(lt2http_objects PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:<libtorrent/session.hpp$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:<libtorrent/alert_types.hpp$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:<libtorrent/torrent_info.hpp$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:<libtorrent/storage.hpp$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:<oatpp/web/server/api/ApiController.hpp$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:<oatpp/parser/json/mapping/ObjectMapper.hpp$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:<boost/filesystem.hpp$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:<boost/algorithm/string.hpp$<ANGLE-R>>")

# The version, commit and changelog.md (served by /changelog) go into one generated source file, so a new commit or
# changelog only recompiles that file. CMake configures again when changelog.md changes.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/changelog.md" lt2http_changelog)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS changelog.md)
set(build_info_source "${CMAKE_BINARY_DIR}/generated/build_info.cpp")
configure_file(cmake/build_info.cpp.in "${build_info_source}" @ONLY)
target_sources(lt2http_objects PRIVATE "${build_info_source}")

# Android before API 24 has no getifaddrs(), which cpp-httplib uses on non-Windows systems
if (NOT WIN32)
    include(CheckSymbolExists)
    check_symbol_exists(getifaddrs "ifaddrs.h" have_getifaddrs)
    if (NOT have_getifaddrs)
        target_sources(lt2http_objects PRIVATE src/external/ifaddrs/ifaddrs.c)
        target_include_directories(lt2http_objects BEFORE PRIVATE src/external/ifaddrs)
    endif ()
endif ()

# Public: the dependencies, definitions and options apply to main.cpp and the linking of both targets too
target_link_libraries(lt2http_objects PUBLIC
        oatpp::oatpp
        oatpp::oatpp-swagger
        LibtorrentRasterbar::torrent-rasterbar
        Boost::program_options
        Boost::filesystem
        OpenSSL::SSL OpenSSL::Crypto
        ${CMAKE_DL_LIBS})

if (WIN32)
    # oatpp is built with -fstack-protector-strong, mingw has __stack_chk_fail in libssp
    target_link_libraries(lt2http_objects PUBLIC ssp)
endif ()

if (backtrace_library)
    target_compile_definitions(lt2http_objects PUBLIC BOOST_STACKTRACE_USE_BACKTRACE)
    target_link_libraries(lt2http_objects PUBLIC ${backtrace_library})
endif ()

if (static_runtime AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # libtorrent links libatomic on some 32-bit ARM targets, which devices may not have: link it statically too
    get_target_property(libtorrent_libs LibtorrentRasterbar::torrent-rasterbar INTERFACE_LINK_LIBRARIES)
    list(TRANSFORM libtorrent_libs REPLACE "^atomic$" "-l:libatomic.a")
    set_target_properties(LibtorrentRasterbar::torrent-rasterbar PROPERTIES INTERFACE_LINK_LIBRARIES "${libtorrent_libs}")
endif ()

if (static_runtime AND NOT APPLE)
    target_link_options(lt2http_objects PUBLIC -static-libgcc -static-libstdc++)
    if (WIN32)
        # Also embeds winpthread, so the binary needs no mingw DLLs
        target_link_options(lt2http_objects PUBLIC -static)
    endif ()
endif ()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm")
    # The "parameter passing changed in GCC 7.1" notes only matter when mixing code built with older GCC
    target_compile_options(lt2http_objects PUBLIC -Wno-psabi)
endif ()

if (enable_sanitizers)
    target_compile_options(lt2http_objects PUBLIC -g -fsanitize=address,leak,undefined)
    target_link_options(lt2http_objects PUBLIC -fsanitize=address,leak,undefined)
endif ()

add_executable(lt2http src/app/main.cpp)
target_link_libraries(lt2http PRIVATE lt2http_objects)

add_library(lt2http_library SHARED src/app/main.cpp)
target_link_libraries(lt2http_library PRIVATE lt2http_objects)
target_compile_definitions(lt2http_library PRIVATE LT2HTTP_LIBRARY)
# lt2http.so / lt2http.dll on every platform, the add-on loads it by that name
set_target_properties(lt2http_library PROPERTIES OUTPUT_NAME lt2http PREFIX "")
# Export only startWithLogAndArgs: Kodi may also load Elementum, which carries its own libtorrent
if (APPLE)
    set_target_properties(lt2http_library PROPERTIES SUFFIX ".so")
    target_link_options(lt2http_library PRIVATE -Wl,-exported_symbol,_startWithLogAndArgs)
elseif (NOT WIN32)
    # The version script also hides "unique" template statics (like boost::asio's), which visibility does not.
    # --no-undefined reports missing symbols at build time instead of when Kodi loads the library.
    file(WRITE "${CMAKE_BINARY_DIR}/exports.map" "{ global: startWithLogAndArgs; local: *; };\n")
    target_link_options(lt2http_library PRIVATE -Wl,--no-undefined "-Wl,--version-script=${CMAKE_BINARY_DIR}/exports.map")
endif ()

if (build_tests)
    enable_testing()
    add_executable(lt2http_tests tests/unit_tests.cpp src/bittorrent/natpmp.cpp "${build_info_source}")
    target_include_directories(lt2http_tests PRIVATE src src/external/json_struct/include)
    target_compile_definitions(lt2http_tests PRIVATE LT2HTTP_CHANGELOG_FILE="${CMAKE_CURRENT_SOURCE_DIR}/changelog.md")
    target_link_libraries(lt2http_tests PRIVATE oatpp::oatpp LibtorrentRasterbar::torrent-rasterbar Boost::filesystem)
    add_test(NAME unit_tests COMMAND lt2http_tests)
endif ()

feature_summary(DEFAULT_DESCRIPTION WHAT ALL)
