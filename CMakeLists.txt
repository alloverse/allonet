cmake_minimum_required(VERSION 3.10.0)
project(allonet VERSION 0.1.0)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

option(ALLONET_BUILD_STATIC "Whether to build allonet as a static library instead of dynamic" OFF)
option(ALLONET_USE_LUA "Whether to include the Lua bridge." ON)

include_directories("${PROJECT_SOURCE_DIR}/include")
include_directories(SYSTEM
    "${PROJECT_SOURCE_DIR}/lib/enet/include"
    "${PROJECT_SOURCE_DIR}/lib/enet/include"
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
add_subdirectory (lib/cJSON)

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
    ${SOURCE_FILES_PREFIX}/state_simulation.c
    ${SOURCE_FILES_PREFIX}/arr.c
    ${SOURCE_FILES_PREFIX}/math.c
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

add_library(allonet ${ALLONET_BUILDTYPE} 
    ${INCLUDE_FILES}
    ${SOURCE_FILES}
    ${SVC_FILES}
)

IF(WIN32)
  SET(PLATFORM_LIBS ws2_32 winmm)
ELSEIF(ANDROID)
  SET(PLATFORM_LIBS "")
ELSE()
  SET(PLATFORM_LIBS m pthread)
ENDIF(WIN32)

target_link_libraries(allonet enet cjson-static opus ${PLATFORM_LIBS} ${LUA_LINKER_FLAGS})

# client example
add_executable (allodummyclient examples/dummyclient/dummyclient.c)
target_link_libraries (allodummyclient allonet)

# cubeappliance
add_executable (allocubeappliance examples/cubeappliance/cubeappliance.c)
target_link_libraries (allocubeappliance allonet)

install(TARGETS allonet LIBRARY)
