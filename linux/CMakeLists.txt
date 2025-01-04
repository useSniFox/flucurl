# The Flutter tooling requires that developers have CMake 3.10 or later
# installed. You should not increase this version, as doing so will cause
# the plugin to fail to compile for some customers of the plugin.
cmake_minimum_required(VERSION 3.10)

execute_process(
  COMMAND dart pub get
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
  RESULT_VARIABLE result
)
if(NOT result EQUAL 0)
	message(FATAL_ERROR "Failed to run pub get. Exit code: ${result}")
endif()

execute_process(
  COMMAND dart run src/build.dart linux ${CMAKE_CXX_COMPILER} ${CMAKE_GENERATOR} ${CMAKE_COMMAND}
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
  RESULT_VARIABLE result
)
if(NOT result EQUAL 0)
	message(FATAL_ERROR "Failed to run build.dart. Exit code: ${result}")
endif()

# Project-level configuration.
set(PROJECT_NAME "flucurl")
project(${PROJECT_NAME} LANGUAGES CXX)

file(GLOB FLUCURL_DLL "${CMAKE_CURRENT_SOURCE_DIR}/../src/build/*.so")

message(INFO "FLUCURL_DLL: ${FLUCURL_DLL}")

set(flucurl_bundled_libraries
  ${FLUCURL_DLL}
  PARENT_SCOPE
)
