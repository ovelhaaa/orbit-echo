add_executable(orbit_delay_wasm
  js/wasm_main.cpp
)

target_link_libraries(orbit_delay_wasm PRIVATE orbit_delay_core)

target_include_directories(orbit_delay_wasm PRIVATE
  ${CMAKE_SOURCE_DIR}
)

if(EMSCRIPTEN)
  set_target_properties(orbit_delay_wasm PROPERTIES SUFFIX ".js")
  target_link_options(orbit_delay_wasm PRIVATE
    "-sEXPORTED_FUNCTIONS=['_main']"
    "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']"
    "-sALLOW_MEMORY_GROWTH=1"
  )
endif()
