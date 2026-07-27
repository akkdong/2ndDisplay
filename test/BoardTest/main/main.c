#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_ldo_regulator.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/usb_serial_jtag.h"
#include "esp_lcd_st7701.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_gt911.h"

static const char *TAG = "main";

#define TEST_WIFI               1


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

#define TOUCH_I2C_SDA_PIN      7
#define TOUCH_I2C_SCL_PIN      8
#define TOUCH_RST_PIN          22
#define TOUCH_INT_PIN          21
#define TOUCH_I2C_SPEED_HZ     400000

#include "private.h"

#define WIFI_CONNECTED_BIT      BIT0
#define WIFI_FAIL_BIT           BIT1

#define USB_SERIAL_BUF_SIZE    512

static const st7701_lcd_init_cmd_t lcd_init_cmds[] = {
    {0xFF, (uint8_t []){0x77,0x01,0x00,0x00,0x13}, 5, 0},
    {0xEF, (uint8_t []){0x08}, 1, 0},
    {0xFF, (uint8_t []){0x77,0x01,0x00,0x00,0x10}, 5, 0},
    {0xC0, (uint8_t []){0x63, 0x00}, 2, 0},
    {0xC1, (uint8_t []){0x0D, 0x02}, 2, 0},
    {0xC2, (uint8_t []){0x10, 0x08}, 2, 0},
    {0xCC, (uint8_t []){0x10}, 1, 0},
    {0xB0, (uint8_t []){0x80,0x09,0x53,0x0C,0xD0,0x07,0x0C,0x09,0x09,0x28,0x06,0xD4,0x13,0x69,0x2B,0x71}, 16, 0},
    {0xB1, (uint8_t []){0x80,0x94,0x5A,0x10,0xD3,0x06,0x0A,0x08,0x08,0x25,0x03,0xD3,0x12,0x66,0x6A,0x0D}, 16, 0},

    {0xFF, (uint8_t []){0x77, 0x01, 0x00, 0x00, 0x11}, 5, 0},
    {0xB0, (uint8_t []){0x5D}, 1, 0},
    {0xB1, (uint8_t []){0x58}, 1, 0},
    {0xB2, (uint8_t []){0x87}, 1, 0},
    {0xB3, (uint8_t []){0x80}, 1, 0},
    {0xB5, (uint8_t []){0x4E}, 1, 0},
    {0xB7, (uint8_t []){0x85}, 1, 0},
    {0xB8, (uint8_t []){0x21}, 1, 0},
    {0xB9, (uint8_t []){0x10, 0x1F}, 2, 0},
    {0xBB, (uint8_t []){0x03}, 1, 0},
    {0xBC, (uint8_t []){0x00}, 1, 0},
    {0xC1, (uint8_t []){0x78}, 1, 0},
    {0xC2, (uint8_t []){0x78}, 1, 0},
    {0xD0, (uint8_t []){0x88}, 1, 100},
    {0xE0, (uint8_t []){0x00, 0x3A, 0x02}, 3, 0},
    {0xE1, (uint8_t []){0x04,0xA0,0x00,0xA0,0x05,0xA0,0x00,0xA0,0x00,0x40,0x40}, 11, 0},
    {0xE2, (uint8_t []){0x30,0x00,0x40,0x40,0x32,0xA0,0x00,0xA0,0x00,0xA0,0x00,0xA0,0x00}, 13, 0},
    {0xE3, (uint8_t []){0x00,0x00,0x33,0x33}, 4, 0},
    {0xE4, (uint8_t []){0x44, 0x44}, 2, 0},
    {0xE5, (uint8_t []){0x09,0x2E,0xA0,0xA0,0x0B,0x30,0xA0,0xA0,0x05,0x2A,0xA0,0xA0,0x07,0x2C,0xA0,0xA0}, 16, 0},
    {0xE6, (uint8_t []){0x00,0x00,0x33,0x33}, 4, 0},
    {0xE7, (uint8_t []){0x44, 0x44}, 2, 0},
    {0xE8, (uint8_t []){0x08,0x2D,0xA0,0xA0,0x0A,0x2F,0xA0,0xA0,0x04,0x29,0xA0,0xA0,0x06,0x2B,0xA0,0xA0}, 16, 0},
    {0xEB, (uint8_t []){0x00,0x00,0x4E,0x4E,0x00,0x00,0x00}, 7, 0},
    {0xEC, (uint8_t []){0x08, 0x01}, 2, 0},
    {0xED, (uint8_t []){0xB0,0x2B,0x98,0xA4,0x56,0x7F,0xFF,0xFF,0xFF,0xFF,0xF7,0x65,0x4A,0x89,0xB2,0x0B}, 16, 0},
    {0xEF, (uint8_t []){0x08,0x08,0x08,0x45,0x3F,0x54}, 6, 0},
    {0xFF, (uint8_t []){0x77, 0x01, 0x00, 0x00, 0x00}, 5, 0},

    {0x11, (uint8_t []){0x00}, 0, 120},
    {0x29, (uint8_t []){0x00}, 0, 20},
};

