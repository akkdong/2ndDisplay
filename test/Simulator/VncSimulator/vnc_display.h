// vnc_display.h
//

#pragma once

#include <stdio.h>
#if defined(_SIMULATOR)
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "event_groups.h"
#else
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "bsp_display.h"
#endif

#include "lvgl.h"
#include "extern.h"


BEGIN_EXTERN_C()


/**
 *
 */

typedef uint16_t lcd_color_t;


typedef struct vnc_display
{
    lv_display_t* disp_handle;
    int disp_width;
    int disp_height;

#if defined(_SIMULATOR)
    void (*display_flush)(lv_display_t* display, const lv_area_t* area, uint8_t* px_map);
    void (*touch_input)(lv_indev_t* indev, lv_indev_data_t* data);
#else
    esp_lcd_panel_handle_t panel_handle;
    esp_lcd_touch_handle_t touch_handle;
#endif

    SemaphoreHandle_t lvgl_mux;

} vnc_display_t;



/**
 *
 */

vnc_display_t* vnc_display_start(void);

bool vnc_display_lock(vnc_display_t* disp, bool lock);


END_EXTERN_C()
