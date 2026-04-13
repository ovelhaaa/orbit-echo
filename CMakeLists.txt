cmake_minimum_required(VERSION 3.16)
project(orbit_echo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)

add_subdirectory(core)
add_subdirectory(targets/wasm)

if(DEFINED ENV{IDF_PATH})
  add_subdirectory(targets/embedded_esp32s3)
endif()

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
