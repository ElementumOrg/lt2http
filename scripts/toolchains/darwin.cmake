SET(CMAKE_SYSTEM_NAME Darwin)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden -fvisibility-inlines-hidden")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden -fvisibility-inlines-hidden")

# which compilers to use for C and C++
find_program(CMAKE_AR NAMES ${CROSS_TRIPLE}-gcc-ar)
find_program(CMAKE_RANLIB NAMES ${CROSS_TRIPLE}-gcc-ranlib)
find_program(CMAKE_NM NAMES ${CROSS_TRIPLE}-gcc-nm)
find_program(CMAKE_INSTALL_NAME_TOOL NAMES ${CROSS_TRIPLE}-install_name_tool)
find_program(CMAKE_C_COMPILER NAMES ${CROSS_TRIPLE}-gcc)
find_program(CMAKE_CXX_COMPILER NAMES ${CROSS_TRIPLE}-g++)

# SET(CMAKE_FIND_ROOT_PATH  /usr/${CROSS_TRIPLE})

SET(CMAKE_FIND_LIBRARY_SUFFIXES ".so;.a")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)