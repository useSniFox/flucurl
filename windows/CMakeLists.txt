# The Flutter tooling requires that developers have a version of Visual Studio
# installed that includes CMake 3.14 or later. You should not increase this
# version, as doing so will cause the plugin to fail to compile for some
# customers of the plugin.
cmake_minimum_required(VERSION 3.14)
message(INFO " ${CMAKE_CXX_COMPILER}")
execute_process(
  COMMAND dart.bat run src/build.dart windows ${CMAKE_CXX_COMPILER} ${CMAKE_GENERATOR} ${CMAKE_COMMAND}
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
  OUTPUT_VARIABLE output
  RESULT_VARIABLE result
)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Failed to run build.dart. Status code: ${result} Output: ${output}")
endif()

# Project-level configuration.
set(PROJECT_NAME "flucurl")
project(${PROJECT_NAME} LANGUAGES CXX)

set(BUILD_TYPE "Release")

# List of absolute paths to libraries that should be bundled with the plugin.
# This list could contain prebuilt libraries, or libraries created by an
# external build triggered from this build file.
set(flucurl_bundled_libraries
  # Defined in ../src/CMakeLists.txt.
  # This can be changed to accommodate different builds.
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/build/${BUILD_TYPE}/flucurl.dll"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/build/${BUILD_TYPE}/libcurl.dll"
  "${CMAKE_CURRENT_SOURCE_DIR}/../src/build/${BUILD_TYPE}/zlib1.dll"
  PARENT_SCOPE
)
