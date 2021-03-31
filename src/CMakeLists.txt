if (UNIX AND (NOT APPLE) AND (NOT CYGWIN))
    find_package(LibtorrentRasterbar ${minLibtorrentVersion} COMPONENTS torrent-rasterbar)
    if (NOT LibtorrentRasterbar_FOUND)
        include(FindPkgConfig)
        pkg_check_modules(LIBTORRENT_RASTERBAR IMPORTED_TARGET GLOBAL "libtorrent-rasterbar>=${minLibtorrentVersion}")
        if (NOT LIBTORRENT_RASTERBAR_FOUND)
            message(
                FATAL_ERROR
                "Package LibtorrentRasterbar >= ${minLibtorrentVersion} not found"
                " with CMake or pkg-config.\n- Set LibtorrentRasterbar_DIR to a directory containing"
                " a LibtorrentRasterbarConfig.cmake file or add the installation prefix of LibtorrentRasterbar"
                " to CMAKE_PREFIX_PATH.\n- Alternatively, make sure there is a valid libtorrent-rasterbar.pc"
                " file in your system's pkg-config search paths (use the system environment variable PKG_CONFIG_PATH"
                " to specify additional search paths if needed)."
            )
        endif()
        add_library(LibtorrentRasterbar::torrent-rasterbar ALIAS PkgConfig::LIBTORRENT_RASTERBAR)
        # force a fake package to show up in the feature summary
        set_property(GLOBAL APPEND PROPERTY
            PACKAGES_FOUND
            "LibtorrentRasterbar via pkg-config (version >= ${minLibtorrentVersion})"
        )
        set_package_properties("LibtorrentRasterbar via pkg-config (version >= ${minLibtorrentVersion})"
            PROPERTIES
            TYPE REQUIRED
        )
    else()
        if (LibtorrentRasterbar_VERSION)
            set(LIBTORRENT_RASTERBAR_VERSION ${LibtorrentRasterbar_VERSION})
        elseif(PC_LIBTORRENT_RASTERBAR_VERSION)
            set(LIBTORRENT_RASTERBAR_VERSION ${PC_LIBTORRENT_RASTERBAR_VERSION})
        endif()

        set_package_properties(LibtorrentRasterbar PROPERTIES TYPE REQUIRED)
    endif()
else()
    find_package(LibtorrentRasterbar ${minLibtorrentVersion} REQUIRED COMPONENTS torrent-rasterbar)
endif()

find_package(OpenSSL ${minOpenSSLVersion} REQUIRED)
find_package(Boost ${minBoostVersion} COMPONENTS system program_options REQUIRED)
find_package(oatpp ${minOatppVersion} REQUIRED)
find_package(oatpp-swagger ${minOatppSwaggerVersion} REQUIRED)

# List all available parameters
# get_cmake_property(_variableNames VARIABLES)
# list (SORT _variableNames)
# foreach (_variableName ${_variableNames})
#     message(STATUS "${_variableName}=${${_variableName}}")
# endforeach()

message(STATUS "Using Boost Version: ${boost_system_VERSION}")
message(STATUS "Using Libtorrent Version: ${LIBTORRENT_RASTERBAR_VERSION}")
message(STATUS "Using Oatpp Version: ${oatpp_VERSION}")
message(STATUS "Using Oatpp-Swagger Version: ${oatpp-swagger_VERSION}")

include_directories(
    ${Boost_INCLUDE_DIR} 
    ${oatpp_INCLUDE_DIRS} 
    ${oatpp-swagger_INCLUDE_DIRS}
)

add_definitions(-DOATPP_SWAGGER_RES_PATH="${oatpp-swagger_INCLUDE_DIRS}/../bin/oatpp-swagger/res" -DBOOST_STACKTRACE_USE_ADDR2LINE)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -rdynamic -export-dynamic -g -lprofiler")
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS} -rdynamic -export-dynamic -g -lprofiler")

set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS} -rdynamic -export-dynamic -fsanitize=address -fsanitize=leak -fsanitize=undefined -fsanitize=bounds -fsanitize=bounds-strict -g")
set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "${CMAKE_EXE_LINKER_FLAGS} -rdynamic -export-dynamic -fsanitize=address -fsanitize=leak -fsanitize=undefined -fsanitize=bounds -fsanitize=bounds-strict -g")

add_library(lt2http-base STATIC
    app/app_component.h
    app/application.h
    app/application.cpp
    app/config.h
    app/config.cpp
    app/swagger.h

    bittorrent/file.h
    bittorrent/file.cpp
    bittorrent/memory_storage.h
    bittorrent/memory_storage.cpp
    bittorrent/reader.h
    bittorrent/reader.cpp
    bittorrent/session.h
    bittorrent/session.cpp
    bittorrent/torrent.h
    bittorrent/torrent.cpp

    dto/error.h
    dto/file_info.h
    dto/file_operation.h
    dto/file_status.h
    dto/message.h
    dto/session_operation.h
    dto/session_status.h
    dto/status.h
    dto/torrent_add.h
    dto/torrent_info.h
    dto/torrent_operation.h
    dto/torrent_status.h

    external/cpp-httplib/httplib.h
    external/json_struct/include/json_struct.h

    utils/exceptions.h
    utils/http_client.h
    utils/http_url.h
    utils/logger.h
    utils/ltunderlyingtypes.h
    utils/numbers.h
    utils/path.h
    utils/pieces.h
    utils/requests.h
    utils/strings.h
    utils/system.h

    web/error_handler.h
    web/error_handler.cpp
    web/files.h
    web/misc.h
    web/request_logger.h
    web/session.h
    web/settings.h
    web/stream.h
    web/torrents.h
)

target_include_directories(lt2http-base PUBLIC .)

target_link_libraries(lt2http-base
    PUBLIC
        LibtorrentRasterbar::torrent-rasterbar
        OpenSSL::Crypto OpenSSL::SSL
        ${Boost_LIBRARIES}
        oatpp::oatpp
        oatpp::oatpp-swagger
        dl
)

add_executable(lt2http app/main.cpp)

target_link_libraries(lt2http
    PRIVATE
        lt2http-base
)

if(CMAKE_BUILD_TYPE STREQUAL "Release" AND ipo_supported)
    message(STATUS "Enabling LTO optimization")
    set_property(TARGET lt2http PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
    set_target_properties(lt2http PROPERTIES ENABLE_EXPORTS ON)
endif()
