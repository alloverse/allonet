cmake_minimum_required(VERSION 3.10.0)
project(allonet VERSION 0.1.0)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
SET(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake) # for FindLibav
set(CMAKE_CXX_STANDARD 11)

option(ALLONET_BUILD_STATIC "Whether to build allonet as a static library instead of dynamic" OFF)
set(ALLONET_THREADING_STRATEGY "force_link_tinycthread" CACHE STRING "Whether to use native_if_available (native means threads.h from C11, with fallback to tinycthread), force_link_tinycthread or force_use_tinycthread (includes but does not link)")

IF(APPLE)
  SET(PLATSTR "mac-universal")
ELSEIF(ANDROID)
  SET(PLATSTR "android-arm64-v8a")
ELSEIF(UNIX)
  SET(PLATSTR "linux-x64")
ELSEIF(WIN32)
  SET(PLATSTR "windows-x64")
ELSE()
  FATAL_ERROR("Unsupported platform")
ENDIF()

# dependencies

include_directories(
  "${PROJECT_SOURCE_DIR}/include"
  "lib/mathc"
  "lib/cpp-httplib"
  "${CMAKE_CURRENT_BINARY_DIR}/include"
)
include_directories(SYSTEM
    "${PROJECT_SOURCE_DIR}/lib/enet/include"
    "${PROJECT_SOURCE_DIR}/lib/tinycthread/source"
    "${PROJECT_SOURCE_DIR}/lib/tinygltf"
    "${PROJECT_SOURCE_DIR}/lib"
)
add_subdirectory (lib/enet)

set(OPUS_DISABLE_INTRINSICS OFF) # fix m1 build
add_subdirectory(lib/opus)

SET(BUILD_SHARED_AND_STATIC_LIBS On CACHE BOOL "cjson static")
SET(ENABLE_CJSON_TEST Off CACHE BOOL "don't build cjson tests")
SET(ENABLE_CJSON_UTILS On CACHE BOOL "Use cJsonUtils")
add_subdirectory (lib/cJSON)

set(HTTPLIB_USE_BROTLI_IF_AVAILABLE OFF CACHE BOOL "disable brotli due to arm/x64 build issues on mac")
set(HTTPLIB_USE_OPENSSL_IF_AVAILABLE OFF CACHE BOOL "disable libssl to fix windows build maybe")
add_subdirectory(lib/cpp-httplib)

add_definitions(-DTINYGLTF_NO_STB_IMAGE -DTINYGLTF_NO_STB_IMAGE_WRITE)
set(TINYGLTF_HEADER_ONLY ON CACHE INTERNAL "" FORCE)
set(TINYGLTF_INSTALL OFF CACHE INTERNAL "" FORCE)
add_subdirectory(lib/tinygltf)

add_definitions(-DMATHC_USE_DOUBLE_FLOATING_POINT)


# shared library

set(INCLUDE_FILES_PREFIX include/allonet)
set(INCLUDE_FILES
    ${INCLUDE_FILES_PREFIX}/allonet.h
    ${INCLUDE_FILES_PREFIX}/arr.h
    ${INCLUDE_FILES_PREFIX}/assetstore.h
    ${INCLUDE_FILES_PREFIX}/client.h
    ${INCLUDE_FILES_PREFIX}/jobs.h
    ${INCLUDE_FILES_PREFIX}/math.h
    ${INCLUDE_FILES_PREFIX}/net.h
    ${INCLUDE_FILES_PREFIX}/server.h
    ${INCLUDE_FILES_PREFIX}/state.h
)
set(SOURCE_FILES_PREFIX src)
set(SOURCE_FILES
    ${SOURCE_FILES_PREFIX}/client/_client.h
    ${SOURCE_FILES_PREFIX}/client/client.c
    ${SOURCE_FILES_PREFIX}/simulation/animation.c
    ${SOURCE_FILES_PREFIX}/simulation/animation_prop.c
    ${SOURCE_FILES_PREFIX}/simulation/grabbing.c
    ${SOURCE_FILES_PREFIX}/simulation/pose_movement.c
    ${SOURCE_FILES_PREFIX}/simulation/simulation.c
    ${SOURCE_FILES_PREFIX}/simulation/simulation.h
    ${SOURCE_FILES_PREFIX}/simulation/stick_movement.c
    ${SOURCE_FILES_PREFIX}/media/media.h
    ${SOURCE_FILES_PREFIX}/media/media.c
    ${SOURCE_FILES_PREFIX}/media/audio/audio.c
    ${SOURCE_FILES_PREFIX}/media/video/video.c
    ${SOURCE_FILES_PREFIX}/media/video/mjpeg.cpp
    ${SOURCE_FILES_PREFIX}/_asset.h
    ${SOURCE_FILES_PREFIX}/allo_gltf.cpp
    ${SOURCE_FILES_PREFIX}/arr.c
    ${SOURCE_FILES_PREFIX}/asset.c
    ${SOURCE_FILES_PREFIX}/asset.h
    ${SOURCE_FILES_PREFIX}/assetstore.c
    ${SOURCE_FILES_PREFIX}/clientproxy.c
    ${SOURCE_FILES_PREFIX}/clientproxy.h
    ${SOURCE_FILES_PREFIX}/delta.c
    ${SOURCE_FILES_PREFIX}/delta.h
    ${SOURCE_FILES_PREFIX}/get_version.c
    ${SOURCE_FILES_PREFIX}/jobs.c
    ${SOURCE_FILES_PREFIX}/math.c
    ${SOURCE_FILES_PREFIX}/os.c
    ${SOURCE_FILES_PREFIX}/os.h
    ${SOURCE_FILES_PREFIX}/server.c
    ${SOURCE_FILES_PREFIX}/sha1.c
    ${SOURCE_FILES_PREFIX}/sha1.h
    ${SOURCE_FILES_PREFIX}/sha256.c
    ${SOURCE_FILES_PREFIX}/sha256.h
    ${SOURCE_FILES_PREFIX}/standalone_server.cpp
    ${SOURCE_FILES_PREFIX}/state.c
    ${SOURCE_FILES_PREFIX}/util.cpp
    ${SOURCE_FILES_PREFIX}/util.h
    lib/mathc/mathc.c
    lib/richgel9999-jpegcompressor/jpgd.cpp
    lib/richgel9999-jpegcompressor/jpge.cpp
    lib/tinyjpeg/tiny_jpeg.c
)

