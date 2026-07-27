set(LV_DEMO_DIR ../managed_components/lvgl__lvgl/demos)
file(GLOB_RECURSE LV_DEMOS_SOURCES ${LV_DEMO_DIR}/*.c)

set(SOURCES
    "main.cpp" 
    ${LV_DEMOS_SOURCES}
    "bsp/board_init.c"
)

idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS "." "./bsp" ${LV_DEMO_DIR}
                       REQUIRES arduino-esp32)

idf_component_get_property(LVGL_LIB lvgl__lvgl COMPONENT_LIB)
target_compile_options(
    ${LVGL_LIB} 
    PRIVATE
        -DLV_LVGL_H_INCLUDE_SIMPLE
)

idf_component_get_property(LVGL_PORT_LIB espressif__esp_lvgl_port COMPONENT_LIB)
target_compile_options(${LVGL_PORT_LIB} INTERFACE -DCONFIG_LV_DRAW_SW_SUPPORT_RGB565_SWAPPED=1)

idf_build_set_property(COMPILE_OPTIONS "-DCONFIG_LV_DRAW_SW_SUPPORT_RGB565_SWAPPED=0" APPEND)
