cmake_minimum_required(VERSION 3.0.0)
project(allonet VERSION 0.1.0)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include_directories("${PROJECT_SOURCE_DIR}/include")
include_directories(SYSTEM
    "${PROJECT_SOURCE_DIR}/lib/enet/include"
    "${PROJECT_SOURCE_DIR}/lib"
)
add_subdirectory (lib/enet) 

# shared library

set(INCLUDE_FILES_PREFIX include/allonet)
set(INCLUDE_FILES
    ${INCLUDE_FILES_PREFIX}/allonet.h
    ${INCLUDE_FILES_PREFIX}/client.h
    ${INCLUDE_FILES_PREFIX}/net.h
    ${INCLUDE_FILES_PREFIX}/server.h
    ${INCLUDE_FILES_PREFIX}/state.h
)
set(SOURCE_FILES_PREFIX src)
set(SOURCE_FILES
    ${SOURCE_FILES_PREFIX}/client.c
    ${SOURCE_FILES_PREFIX}/server.c
    ${SOURCE_FILES_PREFIX}/state.c
    ${SOURCE_FILES_PREFIX}/util.c
    ${SOURCE_FILES_PREFIX}/util.h
)

source_group(include FILES ${INCLUDE_FILES})
source_group(source FILES ${SOURCE_FILES})

add_library(allonet SHARED 
    ${INCLUDE_FILES}
    ${SOURCE_FILES}
    ${SVC_FILES}
)
target_link_libraries(allonet enet pthread m)

# server example
add_executable (allodummyserv examples/dummyserv/dummyserv.c)
target_link_libraries (allodummyserv allonet)

# client example
add_executable (allodummyclient examples/dummyclient/dummyclient.c)
target_link_libraries (allodummyclient allonet)