set(AV_SOURCE_FILES
  ${SOURCE_FILES_PREFIX}/media/video/libav.c
)

source_group(include FILES ${INCLUDE_FILES})
source_group(source FILES ${SOURCE_FILES})

if(ALLONET_BUILD_STATIC)
  SET(ALLONET_BUILDTYPE STATIC)
else()
  SET(ALLONET_BUILDTYPE SHARED)
endif()

add_library(allonet ${ALLONET_BUILDTYPE})

add_library(allonet_av ${ALLONET_BUILDTYPE})

# Versioning
# crazy relative-path hack courtesy of running bash on windows and having mixed path types...
file(RELATIVE_PATH GENERATE_VERSION_SH ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/generate-version.sh)
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/include/allonet_version.h
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMAND bash ${GENERATE_VERSION_SH} include
)
add_custom_target(
    version_header ALL
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/include/allonet_version.h
)
add_dependencies(allonet version_header)

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

target_sources(allonet_av
  PRIVATE
    ${AV_SOURCE_FILES}
)

# Threading libs and various system libs
add_subdirectory (lib/tinycthread)

IF(WIN32)
  SET(PLATFORM_LIBS ws2_32 winmm)
ELSEIF(ANDROID)
  SET(PLATFORM_LIBS "")
ELSE()
  SET(PLATFORM_LIBS m pthread)
ENDIF(WIN32)

find_package(Threads REQUIRED)

# use threads.h only if it's available AND we're allowed to by our parent project.
# else, fallback to tinycthread.
# if threads.h compatible API is not available on this system, use tinycthread.
# even if it's available, use tinycthread if parent project forces us to.
if(CMAKE_USE_PTHREADS_INIT AND ${ALLONET_THREADING_STRATEGY} MATCHES "native_if_available" AND NOT APPLE)
  SET(PLATFORM_LIBS ${PLATFORM_LIBS} ${CMAKE_THREAD_LIBS_INIT})
else()
  add_definitions(-DUSE_TINYCTHREAD)
  if(NOT ${ALLONET_THREADING_STRATEGY} MATCHES "force_use_tinycthread")
    SET(PLATFORM_LIBS ${PLATFORM_LIBS} tinycthread)
  endif()
endif()
message("With threading strategy ${ALLONET_THREADING_STRATEGY}, using platform libs ${PLATFORM_LIBS}")


# Media libs
set(LIBAV_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bindeps/ffmpeg/include)
file(GLOB LIBAV_LIBRARIES ${CMAKE_CURRENT_SOURCE_DIR}/bindeps/ffmpeg/${PLATSTR}/lib/*)
file(GLOB LIBAV_DLLS ${CMAKE_CURRENT_SOURCE_DIR}/bindeps/ffmpeg/${PLATSTR}/bin/*)
get_directory_property(hasParent PARENT_DIRECTORY)
if(hasParent)
  set(LIBAV_DLLS ${LIBAV_DLLS} PARENT_SCOPE)
endif()
IF(APPLE)
  find_library(VIDEO_DECODE_ACCELERATION NAMES VideoDecodeAcceleration)
  set(LIBAV_LIBRARIES ${LIBAV_LIBRARIES} ${VIDEO_DECODE_ACCELERATION})
ENDIF()
message("allonet/libav: linking with ${LIBAV_LIBRARIES} and bundling ${LIBAV_DLLS}")

include_directories(SYSTEM
  ${LIBAV_INCLUDE_DIR}
)

target_link_libraries(allonet 
  enet cjson-static cjson_utils-static opus httplib::httplib
  ${PLATFORM_LIBS}
)

target_link_libraries(allonet_av
  allonet ${PLATFORM_LIBS} ${LIBAV_LIBRARIES}
)

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
