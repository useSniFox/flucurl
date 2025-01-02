# The Flutter tooling requires that developers have CMake 3.10 or later
# installed. You should not increase this version, as doing so will cause
# the plugin to fail to compile for some customers of the plugin.
cmake_minimum_required(VERSION 3.10)

project(flucurl_library VERSION 0.0.1 LANGUAGES CXX)

add_library(flucurl SHARED
  "flucurl.cpp"
)

set_target_properties(flucurl PROPERTIES
  PUBLIC_HEADER flucurl.h
  OUTPUT_NAME "flucurl"
)

target_compile_definitions(flucurl PUBLIC DART_SHARED_LIB)

if (ANDROID)
  # Support Android 15 16k page size
  target_link_options(flucurl PRIVATE "-Wl,-z,max-page-size=16384")
endif()
