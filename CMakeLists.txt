cmake_minimum_required(VERSION 3.10.0)
project(allonet VERSION 0.1.0)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

option(ALLONET_BUILD_STATIC "Whether to build allonet as a static library instead of dynamic" OFF)
option(ALLONET_USE_LUA "Whether to include the Lua bridge." ON)
option(ALLONET_LINK_TINYCTHREAD "Whether to link with tinycthread. say off if you already have it in the process." ON)

include_directories("${PROJECT_SOURCE_DIR}/include", "lib/mathc")
include_directories(SYSTEM
    "${PROJECT_SOURCE_DIR}/lib/enet/include"
    "${PROJECT_SOURCE_DIR}/lib/tinycthread/source"
    "${PROJECT_SOURCE_DIR}/lib"
)
add_subdirectory (lib/enet)

add_subdirectory(lib/opus)

# Don't want to require lua installed on dev machine, so include sources instead.
#find_package(Lua REQUIRED)
add_subdirectory (lib/lua)
set(LUA_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/lib/lua/src)
IF(ALLONET_USE_LUA)
	include_directories("${LUA_INCLUDE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/lib/lua")
	IF(APPLE)
	  SET(LUA_LINKER_FLAGS "-Wl,-flat_namespace -Wl,-undefined,dynamic_lookup")
	ELSEIF(UNIX)
	  SET(LUA_LINKER_FLAGS "-Wl,-undefined,dynamic_lookup")
	ELSEIF(WIN32)
	  SET(LUA_LINKER_FLAGS )
	ENDIF()
ELSE()
	SET(LUA_LINKER_FLAGS "")
ENDIF()

SET(BUILD_SHARED_AND_STATIC_LIBS On CACHE BOOL "cjson static")
SET(ENABLE_CJSON_TEST Off CACHE BOOL "don't build cjson tests")
SET(ENABLE_CJSON_UTILS On CACHE BOOL "Use cJsonUtils")
add_subdirectory (lib/cJSON)

add_definitions(-DMATHC_USE_DOUBLE_FLOATING_POINT)


# shared library

set(INCLUDE_FILES_PREFIX include/allonet)
set(INCLUDE_FILES
    ${INCLUDE_FILES_PREFIX}/allonet.h
    ${INCLUDE_FILES_PREFIX}/client.h
    ${INCLUDE_FILES_PREFIX}/net.h
    ${INCLUDE_FILES_PREFIX}/server.h
    ${INCLUDE_FILES_PREFIX}/state.h
    ${INCLUDE_FILES_PREFIX}/jobs.h
    ${INCLUDE_FILES_PREFIX}/assetstore.h
)
set(SOURCE_FILES_PREFIX src)
set(SOURCE_FILES
    ${SOURCE_FILES_PREFIX}/client/client.c
    ${SOURCE_FILES_PREFIX}/client/audio.c
    ${SOURCE_FILES_PREFIX}/server.c
    ${SOURCE_FILES_PREFIX}/standalone_server.c
    ${SOURCE_FILES_PREFIX}/state.c
    ${SOURCE_FILES_PREFIX}/util.c
    ${SOURCE_FILES_PREFIX}/util.h
    ${SOURCE_FILES_PREFIX}/simulation/simulation.c
    ${SOURCE_FILES_PREFIX}/simulation/grabbing.c
    ${SOURCE_FILES_PREFIX}/simulation/pose_movement.c
    ${SOURCE_FILES_PREFIX}/simulation/stick_movement.c
    ${SOURCE_FILES_PREFIX}/arr.c
    ${SOURCE_FILES_PREFIX}/math.c
    ${SOURCE_FILES_PREFIX}/jobs.c
    ${SOURCE_FILES_PREFIX}/asset.h
    ${SOURCE_FILES_PREFIX}/asset.c
    ${SOURCE_FILES_PREFIX}/assetstore.c
    ${SOURCE_FILES_PREFIX}/delta.h
    ${SOURCE_FILES_PREFIX}/delta.c
    ${SOURCE_FILES_PREFIX}/clientproxy.h
    ${SOURCE_FILES_PREFIX}/clientproxy.c
    ${SOURCE_FILES_PREFIX}/sha1.h
    ${SOURCE_FILES_PREFIX}/sha1.c
    ${SOURCE_FILES_PREFIX}/sha256.h
    ${SOURCE_FILES_PREFIX}/sha256.c
    ${SOURCE_FILES_PREFIX}/os.c
    lib/mathc/mathc.c
)

if(ALLONET_USE_LUA)
	LIST(APPEND SOURCE_FILES
		lang/lua/allonet-lua.c
		lang/lua/lua-utils.c
		lang/lua/lua-weak.c
	)
endif()

source_group(include FILES ${INCLUDE_FILES})
source_group(source FILES ${SOURCE_FILES})

if(ALLONET_BUILD_STATIC)
  SET(ALLONET_BUILDTYPE STATIC)
else()
  SET(ALLONET_BUILDTYPE SHARED)
endif()

add_library(allonet ${ALLONET_BUILDTYPE})

if(MSVC)
  target_compile_options(allonet PRIVATE /W4)
else()
  target_compile_options(allonet PRIVATE -Wall -Wextra -pedantic)
endif()


target_sources(allonet
  PRIVATE
    ${INCLUDE_FILES}
    ${SOURCE_FILES}
)
add_subdirectory (lib/tinycthread)

IF(WIN32)
  SET(PLATFORM_LIBS ws2_32 winmm)
ELSEIF(ANDROID)
  SET(PLATFORM_LIBS "")
ELSE()
  SET(PLATFORM_LIBS m pthread)
ENDIF(WIN32)

if(ALLONET_LINK_TINYCTHREAD)
  SET(TINYCTHREAD tinycthread)
else()
  SET(TINYCTHREAD )
endif()

target_link_libraries(allonet enet cjson-static cjson_utils-static opus ${TINYCTHREAD} ${PLATFORM_LIBS} ${LUA_LINKER_FLAGS})

# client example
add_executable (allodummyclient examples/dummyclient/dummyclient.c)
target_link_libraries (allodummyclient allonet)

# cubeappliance
add_executable (allocubeappliance examples/cubeappliance/cubeappliance.c)
target_link_libraries (allocubeappliance allonet)

# standalone_server
add_executable (standalone_server examples/standalone_server/standalone.c)
target_link_libraries (standalone_server allonet)

install(TARGETS allonet LIBRARY DESTINATION lib)

# tests
add_definitions(-DUNITY_INCLUDE_DOUBLE)
add_subdirectory(lib/unity EXCLUDE_FROM_ALL)

add_executable(allonet_state_test test/state_test.c)
target_link_libraries(allonet_state_test allonet unity)
add_test(NAME allonet_state_test COMMAND allonet_state_test)

add_executable(allonet_math_test test/math_test.c)
target_link_libraries(allonet_math_test allonet unity)
add_test(NAME allonet_math_test COMMAND allonet_math_test)

add_executable(allonet_jobs_test test/jobs_test.c)
target_link_libraries(allonet_jobs_test allonet unity)
add_test(NAME allonet_jobs_test COMMAND allonet_jobs_test)

#add_executable(allonet_assetstore_test test/assetstore_test.c)
#target_link_libraries(allonet_assetstore_test allonet unity tinycthread)
#add_test(NAME allonet_assetstore_test COMMAND allonet_assetstore_test)

add_executable(allonet_delta_test test/delta_test.c)
target_link_libraries(allonet_delta_test allonet unity cjson)
add_test(NAME allonet_delta_test COMMAND allonet_delta_test)
