// main.c
//

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl_port.h"
#include "lvgl.h"
#include "demos/lv_demos.h"
#include "bsp.h"



//
//
//

void app_main()
{
    //
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = {
            .task_priority = 1,
            .task_stack = 7168,
            .task_affinity = -1,
            .task_max_sleep_ms = 500,
            .task_stack_caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_DEFAULT,
            .timer_period_ms = 5,
        },
        .buffer_size = 480 * 800,
        .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
        .flags = {
            .buff_dma = false,
            .buff_spiram = true,
            .sw_rotate = true,
        }
    };

    //bsp_display_start();
    lv_display_t* disp = bsp_display_start_with_config(&cfg);

    bsp_display_lock(0);
    {
        lv_demo_widgets();
    }
    bsp_display_unlock();

    bsp_display_brightness_set(60);
    bsp_display_backlight_on();

    /*
    while(1)
    {
        uint32_t time_till_next = lv_timer_handler();
        lv_delay_ms(time_till_next);
    }
    */
}
