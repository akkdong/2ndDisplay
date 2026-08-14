// lvgl_init.c
//

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "sdl_init.h"
#include "lvgl_init.h"

static void periodic_update_cb(lv_timer_t* timer)
{
    static uint32_t tick = 1;
    lv_obj_t* label = (lv_obj_t*)lv_timer_get_user_data(timer);
    
    char num[32];
    sprintf_s(num, sizeof(num), "Update: %u", tick++);
    lv_label_set_text(label, num);
}

#if 0
typedef struct {
    lv_disp_t* disp;
    lv_obj_t* label;
    lv_timer_t* timer;
} TimerUserData_t;

TimerHandle_t xMyPeriodicTimer = NULL;
TimerUserData_t MyData;

void vMyTimerCallback(TimerHandle_t xTimer) 
{
    TimerUserData_t* pData = (TimerUserData_t*)pvTimerGetTimerID(xTimer);
    periodic_update_cb(pData->timer);
}
#endif

// FreeRTOS Dedicated GUI Task
void vGuiEngineTask(void* pvParameters) 
{
    lv_init();

    // Setup LVGL display draw buffers (Internal MCU RAM)
    {
        static lv_color_t buf1[DISPLAY_WIDTH * 40]; // 40 lines buffer
        lv_display_t* display = lv_display_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);
        lv_display_set_buffers(display, buf1, NULL, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
        lv_display_set_flush_cb(display, lcd_flush_cb);
    }
    // Setup mouse handler
    {
        // 1. Create a base input device object
        lv_indev_t* mouse_indev = lv_indev_create();

        // 2. Classify it strictly as a POINTER type (Touch/Mouse)
        lv_indev_set_type(mouse_indev, LV_INDEV_TYPE_POINTER);

        // 3. Register your multi-threaded read handler
        lv_indev_set_read_cb(mouse_indev, mouse_read_cb);
    }

    //
    {
        lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0xFFC0C0), 0);

        // Create your LVGL Widgets here (buttons, labels, gauges)
        lv_obj_t* label = lv_label_create(lv_screen_active());
        lv_label_set_text(label, "Hello from FreeRTOS Task!");
        lv_obj_center(label);

        lv_timer_t* timer = lv_timer_create(periodic_update_cb, 1000, label);
        /*
        MyData.disp = NULL;
        MyData.label = label;
        MyData.timer = timer;

        xMyPeriodicTimer = xTimerCreate(
            "Periodic_Timer",           // Text name for debugging
            pdMS_TO_TICKS(1000),        // Timer period in ticks (1000ms)
            pdTRUE,                     // pdTRUE = Auto-Reload, pdFALSE = One-Shot
            (void*)&MyData,                 // Unique ID (can be used for shared callbacks)
            vMyTimerCallback            // The callback function assigned
        );
        if (xMyPeriodicTimer != NULL)
            xTimerStart(xMyPeriodicTimer, 0);
        */
    }

    // LVGL System Engine Tick Loop
    uint32_t last_tick = SDL_GetTicks();

    while (1) 
    {
        //
        uint32_t current_time = SDL_GetTicks();
        uint32_t elapsed_time = current_time - last_tick;
        last_tick = current_time;
        lv_tick_inc(elapsed_time);

        // Run LVGL internal timers and render logic
        uint32_t time_till_next = lv_timer_handler();

        // Prevent starvation, yield back to lower priority tasks
        vTaskDelay(pdMS_TO_TICKS(time_till_next < 5 ? 5 : time_till_next));
    }
}



void lvgl_init()
{
    xTaskCreate(vGuiEngineTask, "LVGL_Task", 2048, NULL, tskIDLE_PRIORITY + 2, NULL);
}
