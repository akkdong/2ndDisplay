// vnc_display.h
//

#pragma once

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "bsp_display.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * 
 */

typedef struct vnc_display 
{
    lv_display_t* disp_handle;
    int disp_width;
    int disp_height;

    esp_lcd_panel_handle_t panel_handle;
    esp_lcd_touch_handle_t touch_handle;

    SemaphoreHandle_t lvgl_mux;

} vnc_display_t;


/**
 *
 */

void vnc_display_start(void (* scrn_init_cb)(vnc_display_t* vnc));

bool vnc_display_lock(bool lock);


#ifdef __cplusplus
}
#endif
