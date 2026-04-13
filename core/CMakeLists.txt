add_library(orbit_delay_core STATIC
  src/orbit_delay_core.cpp
  src/orbit_delay_c_api.cpp
)

target_include_directories(orbit_delay_core
  PUBLIC
    ${CMAKE_SOURCE_DIR}
)
