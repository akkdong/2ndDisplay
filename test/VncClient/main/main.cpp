// main.cpp
//

#include <Arduino.h>
#include <WiFi.h>
#include "board_defines.h"
#include "board_init.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st7701.h"
#include "esp_lvgl_port.h"
#include "lv_demos.h"


static const char *TAG = "MAIN";

//
//
//


extern esp_lcd_panel_handle_t panel_handle;
extern esp_lcd_touch_handle_t tp_handle;

size_t buf_size;
uint8_t* frame_buf;



//
//
//

void lvgl_demo_task(void *arg) {
    // 1. 여기에 본인 디스플레이 패널 인터페이스(SPI/I2C) 및 LCD 드라이버 초기화 코드 작성
    // esp_lcd_new_panel_io_spi(...);
    // esp_lcd_new_panel_st7789(...);

    // 2. LVGL 포트 초기화 설정
    const lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_CONFIG_DEFAULT();
    esp_err_t err = lvgl_port_init(&port_cfg);
    if (err != ESP_OK) {
        printf("LVGL 포트 초기화 실패!\n");
        vTaskDelete(NULL);
    }

    // 3. 디스플레이를 LVGL 포트에 추가
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = NULL,        // 본인의 io_handle 대입
        .panel_handle = NULL,     // 본인의 panel_handle 대입
        .buffer_size = 320 * 240 / 10, // 버퍼 크기
        .double_buffer = true,
        .hres = 320,              // 가로 해상도
        .vres = 240,              // 세로 해상도
        .monochrome = false,
        .color_format = LV_COLOR_FORMAT_RGB565_SWAPPED, // 에러가 나던 포맷이 이제 정상 작동합니다.
        .flags = {
            .buff_dma = true,
        }
    };
    lv_display_t * disp = lvgl_port_add_disp(&disp_cfg);

    // 4. LVGL 스레드 세이프 락을 걸고 데모 실행
    if (lvgl_port_lock(0)) {
        
        // 실행하고 싶은 데모 함수 하나만 주석을 해제하세요.
        // (주의: sdkconfig나 lv_conf.h에서 해당 데모가 1로 활성화되어 있어야 합니다)
        lv_demo_widgets(); 
        // lv_demo_benchmark();
        // lv_demo_music();

        lvgl_port_unlock(); // 락 해제 필수
    }

    // 이 태스크는 종료되지 않고 대기합니다 (esp_lvgl_port가 내부 타이머 task를 별도 운영함)
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


//
//
//

void setup()
{
    delay(500);
    Serial.begin(115200);
    //while(!Serial);
    delay(500);

    /*
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.print("Wi-Fi 연결 중: ");
    Serial.println(ssid);

     while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nWi-Fi 연결 성공!");
    Serial.print("할당받은 IP 주소: ");
    Serial.println(WiFi.localIP());
    */

    bsp_init_lcd();
    bsp_init_touch();

#if 1
    xTaskCreatePinnedToCore(lvgl_demo_task, "lvgl_demo", 8 * 1024, NULL, 5, NULL, 1);
#else
    //
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    esp_err_t err = lvgl_port_init(&lvgl_cfg);

    // Display 등록
    lvgl_port_display_cfg_t disp_cfg = {
        .panel_handle = panel_handle,
        .buffer_size = LCD_H_RES * 100, // PSRAM 활용 버퍼 크기
        .double_buffer = true,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = false,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .buff_spiram = true, // ESP32-P4 internal PSRAM 사용
        }
    };
    lvgl_port_display_dsi_cfg_t dsi_cfg = {
        .flags = {
            .avoid_tearing = 1
        }
    };
    lv_disp_t *disp = lvgl_port_add_disp_dsi(&disp_cfg, &dsi_cfg);

    // Touch 등록
    lvgl_port_touch_cfg_t touch_cfg = {
        .disp = disp,
        .handle = tp_handle,
    };
    lvgl_port_add_touch(&touch_cfg);


    // -------------------------------------------------------------------------
    // 5. LVGL 9.x Demo 실행
    // -------------------------------------------------------------------------
    ESP_LOGI(TAG, "Starting LVGL Demo...");
    if (lvgl_port_lock(0)) {
        // Widgets 데모 실행 (menuconfig에서 설정 필요)
        lv_demo_widgets();
        // 또는 Music 데모 실행시: lv_demo_music();
        
        lvgl_port_unlock();
    }
#endif

    //
#if 0
    buf_size = LCD_H_RES * LCD_V_RES * sizeof(uint16_t);
    frame_buf = (uint8_t *)heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);

    lcd_register_event_callbacks();
#endif
}

void loop()
{
#if 0
    while(1)
    {
        Serial.println("[TEST] Fill screen to RED");
        {
            uint16_t red = 0x2000;
            memset(frame_buf, 0, buf_size);
            for (int i = 0; i < LCD_H_RES * LCD_V_RES; i++) {
                ((uint16_t *)frame_buf)[i] = red;
            }

            lcd_draw_bitmap(0, 0, LCD_H_RES, LCD_V_RES, frame_buf);
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
            ESP_LOGI(TAG, "[TEST] Red screen displayed");
        }

        delay(1000);

        Serial.println("[TEST] Fill screen to BLUE");
        {
            uint16_t blue = 0x0004;
            memset(frame_buf, 0, buf_size);
            for (int i = 0; i < LCD_H_RES * LCD_V_RES; i++) {
                ((uint16_t *)frame_buf)[i] = blue;
            }

            lcd_draw_bitmap(0, 0, LCD_H_RES, LCD_V_RES, frame_buf);
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
            ESP_LOGI(TAG, "[TEST] Blue screen displayed");
        }

        delay(1000);        
    }
#else
    delay(1000);
#endif
}
