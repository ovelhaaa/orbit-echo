cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED ENV{IDF_PATH})
  message(STATUS "ESP-IDF não detectado (IDF_PATH ausente). Pulando target embedded_esp32s3.")
  return()
endif()

idf_component_register(
  SRCS
    main/app_main.cpp
    ../../core/src/orbit_delay_core.cpp
  INCLUDE_DIRS
    ../../
)
