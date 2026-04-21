cmake_minimum_required(VERSION 3.16)

# This directory is an ESP-IDF application root when configured directly via:
#   cd targets/embedded_esp32s3 && idf.py build
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  if(NOT DEFINED ENV{IDF_PATH})
    message(FATAL_ERROR "ESP-IDF não detectado. Defina IDF_PATH antes de executar idf.py build.")
  endif()

  include($ENV{IDF_PATH}/tools/cmake/project.cmake)
  project(orbit_echo_esp32s3)
else()
  message(STATUS "target embedded_esp32s3 deve ser compilado via ESP-IDF (idf.py build).")
endif()
