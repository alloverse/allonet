cmake_minimum_required(VERSION 3.0.0)
project(allonet VERSION 0.1.0)

include_directories("${PROJECT_SOURCE_DIR}/include")
include_directories(SYSTEM
    "${PROJECT_SOURCE_DIR}/lib/enet/include"
    "${PROJECT_SOURCE_DIR}/lib" # for svc
)
add_subdirectory (lib/enet) 

# shared library

add_library(allonet SHARED 
    src/server.c src/state.c 
    lib/svc/ntv.c lib/svc/ntv_json.c lib/svc/mbuf.c lib/svc/trace.c lib/svc/json.c lib/svc/misc.c lib/svc/utf8.c lib/svc/talloc.c lib/svc/dbl.c)
target_link_libraries (allonet enet)

# server example
add_executable (allodummyserv examples/dummyserv/dummyserv.c)
target_link_libraries (allodummyserv allonet)
