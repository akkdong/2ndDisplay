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

#include "lvgl.h"
#include "demos/lv_demos.h"

static const char *TAG = "MAIN";


//
//
//

#include "sdkconfig.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st7701.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_ldo_regulator.h"

#define LCD_H_RES              480
#define LCD_V_RES              800
#define LCD_DPI_CLK_MHZ        34 // 25

#define LCD_HSYNC              12  // 10
#define LCD_HBP                42 // 40
#define LCD_HFP                42 // 40
#define LCD_VSYNC              2  // 4
#define LCD_VBP                8  // 13
#define LCD_VFP                166 // 2

#define MIPI_DSI_LANE_NUM      2
#define MIPI_DSI_LANE_RATE     1000

#define MIPI_PHY_LDO_CHAN      3
#define MIPI_PHY_LDO_MV       2500

#define LCD_RST_PIN            5
#define LCD_BK_LIGHT_PIN       23

extern "C" void disp_init(esp_lcd_panel_handle_t* panel_handle);
extern "C" void touch_init();
extern "C" bool touch_driver_read(int16_t* x, int16_t* y);


static bool notify_color_trans_done(esp_lcd_panel_handle_t panel, esp_lcd_dpi_panel_event_data_t *edata, void *user_ctx) 
{
    lv_display_t *disp = (lv_display_t *)user_ctx;
    // LVGL v9: 플러시 완료 통보
    //ESP_LOGI(TAG, "[] notify_color_trans_done()");
    lv_display_flush_ready(disp);
    return false;
}

static  void my_disp_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) 
{
    //ESP_LOGI(TAG, "[] my_disp_flush_cb(START)");
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);
    
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;

    // esp_lcd를 통해 화면에 픽셀 데이터 전송
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, px_map);
    //lv_display_flush_ready(disp); 
    //ESP_LOGI(TAG, "[] my_disp_flush_cb(DONE)");
}



