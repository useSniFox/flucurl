# The Flutter tooling requires that developers have CMake 3.10 or later
# installed. You should not increase this version, as doing so will cause
# the plugin to fail to compile for some customers of the plugin.
cmake_minimum_required(VERSION 3.10)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(CMAKE_ENABLE_EXPORTS ON)
set(CMAKE_ENABLE_DEBUG_INFO ON)

set(CMAKE_CXX_STANDARD 20)
project(flucurl_library VERSION 0.0.1 LANGUAGES CXX)

if (ANDROID)
  message(INFO " NDK: ${ANDROID_NDK}")
  set(ENV{ANDROID_NDK_HOME} "${ANDROID_NDK}")
  set(VCPKG_CRT_LINKAGE dynamic)
  set(VCPKG_LIBRARY_LINKAGE static)
  set(VCPKG_CMAKE_SYSTEM_NAME Android)
  set(VCPKG_CMAKE_SYSTEM_VERSION 21)
  if (ANDROID_ABI STREQUAL "armeabi-v7a")
    set(VCPKG_TARGET_TRIPLET "arm-neon-android")
  elseif (ANDROID_ABI STREQUAL "arm64-v8a")
    set(VCPKG_TARGET_TRIPLET "arm64-android")
  elseif (ANDROID_ABI STREQUAL "x86_64")
    set(VCPKG_TARGET_TRIPLET "x64-android")
  else()
    message(FATAL_ERROR "Unsupported ANDROID_ABI: ${ANDROID_ABI}")
    # stop processing
    return()
  endif()
  include($ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake)
  set(ANDROID_CPP_FEATURES "rtti exceptions")
  set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
endif()

find_package(CURL REQUIRED)
add_library(flucurl SHARED
  "flucurl.cpp"
)

set_target_properties(flucurl PROPERTIES
  PUBLIC_HEADER flucurl.h
  OUTPUT_NAME "flucurl"
)

target_compile_definitions(flucurl PUBLIC DART_SHARED_LIB)
target_link_libraries(flucurl PRIVATE CURL::libcurl)


if (ANDROID)
  # Support Android 15 16k page size
  target_link_options(flucurl PRIVATE "-Wl,-z,max-page-size=16384")
endif()
