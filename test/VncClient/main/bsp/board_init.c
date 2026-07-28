// board_init.c
//


#include "esp_log.h"
#include "esp_err.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"

#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_ldo_regulator.h"

#include "esp_lcd_st7701.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_gt911.h"

#include "board_defines.h"
#include "board_init.h"

static const char *TAG = "BSP";



#include "private.h"

#define WIFI_CONNECTED_BIT      BIT0
#define WIFI_FAIL_BIT           BIT1


//
//
//

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

esp_lcd_panel_handle_t panel_handle = NULL;
esp_lcd_touch_handle_t tp_handle = NULL;



/// @brief 
//
//
//

void bsp_init_gpio()
{
   

}


void bsp_init_lcd()
{
    ESP_LOGI(TAG, "[LCD] Init backlight pin");
    gpio_config_t bk_io_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << LCD_BK_LIGHT_PIN,
    };
    ESP_ERROR_CHECK(gpio_config(&bk_io_config));
    gpio_set_level(LCD_BK_LIGHT_PIN, 0);

    ESP_LOGI(TAG, "[LCD] Initializing MIPI DSI PHY power");
    esp_ldo_channel_handle_t ldo_mipi = NULL;
    esp_ldo_channel_config_t ldo_config = {
        .chan_id = MIPI_PHY_LDO_CHAN,
        .voltage_mv = MIPI_PHY_LDO_MV,
    };
    ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo_config, &ldo_mipi));

    ESP_LOGI(TAG, "[LCD] Create MIPI DSI bus");
    esp_lcd_dsi_bus_handle_t dsi_bus = NULL;
    esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id = 0,
        .num_data_lanes = MIPI_DSI_LANE_NUM,
        .lane_bit_rate_mbps = MIPI_DSI_LANE_RATE,
    };
    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &dsi_bus));

    ESP_LOGI(TAG, "[LCD] Create DBI IO");
    esp_lcd_panel_io_handle_t dbi_io = NULL;
    esp_lcd_dbi_io_config_t dbi_config = {
        .virtual_channel = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(dsi_bus, &dbi_config, &dbi_io));

    ESP_LOGI(TAG, "[LCD] Create ST7701 DPI panel");
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

    ESP_ERROR_CHECK(esp_lcd_new_panel_st7701(dbi_io, &dev_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
    gpio_set_level(LCD_BK_LIGHT_PIN, 1);

    ESP_LOGI(TAG, "[LCD] Initialized");
}

void bsp_init_touch()
{
    ESP_LOGI(TAG, "[TOUCH] Initialize GT911");
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

    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &tp_handle));
    ESP_LOGI(TAG, "[TOUCH] GT911 initialized");
}


void lcd_draw_bitmap(int x, int y, int w, int h, uint8_t* data)
{
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, LCD_H_RES, LCD_V_RES, data));
}

void lcd_backlight(bool on)
{
    gpio_set_level(LCD_BK_LIGHT_PIN, on ? 1 : 0);
}



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

void lcd_register_event_callbacks()
{
    TaskHandle_t trans_done_task = xTaskGetCurrentTaskHandle();
    esp_lcd_dpi_panel_event_callbacks_t cbs = {
        .on_color_trans_done = notify_color_trans_done,
    };
    ESP_ERROR_CHECK(esp_lcd_dpi_panel_register_event_callbacks(panel_handle, &cbs, trans_done_task));    
}