static void my_touchpad_read_cb(lv_indev_t * indev, lv_indev_data_t * data)
{
    int16_t touch_x = 0;
    int16_t touch_y = 0;
    bool is_pressed = false;

    // TODO: 개발자님이 만드신 기존 터치 드라이버 함수를 여기에 호출하세요.
    is_pressed = touch_driver_read(&touch_x, &touch_y);

    if(is_pressed) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = touch_x;
        data->point.y = touch_y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void lvgl_touch_init(void)
{
    // 입력 장치 객체 생성
    lv_indev_t * indev = lv_indev_create();
    
    // 장치 타입을 포인터(터치스크린, 마우스 등)로 설정
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    
    // 위에서 만든 읽기 콜백 함수 등록
    lv_indev_set_read_cb(indev, my_touchpad_read_cb);
}

void lvgl_port_init(esp_lcd_panel_handle_t panel_handle) 
{
    // 1. LVGL 핵심 코어 초기화
    ESP_LOGI(TAG, "lv_init()");
    lv_init();

#if 1
    // 2. LVGL v9 디스플레이 객체 생성
    ESP_LOGI(TAG, "lv_display_create()");
    lv_display_t *disp = lv_display_create(LCD_H_RES, LCD_V_RES);
    
    // esp_lcd 패널 핸들을 유저 데이터로 저장 (콜백에서 사용)
    ESP_LOGI(TAG, "lv_display_set_user_data()");
    lv_display_set_user_data(disp, panel_handle);

    // 3. 내부/외장 RAM 버퍼 할당 (v9은 1바이트 포인터 배열로 전달 가능)
    ESP_LOGI(TAG, "heap_caps_malloc(1)");
    void *buf1 = heap_caps_malloc(LCD_H_RES * LCD_V_RES * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    //ESP_LOGI(TAG, "heap_caps_malloc(2)");
    //void *buf2 = heap_caps_malloc(LCD_H_RES * LCD_V_RES * sizeof(uint16_t), MALLOC_CAP_SPIRAM); // 더블 버퍼 사용 시 (선택)
    assert(buf1 != NULL);

    // 디스플레이에 버퍼 등록
    ESP_LOGI(TAG, "lv_display_set_buffers()");
    lv_display_set_buffers(disp, buf1, 0/*buf2*/, LCD_H_RES * LCD_V_RES * sizeof(uint16_t), LV_DISPLAY_RENDER_MODE_PARTIAL);

    // 4. 플러시 콜백 등록
    ESP_LOGI(TAG, "lv_display_set_flush_cb()");
    lv_display_set_flush_cb(disp, my_disp_flush_cb);

    // 5. esp_lcd 전송 완료 이벤트에 콜백 등록 (플러시 완료 연동)
    //const esp_lcd_panel_dev_config_t *dev_cfg; // disp_init에서 설정한 config
    // 주의: 실제 코드에서는 disp_init 내부에서 이 콜백을 등록하거나, panel_handle 생성이 끝난 직후 등록해야 합니다.
    //TaskHandle_t trans_done_task = xTaskGetCurrentTaskHandle();
    esp_lcd_dpi_panel_event_callbacks_t cbs = {
        .on_color_trans_done = notify_color_trans_done,
    };
    ESP_LOGI(TAG, "esp_lcd_dpi_panel_register_event_callbacks()");
    ESP_ERROR_CHECK(esp_lcd_dpi_panel_register_event_callbacks(panel_handle, &cbs, disp));
#endif

#if 1
    //
    lvgl_touch_init();
#endif

    ESP_LOGI(TAG, "lvgl_port_init() DONE");    
}

static void lvgl_tick_cb(void *arg) 
{
    lv_tick_inc(1);
}

void lvgl_task(void *arg) 
{
    // 1ms 주기 esp_timer 생성 및 시작
    const esp_timer_create_args_t tick_timer_args = {
        .callback = &lvgl_tick_cb,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t tick_timer;
    esp_timer_create(&tick_timer_args, &tick_timer);
    esp_timer_start_periodic(tick_timer, 1000); // 1000us = 1ms

    // 2. LVGL 핸들러 루프 실행
    ESP_LOGI(TAG, "Start LVGL main task");
    while (1) 
    {
        // lv_timer_handler()는 다음 실행까지 필요한 대기 시간을 ms 단위로 반환합니다.
        uint32_t task_delay_ms = lv_timer_handler();
        /*
        if (task_delay_ms < 1) {
            task_delay_ms = 1;
        } else if (task_delay_ms > 30) {
            task_delay_ms = 30; // 지나치게 긴 대기 방지
        }
        */
        //ESP_LOGI(TAG, "[*] delay_ms : %u", task_delay_ms);
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}


static lv_obj_t * my_label;
static int counter = 0;

/* 1초마다 실행될 콜백 함수 */
static void timer_callback(lv_timer_t * timer)
{
    counter++;
    
    /* 카운터 값에 따라 글자 변경 (printf 포맷 사용) */
    lv_label_set_text_fmt(my_label, "Count: %d", counter);
}


extern "C" void app_main(void)
{
#if 1
    //
    esp_lcd_panel_handle_t panel_handle = NULL;   
    disp_init(&panel_handle); 
    assert(panel_handle != NULL);

    //
    touch_init();

    //
    lvgl_port_init(panel_handle);

    #if 1

    #if 1
    lv_demo_widgets(); 
    #else
    {
        lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x200000), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, LV_PART_MAIN);

        /* 2. 텍스트 라벨 생성 */
        lv_obj_t * label = lv_label_create(lv_screen_active());
        my_label = label;
        
        /* 3. "Hello" 글자 설정 */
        lv_label_set_text(label, "Hello");

        /* 4. 왼쪽 상단 정렬 및 여백(Offset) 설정 */
        lv_obj_align(label, LV_ALIGN_TOP_LEFT, 10, 10);

        /* 5. 글자 색상 변경 (선택 사항: 빨간 배경에서 잘 보이도록 흰색 설정) */
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        //
        lv_obj_set_style_text_font(label, &lv_font_montserrat_26, LV_PART_MAIN);


        //
        lv_timer_create(timer_callback, 1000, NULL);
    }
    #endif

    #if 1
    xTaskCreatePinnedToCore(
        lvgl_task,          // 태스크 함수
        "LVGL Task",        // 태스크 이름
        1024 * 16,           // 스택 크기 (LVGL v9은 최소 4~5KB 이상 권장)
        NULL,               // 전달 인자
        5,                  // 태스크 우선순위 (비교적 높은 순위 부여)
        NULL,               // 태스크 핸들
        1                   // 실행할 CPU 코어 번호 (0 또는 1)
    );
    #else
    lvgl_task(nullptr);
    #endif
    #endif

    //
    while (1) 
    {
        //ESP_LOGI(TAG, "1초마다 출력되는 로그입니다.");
        
        // 1000ms(1초) 동안 태스크를 대기 상태로 전환
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
#endif

#if 0
    //
    //
    initArduino();
    //

    Serial.begin(115200);
    while (!Serial);

    while (1)
    {
        Serial.println("Hello");
        delay(1000);
    }
#endif

#if 0
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
#endif   
}
