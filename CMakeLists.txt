cmake_minimum_required(VERSION 3.1)
project(p2p C)

set(CMAKE_C_STANDARD 11)

find_package(Threads)

include_directories(.)

add_executable(p2p
        utils/alist.c
        node.c
        utils/sockutil.c
        server.c
        main.c)

target_link_libraries(p2p ${CMAKE_THREAD_LIBS_INIT})


