// bsp.c
//

#include "bsp.h"

#include "sdkconfig.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_ldo_regulator.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_st7701.h"
#include "esp_lcd_touch_gt911.h"

#define CONFIG_BSP_LCD_TYPE_480_800 1

#define LCD_LEDC_CH             CONFIG_BSP_DISPLAY_BRIGHTNESS_LEDC_CH
#define BSP_I2C_NUM             0 // CONFIG_BSP_I2C_NUM
#define USE_LEDC_BRIGHTNESS     1

#ifdef __cplusplus
extern "C" {
#endif

/* Assert on error, if selected in menuconfig. Otherwise return error code. */
#if CONFIG_BSP_ERROR_CHECK
#define BSP_ERROR_CHECK_RETURN_ERR(x)    ESP_ERROR_CHECK(x)
#define BSP_ERROR_CHECK_RETURN_NULL(x)   ESP_ERROR_CHECK(x)
#define BSP_ERROR_CHECK(x, ret)          ESP_ERROR_CHECK(x)
#define BSP_NULL_CHECK(x, ret)           assert(x)
#define BSP_NULL_CHECK_GOTO(x, goto_tag) assert(x)
#else
#define BSP_ERROR_CHECK_RETURN_ERR(x) do { \
        esp_err_t err_rc_ = (x);            \
        if (unlikely(err_rc_ != ESP_OK)) {  \
            return err_rc_;                 \
        }                                   \
    } while(0)

#define BSP_ERROR_CHECK_RETURN_NULL(x)  do { \
        if (unlikely((x) != ESP_OK)) {      \
            return NULL;                    \
        }                                   \
    } while(0)

#define BSP_NULL_CHECK(x, ret) do { \
        if ((x) == NULL) {          \
            return ret;             \
        }                           \
    } while(0)

#define BSP_ERROR_CHECK(x, ret)      do { \
        if (unlikely((x) != ESP_OK)) {    \
            return ret;                   \
        }                                 \
    } while(0)

#define BSP_NULL_CHECK_GOTO(x, goto_tag) do { \
        if ((x) == NULL) {      \
            goto goto_tag;      \
        }                       \
    } while(0)
#endif

#ifdef __cplusplus
}
#endif


static const char* TAG = "BSP";

static const st7701_lcd_init_cmd_t lcd_init_cmds[] = 
{
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

    //
    // 180도 회전 또는 반전이 필요할 때
    //
    //{0x36, (uint8_t []){0x00}, 1, 0}, // 기본값 (0도)
    // {0x36, (uint8_t []){0x80}, 1, 0}, // Y-Mirror (상하 반전)
    // {0x36, (uint8_t []){0x40}, 1, 0}, // X-Mirror (좌우 반전)
    // {0x36, (uint8_t []){0xC0}, 1, 0}, // X, Y 둘 다 반전 (180도 회전)
};


static lv_indev_t *disp_indev = NULL;

static bool i2c_initialized = false;
static i2c_master_bus_handle_t i2c_handle = NULL;  // I2C Handle

static bsp_lcd_handles_t disp_handles;
static esp_ldo_channel_handle_t disp_phy_pwr_chan = NULL;
static esp_lcd_touch_handle_t tp = NULL;
static esp_lcd_panel_io_handle_t tp_io_handle = NULL;



//
//
//

static esp_err_t bsp_enable_dsi_phy_power(void)
{
#if BSP_MIPI_DSI_PHY_PWR_LDO_CHAN > 0
    // Turn on the power for MIPI DSI PHY, so it can go from "No Power" state to "Shutdown" state
    esp_ldo_channel_config_t ldo_cfg = {
        .chan_id = BSP_MIPI_DSI_PHY_PWR_LDO_CHAN,
        .voltage_mv = BSP_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV,
    };
    ESP_RETURN_ON_ERROR(esp_ldo_acquire_channel(&ldo_cfg, &disp_phy_pwr_chan), TAG, "Acquire LDO channel for DPHY failed");
    ESP_LOGI(TAG, "MIPI DSI PHY Powered on");
#endif // BSP_MIPI_DSI_PHY_PWR_LDO_CHAN > 0

    return ESP_OK;
}

