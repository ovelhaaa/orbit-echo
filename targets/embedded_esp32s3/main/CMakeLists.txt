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

if(IDF_VERSION_MAJOR GREATER_EQUAL 5)
  idf_component_get_property(component_requires ${COMPONENT_LIB} PRIV_REQUIRES)
  list(APPEND component_requires
    esp_driver_gpio
    esp_driver_i2s
    esp_driver_pcnt
    esp_driver_spi
  )
  set_property(TARGET ${COMPONENT_LIB} PROPERTY PRIV_REQUIRES "${component_requires}")
endif()

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
