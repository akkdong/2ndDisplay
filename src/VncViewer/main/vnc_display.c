// vnc_display.c
//

#include "vnc_display.h"



static const char* TAG = "VNC_Disp";


//
static esp_lcd_panel_handle_t  panel_handle = NULL;
static esp_lcd_touch_handle_t  touch_handle = NULL;

static vnc_display_t vnc_disp;


// LVGL Mutex Handle for Protecting Resource Sharing Between Tasks
static SemaphoreHandle_t lvgl_mux = NULL;

// Counting semaphore for DSI frame transmission (VSYNC) king
static SemaphoreHandle_t trans_sem = NULL;




/**
 * @brief DSI Frame Transmission Complete (VSYNC) Interrupt Callback (ISR Context)
 *          A semaphore is issued whenever a frame is successfully transmitted to the LCD.
 *          The flush callback waits for this token to safely reuse the buffer.
 */

static bool notify_dsi_vsync_ready(esp_lcd_panel_handle_t panel, esp_lcd_dpi_panel_event_data_t *edata, void *user_ctx) {
    BaseType_t need_yield = pdFALSE;
    if (trans_sem) {
        xSemaphoreGiveFromISR(trans_sem, &need_yield);
    }
    return (need_yield == pdTRUE);
}


/**
 * @brief 2. LVGL v9 Display Flush Callback (DIRECT Mode)
 *          Since the LVGL buffer uses the panel's internal FB directly, draw_bitmap performs only cache write-back.
 *          It writes back the entire screen (including FB switching) from the last area of ​​the frame,,
 *          waits for the transmission complete (VSYNC) of the previous frame, and then calls flush_ready.
 */

static void lvgl_v9_dsi_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);

    if (lv_display_flush_is_last(disp)) {
        // Reflect entire frame data in panel FB (Internal FB determination → Cache write-back + cur_fb_index conversion)
        esp_lcd_panel_draw_bitmap(panel, 0, 0, BSP_LCD_H_RES, BSP_LCD_V_RES, px_map);

        // Wait until the previous frame is actually transmitted (prevents tearing)
        xSemaphoreTake(trans_sem, 0);
        xSemaphoreTake(trans_sem, portMAX_DELAY);
    }

    lv_display_flush_ready(disp);
}


/**
 * @brief LVGL v9 touch input read callback
 */

static void lvgl_v9_touch_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    esp_lcd_touch_handle_t touch = (esp_lcd_touch_handle_t)lv_indev_get_user_data(indev);
    uint16_t touch_x;
    uint16_t touch_y;
    uint8_t touch_cnt = 0;

    // Hardware touch IC data read command
    esp_lcd_touch_read_data(touch);
    bool pressed = esp_lcd_touch_get_coordinates(touch, &touch_x, &touch_y, NULL, &touch_cnt, 1);

    if (pressed && touch_cnt > 0) {
        data->point.x = touch_x;
        data->point.y = touch_y;
        data->state = LV_INDEV_STATE_PRESSED;  // v9 specification touch-down status flag
    } else {
        data->state = LV_INDEV_STATE_RELEASED; // v9 specification touch-up status flag
    }
}


/**
 * @brief LVGL System Fixed-Precision Time Axis (Tick) Registration Callback
 */

static uint32_t lvgl_tick_get_cb(void) {
    // Convert 64-bit microsecond system uptime to 32-bit milliseconds
    return esp_timer_get_time() / 1000;
}



/**
 * 
 */

static void vnc_lvgl_task(void* arg)
{
    ESP_LOGI(TAG, "Starting LVGL main loop...");
    while (1) {
        // Perform multithreaded resource locking
        if (xSemaphoreTake(lvgl_mux, portMAX_DELAY) == pdTRUE) {
            uint32_t time_till_next = lv_timer_handler();
            xSemaphoreGive(lvgl_mux);
            
            // Efficiency is achieved by yielding tasks until the next event occurs.
            vTaskDelay(pdMS_TO_TICKS(time_till_next > 0 ? time_till_next : 1));
        }
    }
}