static bool notify_color_trans_done(esp_lcd_panel_handle_t panel,
                                    esp_lcd_dpi_panel_event_data_t *edata,
                                    void *user_ctx)
{
    TaskHandle_t task_handle = (TaskHandle_t)user_ctx;
    if (task_handle) {
        xTaskNotifyGive(task_handle);
    }
    return false;
}

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
#define WIFI_MAX_RETRY          10

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retrying WiFi connection (%d/%d)", s_retry_num, WIFI_MAX_RETRY);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t inst_any_id;
    esp_event_handler_instance_t inst_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                    &wifi_event_handler, NULL, &inst_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                    &wifi_event_handler, NULL, &inst_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                       WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                       pdFALSE, pdFALSE, portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WiFi connected to %s", WIFI_SSID);
    } else {
        ESP_LOGE(TAG, "WiFi connection failed");
    }
}

static void touch_task(void *arg)
{
    esp_lcd_touch_handle_t tp = (esp_lcd_touch_handle_t)arg;
    esp_lcd_touch_point_data_t point_data;
    uint8_t cnt = 0;

    while (1) {
        esp_lcd_touch_read_data(tp);
        if (esp_lcd_touch_get_data(tp, &point_data, &cnt, 1) == ESP_OK && cnt > 0) {
            ESP_LOGI(TAG, "Touch X:%d Y:%d", point_data.x, point_data.y);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing MIPI DSI PHY power");
    esp_ldo_channel_handle_t ldo_mipi = NULL;
    esp_ldo_channel_config_t ldo_config = {
        .chan_id = MIPI_PHY_LDO_CHAN,
        .voltage_mv = MIPI_PHY_LDO_MV,
    };
    ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo_config, &ldo_mipi));

    ESP_LOGI(TAG, "Init LCD backlight pin");
    gpio_config_t bk_io_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << LCD_BK_LIGHT_PIN,
    };
    ESP_ERROR_CHECK(gpio_config(&bk_io_config));
    gpio_set_level(LCD_BK_LIGHT_PIN, 0);

    ESP_LOGI(TAG, "Create MIPI DSI bus");
    esp_lcd_dsi_bus_handle_t dsi_bus = NULL;
    esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id = 0,
        .num_data_lanes = MIPI_DSI_LANE_NUM,
        .lane_bit_rate_mbps = MIPI_DSI_LANE_RATE,
    };
    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &dsi_bus));

    ESP_LOGI(TAG, "Create DBI IO");
    esp_lcd_panel_io_handle_t dbi_io = NULL;
    esp_lcd_dbi_io_config_t dbi_config = {
        .virtual_channel = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(dsi_bus, &dbi_config, &dbi_io));

    ESP_LOGI(TAG, "Create ST7701 DPI panel");
    esp_lcd_panel_handle_t panel = NULL;
    esp_lcd_dpi_panel_config_t dpi_config = {
        .virtual_channel = 0,
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz = LCD_DPI_CLK_MHZ,
        .in_color_format = LCD_COLOR_FMT_RGB565,
        .video_timing = {
            .h_size = LCD_H_RES,
            .v_size = LCD_V_RES,
            .hsync_pulse_width = LCD_HSYNC,
            .hsync_back_porch = LCD_HBP,
            .hsync_front_porch = LCD_HFP,
            .vsync_pulse_width = LCD_VSYNC,
            .vsync_back_porch = LCD_VBP,
            .vsync_front_porch = LCD_VFP,
        },
    };
    st7701_vendor_config_t vendor_config = {
        .init_cmds = lcd_init_cmds,
        .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
        .flags.use_mipi_interface = 1,
        .mipi_config = {
            .dsi_bus = dsi_bus,
            .dpi_config = &dpi_config,
        },
    };
    esp_lcd_panel_dev_config_t dev_config = {
        .reset_gpio_num = LCD_RST_PIN,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = &vendor_config,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7701(dbi_io, &dev_config, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));
    gpio_set_level(LCD_BK_LIGHT_PIN, 1);

    ESP_LOGI(TAG, "LCD initialized, filling red");

    size_t buf_size = LCD_H_RES * LCD_V_RES * sizeof(uint16_t);
    uint8_t *frame_buf = (uint8_t *)heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
    assert(frame_buf);

    uint16_t red = 0x000000; //0xF800;
    memset(frame_buf, 0, buf_size);
    for (int i = 0; i < LCD_H_RES * LCD_V_RES; i++) {
        ((uint16_t *)frame_buf)[i] = red;
    }

    TaskHandle_t trans_done_task = xTaskGetCurrentTaskHandle();
    esp_lcd_dpi_panel_event_callbacks_t cbs = {
        .on_color_trans_done = notify_color_trans_done,
    };
    ESP_ERROR_CHECK(esp_lcd_dpi_panel_register_event_callbacks(panel, &cbs, trans_done_task));

    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, 0, 0, LCD_H_RES, LCD_V_RES, frame_buf));
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "Red screen displayed");

    #if TEST_WIFI
    ESP_LOGI(TAG, "Initialize WiFi STA");
    wifi_init_sta();
    #endif

    ESP_LOGI(TAG, "Initialize USB Serial/JTAG");
    usb_serial_jtag_driver_config_t usb_serial_config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    usb_serial_config.rx_buffer_size = USB_SERIAL_BUF_SIZE;
    usb_serial_config.tx_buffer_size = USB_SERIAL_BUF_SIZE;
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb_serial_config));

    ESP_LOGI(TAG, "Initialize GT911 touch");
    i2c_master_bus_config_t i2c_bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = TOUCH_I2C_SDA_PIN,
        .scl_io_num = TOUCH_I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t i2c_bus = NULL;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &i2c_bus));

    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &tp_io_config, &tp_io_handle));

    esp_lcd_touch_io_gt911_config_t tp_gt911_config = {
        .dev_addr = tp_io_config.dev_addr,
    };
    esp_lcd_touch_config_t tp_cfg = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = TOUCH_RST_PIN,
        .int_gpio_num = TOUCH_INT_PIN,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
        .driver_data = &tp_gt911_config,
    };
    esp_lcd_touch_handle_t tp = NULL;
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &tp));
    ESP_LOGI(TAG, "GT911 touch initialized");

    xTaskCreate(touch_task, "touch", 4096, tp, 5, NULL);

    ESP_LOGI(TAG, "USB Serial Echo + Touch ready");
    uint8_t rx_buf[USB_SERIAL_BUF_SIZE];
    while (1) {
        int len = usb_serial_jtag_read_bytes(rx_buf, sizeof(rx_buf), pdMS_TO_TICKS(100));
        if (len > 0) {
            usb_serial_jtag_write_bytes(rx_buf, len, pdMS_TO_TICKS(100));
        }
    }
}