static lv_display_t *bsp_display_lcd_init(const bsp_display_cfg_t *cfg)
{
    assert(cfg != NULL);
    BSP_ERROR_CHECK_RETURN_NULL(bsp_display_new_with_handles(&cfg->hw_cfg, &disp_handles));

    uint32_t display_hres = CONFIG_BSP_LCD_WIDTH;
    uint32_t display_vres = CONFIG_BSP_LCD_HEIGHT;

    ESP_LOGI(TAG, "Display resolution %ldx%ld", display_hres, display_vres);

    /* Add LCD screen */
    ESP_LOGD(TAG, "Add LCD screen");
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = disp_handles.io,
        .panel_handle = disp_handles.panel,
        .control_handle = disp_handles.control,
        .buffer_size = cfg->buffer_size,
        .double_buffer = cfg->double_buffer,
        .hres = display_hres,
        .vres = display_vres,
        .monochrome = false,
        /* Rotation values must be same as used in esp_lcd for initial settings of the screen */
        .rotation = {
            .swap_xy = false,
            .mirror_x = true,
            .mirror_y = true,
        },
#if LVGL_VERSION_MAJOR >= 9
#if CONFIG_BSP_LCD_COLOR_FORMAT_RGB888
        .color_format = LV_COLOR_FORMAT_RGB888,
#else
        .color_format = LV_COLOR_FORMAT_RGB565,
#endif
#endif
        .flags = {
            .buff_dma = cfg->flags.buff_dma,
            .buff_spiram = cfg->flags.buff_spiram,
#if LVGL_VERSION_MAJOR >= 9
            .swap_bytes = false, // (BSP_LCD_BIGENDIAN ? true : false),
#endif
#if CONFIG_BSP_DISPLAY_LVGL_AVOID_TEAR
            .sw_rotate = false,                /* Avoid tearing is not supported for SW rotation */
#else
            .sw_rotate = cfg->flags.sw_rotate, /* Only SW rotation is supported for 90° and 270° */
#endif
#if CONFIG_BSP_DISPLAY_LVGL_FULL_REFRESH
            .full_refresh = true,
#elif CONFIG_BSP_DISPLAY_LVGL_DIRECT_MODE
            .direct_mode = true,
#endif
        }
    };

    const lvgl_port_display_dsi_cfg_t dpi_cfg = {
        .flags = {
#if CONFIG_BSP_DISPLAY_LVGL_AVOID_TEAR
            .avoid_tearing = true,
#else
            .avoid_tearing = false,
#endif
        }
    };

    return lvgl_port_add_disp_dsi(&disp_cfg, &dpi_cfg);
}

static lv_indev_t *bsp_display_indev_init(lv_display_t *disp)
{
    BSP_ERROR_CHECK_RETURN_NULL(bsp_touch_new(NULL, &tp));
    assert(tp);

    /* Add touch input (for selected screen) */
    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = disp,
        .handle = tp,
    };

    return lvgl_port_add_touch(&touch_cfg);
}



//
//
//

esp_err_t bsp_display_new(const bsp_display_config_t *config, esp_lcd_panel_handle_t *ret_panel,
                          esp_lcd_panel_io_handle_t *ret_io)
{
    esp_err_t ret = ESP_OK;
    bsp_lcd_handles_t handles;
    ret = bsp_display_new_with_handles(config, &handles);

    *ret_panel = handles.panel;
    *ret_io = handles.io;

    return ret;
}

