set(SOURCES main.c adc_esp32.c button.c ui_v3.c)

idf_component_register(
  SRCS ${SOURCES}
  INCLUDE_DIRS "."
  # REQUIRES button lvgl_esp32_drivers lvgl lv_examples lvgl_tft lvgl_touch adc
  )

target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_CONF_INCLUDE_SIMPLE=1)
