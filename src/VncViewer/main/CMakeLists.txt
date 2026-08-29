#
#

set(
    SOURCES
        "main.c" 
        "app_main.c"
        "bsp_display.c"
        "vnc_display.c"
        "vnc_screen.c"
        "wifi_setting_popup.c"
        "vnc_connect_popup.c"
        "vnc_client.c"
        "tight_decoder.c"
        "zrle_decode.c"
        "d3des.c"
        "miniz/miniz.c"
)

idf_component_register(
    SRCS 
        ${SOURCES}
    INCLUDE_DIRS 
        "."
    REQUIRES
        nvs_flash
        esp_timer
)

lvgl_port_create_c_image("images/esp_logo.png" "images/" "ARGB8888" "NONE")
lvgl_port_add_images(${COMPONENT_LIB} "images/")
