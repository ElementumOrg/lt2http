set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DIFADDRS")

add_library(ifaddrs STATIC
    ifaddrs.h
    ifaddrs.c
)

target_include_directories(ifaddrs PUBLIC .)
