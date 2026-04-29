idf_component_register(
  SRCS
    app_main.cpp
    audio_engine_esp32.cpp
    audio_source.cpp
    parameter_bridge.cpp
    ui_tft.cpp
    ../../../core/src/orbit_delay_core.cpp
    ../../../core/src/orbit_delay_c_api.cpp
  INCLUDE_DIRS
    .
    ../../..
    ../../../core/include
  PRIV_REQUIRES
    driver
    esp_lcd
    freertos
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