esp_err_t bsp_display_new_with_handles(const bsp_display_config_t *config, bsp_lcd_handles_t *ret_handles)
{
    esp_err_t ret = ESP_OK;
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_handle_t disp_panel = NULL;

    ESP_LOGI(TAG, "[0] dsi_bus.lane_bit_rate_mbps = %d", config->dsi_bus.lane_bit_rate_mbps);
    ESP_RETURN_ON_ERROR(bsp_display_brightness_init(), TAG, "Brightness init failed");
    ESP_RETURN_ON_ERROR(bsp_enable_dsi_phy_power(), TAG, "DSI PHY power failed");

    /* create MIPI DSI bus first, it will initialize the DSI PHY as well */
    esp_lcd_dsi_bus_handle_t mipi_dsi_bus = NULL;
    esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id = 0,
        .num_data_lanes = CONFIG_BSP_MIPI_DSI_LANE_NUM,
        .phy_clk_src = 0, // config->dsi_bus.phy_clk_src,
        .lane_bit_rate_mbps = CONFIG_BSP_MIPI_DSI_LANE_RATE, // config->dsi_bus.lane_bit_rate_mbps,
    };

    ESP_RETURN_ON_ERROR(esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus), TAG, "New DSI bus init failed");

    ESP_LOGI(TAG, "Install MIPI DSI LCD control panel");
    // we use DBI interface to send LCD commands and parameters
    esp_lcd_dbi_io_config_t dbi_config = {
        .virtual_channel = 0,
        .lcd_cmd_bits = 8,   // according to the LCD spec
        .lcd_param_bits = 8, // according to the LCD spec
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &io), err, TAG, "New panel IO failed");

    // create ST7701 control panel
    ESP_LOGI(TAG, "Install ST7701 LCD control panel");

    esp_lcd_dpi_panel_config_t dpi_config = {
        .virtual_channel = 0,
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz = CONFIG_BSP_LCD_DPI_CLK,
        .in_color_format = LCD_COLOR_FMT_RGB565, // CONFIG_BSP_LCD_COLOR_FORMAT_RGB565 ? LCD_COLOR_FMT_RGB565 : LCD_COLOR_FMT_RGB888
        .video_timing = {
            .h_size = CONFIG_BSP_LCD_WIDTH,
            .v_size = CONFIG_BSP_LCD_HEIGHT,
            .hsync_pulse_width = CONFIG_BSP_LCD_HSYNC,
            .hsync_back_porch = CONFIG_BSP_LCD_HBP,
            .hsync_front_porch = CONFIG_BSP_LCD_HFP,
            .vsync_pulse_width = CONFIG_BSP_LCD_VSYNC,
            .vsync_back_porch = CONFIG_BSP_LCD_VBP,
            .vsync_front_porch = CONFIG_BSP_LCD_VFP,
        },
    };
    st7701_vendor_config_t vendor_config = {
        .init_cmds = lcd_init_cmds,
        .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
        .flags.use_mipi_interface = 1,
        .mipi_config = {
            .dsi_bus = mipi_dsi_bus,
            .dpi_config = &dpi_config,
        },
    };
    esp_lcd_panel_dev_config_t dev_config = {
        .reset_gpio_num = CONFIG_BSP_LCD_RST_PIN,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = &vendor_config,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_st7701(io, &dev_config, &disp_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(disp_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(disp_panel));


    /* Return all handles */
    ret_handles->io = io;
    disp_handles.io = io;
    ret_handles->mipi_dsi_bus = mipi_dsi_bus;
    disp_handles.mipi_dsi_bus = mipi_dsi_bus;
    ret_handles->panel = disp_panel;
    disp_handles.panel = disp_panel;
    ret_handles->control = NULL;
    disp_handles.control = NULL;

    ESP_LOGI(TAG, "Display initialized");

    return ret;

err:
    bsp_display_delete();
    return ret;
}

void bsp_display_delete(void)
{
    if (disp_handles.panel) {
        esp_lcd_panel_del(disp_handles.panel);
        disp_handles.panel = NULL;
    }

    if (disp_handles.io) {
        esp_lcd_panel_io_del(disp_handles.io);
        disp_handles.io = NULL;
    }

    if (disp_handles.mipi_dsi_bus) {
        esp_lcd_del_dsi_bus(disp_handles.mipi_dsi_bus);
        disp_handles.mipi_dsi_bus = NULL;
    }

    if (disp_phy_pwr_chan) {
        esp_ldo_release_channel(disp_phy_pwr_chan);
        disp_phy_pwr_chan = NULL;
    }

    bsp_display_brightness_deinit();
}



//
//
//

esp_err_t bsp_display_brightness_init(void)
{
#if USE_LEDC_BRIGHTNESS
    // Setup LEDC peripheral for PWM backlight control
    const ledc_channel_config_t LCD_backlight_channel = {
        .gpio_num = CONFIG_BSP_LCD_BKLIGHT_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LCD_LEDC_CH,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = 1,
        .duty = 0,
        .hpoint = 0
    };
    const ledc_timer_config_t LCD_backlight_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = 1,
        .freq_hz = 5000,
        .clk_cfg = LEDC_USE_XTAL_CLK //LEDC_AUTO_CLK
    };

    BSP_ERROR_CHECK_RETURN_ERR(ledc_timer_config(&LCD_backlight_timer));
    gpio_reset_pin(LCD_backlight_channel.gpio_num);
    BSP_ERROR_CHECK_RETURN_ERR(ledc_channel_config(&LCD_backlight_channel));
#else
    ESP_LOGI(TAG, "[LCD] Init backlight pin");
    gpio_config_t bk_io_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << CONFIG_BSP_LCD_BKLIGHT_PIN,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&bk_io_config));
    gpio_set_level(CONFIG_BSP_LCD_BKLIGHT_PIN, 0);
