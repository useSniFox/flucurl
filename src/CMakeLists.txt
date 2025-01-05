# The Flutter tooling requires that developers have CMake 3.10 or later
# installed. You should not increase this version, as doing so will cause
# the plugin to fail to compile for some customers of the plugin.
cmake_minimum_required(VERSION 3.10)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(CMAKE_ENABLE_EXPORTS ON)
set(CMAKE_ENABLE_DEBUG_INFO ON)

set(ANDROID CMAKE_SYSTEM_NAME STREQUAL "Android")

if (ANDROID)
  if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    set(DART_NAME "dart.bat")
  else()
    set(DART_NAME "dart")
  endif()

  find_program(DART_EXECUTABLE ${DART_NAME})

  execute_process(
    COMMAND "${DART_EXECUTABLE}" pub get
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
    RESULT_VARIABLE result
  )
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "Failed to run pub get. Exit code: ${result}")
  endif()

  execute_process(
    COMMAND "${DART_EXECUTABLE}" run src/build.dart android ${CMAKE_CXX_COMPILER} ${CMAKE_GENERATOR} ${CMAKE_COMMAND}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
    RESULT_VARIABLE result
  )
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "Failed to run build.dart. Status code: ${result}")
  endif()
endif()

set(CMAKE_CXX_STANDARD 20 REQUIRED)
project(flucurl_library VERSION 0.0.1 LANGUAGES CXX)

if (LINUX) 
  set(CURL_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/curl-x86_64/lib")
  set(CURL_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/curl-x86_64/include")
  include_directories(${CURL_INCLUDE_DIR})
elseif (ANDROID)
  set(CURL_LIB_ARCH "${ANDROID_ABI}")
  if (NOT "${CURL_LIB_ARCH}" MATCHES "armeabi-v7a|arm64-v8a|x86_64")
    message(FATAL_ERROR "Invalid ANDROID_ABI value: ${CURL_LIB_ARCH}")
  endif()
  set(CURL_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/curl/lib/android.${CURL_LIB_ARCH}")
  set(CURL_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/curl/include")
  include_directories(${CURL_INCLUDE_DIR})
endif()

if (WIN32)
  find_package(CURL REQUIRED)
endif()
add_library(flucurl SHARED
  "flucurl.cpp"
)

set_target_properties(flucurl PROPERTIES
  PUBLIC_HEADER flucurl.h
  OUTPUT_NAME "flucurl"
)

target_compile_definitions(flucurl PUBLIC DART_SHARED_LIB)

if (WIN32)
  target_link_libraries(flucurl PRIVATE CURL::libcurl)
elseif (LINUX OR ANDROID)
  file(GLOB CURL_LIBS "${CURL_LIB_DIR}/*.a")
  target_link_libraries(flucurl PRIVATE ${CURL_LIBS})
endif()

if (ANDROID)
  # Support Android 15 16k page size
  target_link_options(flucurl PRIVATE "-Wl,-z,max-page-size=16384")
endif()
