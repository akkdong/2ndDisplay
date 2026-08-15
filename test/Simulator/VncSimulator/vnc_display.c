// vnc_display.c
//

#include "vnc_display.h"
#include "bsp_display.h"
#include "SDL2/SDL.h"


#define ENABLE_SDL_FEATURE	1

#define DISPLAY_WIDTH		480
#define DISPLAY_HEIGHT		800



static vnc_display_t disp = {
    .disp_handle = NULL,
    .disp_width = DISPLAY_WIDTH,
    .disp_height = DISPLAY_HEIGHT,

#if !defined(_SIMULATOR)
    .panel_handle = NULL,
    .touch_handle = NULL,
#endif

    .lvgl_mux = NULL,
};


static void vnc_lvgl_task(void* param)
{
    // LVGL System Engine Tick Loop
    uint32_t last_tick = SDL_GetTicks();

    while (1)
    {
        vnc_display_lock(&disp, true);
        //{

        //
        uint32_t current_time = SDL_GetTicks();
        uint32_t elapsed_time = current_time - last_tick;
        last_tick = current_time;
        lv_tick_inc(elapsed_time);

        // Run LVGL internal timers and render logic
        uint32_t time_till_next = lv_timer_handler();

        //}
        vnc_display_lock(&disp, false);

        // Prevent starvation, yield back to lower priority tasks
        vTaskDelay(pdMS_TO_TICKS(time_till_next < 5 ? 5 : time_till_next));
    }

    //
    vTaskDelete(NULL);
}



/**
 *
 */

vnc_display_t* vnc_display_start(void)
{
    disp.lvgl_mux = xSemaphoreCreateMutex();
    if (disp.lvgl_mux == NULL)
        return NULL;

    bsp_display_init(&disp);

    //
    lv_init();

    // Setup LVGL display draw buffers (Internal MCU RAM)
    {
        static lv_color_t buf1[DISPLAY_WIDTH * 40]; // 40 lines buffer
        lv_display_t* display = lv_display_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);
        lv_display_set_buffers(display, buf1, NULL, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
        lv_display_set_flush_cb(display, disp.display_flush);
    }

    // Setup mouse handler
    {
        // 1. Create a base input device object
        lv_indev_t* mouse_indev = lv_indev_create();

        // 2. Classify it strictly as a POINTER type (Touch/Mouse)
        lv_indev_set_type(mouse_indev, LV_INDEV_TYPE_POINTER);

        // 3. Register your multi-threaded read handler
        lv_indev_set_read_cb(mouse_indev, disp.touch_input);
    }

    // Default screen
    {
        lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0xD0D0FF), 0);

        lv_obj_t* label = lv_label_create(lv_screen_active());
        lv_label_set_text(label, "Start VNC Simulator");
        lv_obj_center(label);
    }

    xTaskCreate(vnc_lvgl_task, "vnc_display_task", 4 * 1024, &disp, tskIDLE_PRIORITY + 2, NULL);

    return &disp;
}

bool vnc_display_lock(vnc_display_t* disp, bool lock)
{
    if (lock)
        return xSemaphoreTake(disp->lvgl_mux, portMAX_DELAY) == pdTRUE;
    else
        return xSemaphoreGive(disp->lvgl_mux) == pdTRUE;
}