#endif

    return ESP_OK;
}

esp_err_t bsp_display_brightness_deinit(void)
{
#if USE_LEDC_BRIGHTNESS
    const ledc_timer_config_t LCD_backlight_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = 1,
        .deconfigure = 1
    };
    BSP_ERROR_CHECK_RETURN_ERR(ledc_timer_pause(LEDC_LOW_SPEED_MODE, 1));
    BSP_ERROR_CHECK_RETURN_ERR(ledc_timer_config(&LCD_backlight_timer));
#endif

    return ESP_OK;
}

esp_err_t bsp_display_brightness_set(int brightness_percent)
{
#if USE_LEDC_BRIGHTNESS
    if (brightness_percent > 100) {
        brightness_percent = 100;
    }
    if (brightness_percent < 0) {
        brightness_percent = 0;
    }

    ESP_LOGI(TAG, "Setting LCD backlight: %d%%", brightness_percent);
    uint32_t duty_cycle = (1023 * brightness_percent) / 100; // LEDC resolution set to 10bits, thus: 100% = 1023
    BSP_ERROR_CHECK_RETURN_ERR(ledc_set_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH, duty_cycle));
    BSP_ERROR_CHECK_RETURN_ERR(ledc_update_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH));
#else
    if (brightness_percent > 0)
        gpio_set_level(BSP_LCD_BACKLIGHT, 1);
    else
        gpio_set_level(BSP_LCD_BACKLIGHT, 0);
#endif

    return ESP_OK;
}

esp_err_t bsp_display_backlight_off(void)
{
    return bsp_display_brightness_set(0);
}

esp_err_t bsp_display_backlight_on(void)
{
    return bsp_display_brightness_set(100);
}
 


//
//
//

esp_err_t bsp_touch_new(const bsp_touch_config_t *config, esp_lcd_touch_handle_t *ret_touch)
{
    /* Initilize I2C */
    BSP_ERROR_CHECK_RETURN_ERR(bsp_i2c_init());

    /* Initialize touch */
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();

    esp_lcd_touch_io_gt911_config_t tp_gt911_config = {
        .dev_addr = tp_io_config.dev_addr,
    };

    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = CONFIG_BSP_LCD_WIDTH, // BSP_LCD_H_RES,
        .y_max = CONFIG_BSP_LCD_HEIGHT, // BSP_LCD_V_RES,
        .rst_gpio_num = CONFIG_BSP_TOUCH_RST, // BSP_LCD_TOUCH_RST,
        .int_gpio_num = CONFIG_BSP_TOUCH_INT, // BSP_LCD_TOUCH_INT,
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

    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(i2c_handle, &tp_io_config, &tp_io_handle), TAG, "");

    return esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, ret_touch);
}

