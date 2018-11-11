cmake_minimum_required(VERSION 3.0.0)
project(allonet VERSION 0.1.0)

include_directories("${PROJECT_SOURCE_DIR}/include")
add_subdirectory (lib/enet) 

# shared library

add_library(allonet SHARED src/server.c)

# server example
add_executable (allodummyserv examples/dummyserv/dummyserv.c)
target_link_libraries (allodummyserv allonet)
