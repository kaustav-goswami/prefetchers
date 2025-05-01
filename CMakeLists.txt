cmake_minimum_required(VERSION 3.9.1)
project(SimplePrefetchers)


set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/)

add_executable(main src/prefetch.c)
