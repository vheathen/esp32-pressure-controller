set(SOURCES main.c pressure_sensors.c button.c ui.c)

idf_component_register(
  SRCS ${SOURCES}
  INCLUDE_DIRS "."
  # REQUIRES button lvgl_esp32_drivers lvgl lv_examples lvgl_tft lvgl_touch adc
  )

target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_CONF_INCLUDE_SIMPLE=1)
