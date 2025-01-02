# The Flutter tooling requires that developers have CMake 3.10 or later
# installed. You should not increase this version, as doing so will cause
# the plugin to fail to compile for some customers of the plugin.
cmake_minimum_required(VERSION 3.10)

project(flucurl_library VERSION 0.0.1 LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(
        curl
        URL https://github.com/curl/curl/releases/download/curl-8_6_0/curl-8.6.0.tar.gz
)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)  # 静态库
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(HTTP_ONLY OFF CACHE BOOL "" FORCE)
set(ENABLE_MANUAL OFF CACHE BOOL "" FORCE)
set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(curl)

add_library(flucurl SHARED
  "flucurl.cpp"
)

set_target_properties(flucurl PROPERTIES
  PUBLIC_HEADER flucurl.h
  OUTPUT_NAME "flucurl"
)

target_compile_definitions(flucurl PUBLIC DART_SHARED_LIB)
target_link_libraries(flucurl PRIVATE libcurl)

if (ANDROID)
  # Support Android 15 16k page size
  target_link_options(flucurl PRIVATE "-Wl,-z,max-page-size=16384")
endif()
