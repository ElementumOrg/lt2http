# Cross compilation toolchain driven by the environment of docker/<platform>.Dockerfile images:
#   TARGET_SYSTEM     CMake system name: Linux (also used for Android), Windows or Darwin
#   TARGET_PROCESSOR  target processor, e.g. x86_64, aarch64
#   TARGET_CC/CXX     target compilers
#   CROSS_TRIPLE      tool prefix for ar, ranlib, windres, install_name_tool
#   CROSS_ROOT        sysroot where dependencies are installed
#   TARGET_FLAGS      optional compiler flags for every C/C++ target
set(CMAKE_SYSTEM_NAME "$ENV{TARGET_SYSTEM}")
set(CMAKE_SYSTEM_PROCESSOR "$ENV{TARGET_PROCESSOR}")

set(CMAKE_C_COMPILER "$ENV{TARGET_CC}")
set(CMAKE_CXX_COMPILER "$ENV{TARGET_CXX}")
find_program(CMAKE_AR NAMES "$ENV{CROSS_TRIPLE}-ar" REQUIRED)
find_program(CMAKE_RANLIB NAMES "$ENV{CROSS_TRIPLE}-ranlib" REQUIRED)
set(CMAKE_C_FLAGS_INIT "$ENV{TARGET_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "$ENV{TARGET_FLAGS}")

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    find_program(CMAKE_RC_COMPILER NAMES "$ENV{CROSS_TRIPLE}-windres" REQUIRED)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    find_program(CMAKE_INSTALL_NAME_TOOL NAMES "$ENV{CROSS_TRIPLE}-install_name_tool" REQUIRED)
endif ()

set(CMAKE_FIND_ROOT_PATH "$ENV{CROSS_ROOT}")
set(CMAKE_PREFIX_PATH "$ENV{CROSS_ROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
