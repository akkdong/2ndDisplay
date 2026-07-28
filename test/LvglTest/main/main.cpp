// main.cpp
//

#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
//#include "nvs_flash.h"
//#include "nvs.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_memory_utils.h"
#include "bsp/esp-bsp.h"

#include "demos/lv_demos.h"


//
//
//

extern "C" void app_main(void)
{
    //
    initArduino();

    //
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = 480 * 800,
        .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
        .flags = {
            .buff_dma = false,
            .buff_spiram = true,
            .sw_rotate = true,
        }
    };
    bsp_display_start_with_config(&cfg);
    bsp_display_backlight_on();

    bsp_display_lock(0);
    {
        lv_demo_widgets();
    }
    bsp_display_unlock();

    bsp_display_brightness_set(100);
}
