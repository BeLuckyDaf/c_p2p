cmake_minimum_required(VERSION 3.1)
project(p2p C)

set(CMAKE_C_STANDARD 11)

find_package(Threads)

include_directories(.)

add_executable(p2p
        third_party/alist.c
        node.c
        third_party/sockutil.c
        third_party/rxi/map.c
        server.c
        main.c)

add_executable(ddos
        third_party/sockutil.c
        ddos/ddos.c)

target_link_libraries(p2p ${CMAKE_THREAD_LIBS_INIT})