/**
 * 
 * 
 */

vnc_display_t* vnc_display_start(void)
{
    // 1. Hardware driver execution and handle collection
    bsp_display_init(&panel_handle, &touch_handle);
    
    if (panel_handle == NULL) {
        ESP_LOGE(TAG, "LCD initialisation failed! Aborting.");
        return NULL;
    }

    // 2. Creating a mutex for multithreaded synchronization
    lvgl_mux = xSemaphoreCreateMutex();
    if (lvgl_mux == NULL) {
        ESP_LOGE(TAG, "Failed to create LVGL Mutex");
        return NULL;
    }

    // 3. LVGL v9 Core System Initialization
    lv_init();
    
    // 4. High-precision system hardware timer time axis mapping
    lv_tick_set_cb(lvgl_tick_get_cb);

    // 5. Create LVGL v9 Operation Display Instance Object
    lv_display_t *disp = lv_display_create(BSP_LCD_H_RES, BSP_LCD_V_RES);
    lv_display_set_user_data(disp, panel_handle);
    lv_display_set_flush_cb(disp, lvgl_v9_dsi_flush_cb);

    // 6. Use panel internal FB directly as LVGL direct buffer (0-copy, prevents tearing)
    uint32_t buffer_bytes = BSP_LCD_H_RES * BSP_LCD_V_RES * sizeof(lv_color16_t);
    void *buf1 = NULL;
    void *buf2 = NULL;
    ESP_ERROR_CHECK(esp_lcd_dpi_panel_get_frame_buffer(panel_handle, 2, &buf1, &buf2));

    if (buf1 == NULL || buf2 == NULL) {
        ESP_LOGE(TAG, "Failed to get frame buffers from DSI panel!");
        return NULL;
    }

    // 7. Force Direct Mode setting for optimal DSI operation
    lv_display_set_buffers(disp, buf1, buf2, buffer_bytes, LV_DISPLAY_RENDER_MODE_DIRECT);

    // 8. Register DSI frame transmission complete (VSYNC) signal
    trans_sem = xSemaphoreCreateCounting(1, 0);
    if (trans_sem == NULL) {
        ESP_LOGE(TAG, "Failed to create DSI trans semaphore");
        return NULL;
    }

    esp_lcd_dpi_panel_event_callbacks_t cbs = {
        .on_refresh_done = notify_dsi_vsync_ready,
    };

    esp_lcd_dpi_panel_register_event_callbacks(panel_handle, &cbs, disp);

    // 9. Create touchscreen input driver object and map handle (perform only when touch is initialized)
    if (touch_handle != NULL) {
        lv_indev_t *indev = lv_indev_create();
        lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_user_data(indev, touch_handle);
        lv_indev_set_read_cb(indev, lvgl_v9_touch_cb);
    }


    //
    // 10. Initialzie vnc screen : call initalize callback
    //
    vnc_disp.disp_handle = disp;
    vnc_disp.disp_width = BSP_LCD_H_RES;
    vnc_disp.disp_height = BSP_LCD_V_RES;
    vnc_disp.panel_handle = panel_handle;
    vnc_disp.touch_handle = touch_handle;
    vnc_disp.lvgl_mux = lvgl_mux;

    // 11. Final LVGL dedicated scheduler execution background task startup (Core 1 recommended)
    xTaskCreatePinnedToCore(vnc_lvgl_task, "LVGL_Task", 1024 * 8, NULL, 5, NULL, 1);

    return &vnc_disp;
}


/**
 * 
 */

 bool vnc_display_lock(vnc_display_t* disp, bool lock)
 {
    if (lock)
        return xSemaphoreTake(lvgl_mux, portMAX_DELAY) == pdTRUE;
    else
        return xSemaphoreGive(lvgl_mux) == pdTRUE;
 }
