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

vnc_display_t* vnc_display_start(void);

bool vnc_display_lock(vnc_display_t* disp, bool lock);


#ifdef __cplusplus
}
#endif