void bsp_touch_delete(void)
{
    if (tp) {
        esp_lcd_touch_del(tp);
    }
    if (tp_io_handle) {
        esp_lcd_panel_io_del(tp_io_handle);
        tp_io_handle = NULL;
    }
}



//
//
//
 
esp_err_t bsp_i2c_init(void)
{
    /* I2C was initialized before */
    if (i2c_initialized) {
        return ESP_OK;
    }

    i2c_master_bus_config_t i2c_bus_conf = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .sda_io_num = CONFIG_BSP_TOUCH_I2C_SDA,
        .scl_io_num = CONFIG_BSP_TOUCH_I2C_SCL,
        .i2c_port = BSP_I2C_NUM,
    };
    BSP_ERROR_CHECK_RETURN_ERR(i2c_new_master_bus(&i2c_bus_conf, &i2c_handle));

    i2c_initialized = true;

    return ESP_OK;
}

esp_err_t bsp_i2c_deinit(void)
{
    if (i2c_initialized && i2c_handle) {
        BSP_ERROR_CHECK_RETURN_ERR(i2c_del_master_bus(i2c_handle));
        i2c_initialized = false;
    }
    return ESP_OK;
}

i2c_master_bus_handle_t bsp_i2c_get_handle(void)
{
    return i2c_handle;
}





//
//
//

lv_display_t *bsp_display_start(void)
{
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_DRAW_BUFF_SIZE,
        .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
        .hw_cfg = {
            .dsi_bus = {
                .phy_clk_src = 0, // let the driver to choose the default clock source
                .lane_bit_rate_mbps = CONFIG_BSP_MIPI_DSI_LANE_RATE,
            }
        },
        .flags = {
#if CONFIG_BSP_LCD_COLOR_FORMAT_RGB888
            .buff_dma = false,
#else
            .buff_dma = true,
#endif
            .buff_spiram = false,
            .sw_rotate = true,
        }
    };

    ESP_LOGI(TAG, "dsi_bus.lane_bit_rate_mbps = %d", cfg.hw_cfg.dsi_bus.lane_bit_rate_mbps);
    return bsp_display_start_with_config(&cfg);
}

lv_display_t *bsp_display_start_with_config(const bsp_display_cfg_t *cfg)
{
    lv_display_t *disp;

    assert(cfg != NULL);
    ESP_ERROR_CHECK(bsp_display_brightness_init());
    BSP_ERROR_CHECK_RETURN_NULL(lvgl_port_init(&cfg->lvgl_port_cfg));
    BSP_NULL_CHECK(disp = bsp_display_lcd_init(cfg), NULL);
#if !CONFIG_BSP_LCD_TYPE_HDMI
    BSP_NULL_CHECK(disp_indev = bsp_display_indev_init(disp), NULL);
#endif
    return disp;
}

void bsp_display_stop(lv_display_t *display)
{
    /* Deinit LVGL */
#if !CONFIG_BSP_LCD_TYPE_HDMI
    lvgl_port_remove_touch(disp_indev);
#endif
    lvgl_port_remove_disp(display);
    lvgl_port_deinit();

#if !CONFIG_BSP_LCD_TYPE_HDMI
    /* Deinit touch */
    bsp_touch_delete();
#endif

    /* Deinit display */
    bsp_display_delete();

    /* Deinit I2C if initialized */
    bsp_i2c_deinit();
}

lv_indev_t *bsp_display_get_input_dev(void)
{
    return disp_indev;
}

void bsp_display_rotate(lv_display_t *disp, lv_disp_rotation_t rotation)
{
    lv_disp_set_rotation(disp, rotation);
}

bool bsp_display_lock(uint32_t timeout_ms)
{
    return lvgl_port_lock(timeout_ms);
}

void bsp_display_unlock(void)
{
    lvgl_port_unlock();
}
