if(NOT EMSCRIPTEN)
  message(STATUS "Skipping targets/wasm: configure with Emscripten to build WASM target.")
  return()
endif()

add_executable(orbit_delay_wasm
  orbit_delay_wasm.cpp
)

target_link_libraries(orbit_delay_wasm PRIVATE orbit_delay_core)

target_include_directories(orbit_delay_wasm PRIVATE
  ${CMAKE_SOURCE_DIR}
)

set_target_properties(orbit_delay_wasm PROPERTIES
  SUFFIX ".js"
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/js
)

target_link_options(orbit_delay_wasm PRIVATE
  "--no-entry"
  "-sMODULARIZE=1"
  "-sEXPORT_ES6=1"
  "-sENVIRONMENT=web,worker,node"
  "-sALLOW_MEMORY_GROWTH=1"
  "-sEXPORTED_FUNCTIONS=['_malloc','_free','_orbit_wasm_init','_orbit_wasm_free','_orbit_wasm_reset','_orbit_wasm_process_stereo','_orbit_wasm_set_orbit','_orbit_wasm_set_offset_samples','_orbit_wasm_set_stereo_spread','_orbit_wasm_set_feedback','_orbit_wasm_set_mix','_orbit_wasm_set_input_gain','_orbit_wasm_set_output_gain','_orbit_wasm_set_tone_hz','_orbit_wasm_set_smear_amount','_orbit_wasm_set_diffuser_stages','_orbit_wasm_set_dc_block_enabled','_orbit_wasm_set_read_mode']"
  "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']"
)
