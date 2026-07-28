#include "Arduino.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"
#include "demos/lv_demos.h"

// ESP32-P4 전용 LCD/MIPI 드라이버 헤더
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"

// JC4880P443C 하드웨어 사양 정의
#define LCD_H_RES              480
#define LCD_V_RES              800
#define LCD_LEDC_CH            LEDC_CHANNEL_0

// 글로벌 핸들 변수
static esp_lcd_dsi_bus_handle_t dsi_bus = NULL;
static esp_lcd_panel_handle_t mipi_dpi_panel = NULL;

void init_jc4880p443c_dsi_display(void)
{
    // 1. MIPI DSI 버스 구성
    esp_lcd_dsi_bus_config_t bus_cfg = {
        .bus_id = 0,
        .num_data_lanes = 2,               // 보통 2레인 구성
        .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
        .lane_bitrate_mbps = 500           // 500Mbps
    };
    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_cfg, &dsi_bus));

    // 2. MIPI DSI 엔드포인트 채널 구성
    esp_lcd_dbi_io_config_t dbi_config = {
        .virtual_channel = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    
    // 3. 타이밍 및 해상도 설정 (480x800)
    esp_lcd_mipi_dpi_panel_config_t dpi_cfg = {
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz = 24,           // 24MHz 전후 오차 조정 가능
        .virtual_channel = 0,
        .video_timing = {
            .h_size = LCD_H_RES,
            .v_size = LCD_V_RES,
            .hsync_back_porch = 40,
            .hsync_front_porch = 40,
            .hsync_pulse_width = 20,
            .vsync_back_porch = 20,
            .vsync_front_porch = 20,
            .vsync_pulse_width = 10,
        },
        .flags = {
            .use_dma2d = true              // ESP32-P4의 2D DMA 가속 사용
        }
    };

    // 4. 패널 드라이버 생성 (제조사 벤더 드라이버가 빌트인 또는 컴포넌트에 포함되어 있어야 함)
    // 일반적으로 제조사 배포 소스코드에 포함된 판넬 이니셜라이저(예: ek79007 등)를 대입합니다.
    // 여기서는 기본 생성 흐름을 보여줍니다.
    ESP_ERROR_CHECK(esp_lcd_new_panel_variant_mipi_dpi(dsi_bus, &dpi_cfg, &mipi_dpi_panel));
    
    // 디스플레이 리셋 및 켜기
    ESP_ERROR_CHECK(esp_lcd_panel_reset(mipi_dpi_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(mipi_dpi_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(mipi_dpi_panel, true));
}

void lvgl_demo_task(void *arg) 
{
    // 1. 디스플레이 하드웨어 선행 초기화
    init_jc4880p443c_dsi_display();

    // 2. LVGL 포트 레이어 초기화
    const lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(lvgl_port_init(&port_cfg));

    // 3. 고해상도 DSI용 LVGL 설정 구성
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = NULL,
        .panel_handle = mipi_dpi_panel,
        .buffer_size = LCD_H_RES * 100,        // 100라인만큼의 렌더링 버퍼 크기
        .double_buffer = true,                 // 부드러운 60fps를 위해 더블버퍼 권장
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = false,
        .color_format = LV_COLOR_FORMAT_RGB565_SWAPPED,
        .flags = {
            .buff_dma = true,
            .buff_spiram = true,               // ESP32-P4의 32MB 대용량 PSRAM 적극 활용
        }
    };

    const lvgl_port_display_dsi_cfg_t dsi_cfg = {
        .flags = {
            .avoid_tearing = true,             // DSI 찢어짐 현상 방지 ON
        }
    };

    // 4. DSI 디스플레이 추가
    lv_display_t *disp = lvgl_port_add_disp_dsi(&disp_cfg, &dsi_cfg);
    
    // 화면 방향 조정 (기본 포트레이트가 세로형일 경우 가로 세로 전환)
    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_0);

    // 5. LVGL 데모 스레드 락 후 실행
    if (lvgl_port_lock(0)) {
        lv_demo_widgets(); // 또는 lv_demo_benchmark();
        lvgl_port_unlock();
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

extern "C" void app_main() 
{
    initArduino();
    
    // MIPI DSI 전용 대용량 스택 배정 태스크 생성 (8KB 이상)
    xTaskCreatePinnedToCore(lvgl_demo_task, "lvgl_demo_dsi", 8 * 1024, NULL, 5, NULL, 1);
}
