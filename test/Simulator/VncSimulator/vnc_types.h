// vnc_types.h
//

#pragma once

#include <stdlib.h>
#include <stdint.h>


#if defined(_WIN32)
#pragma pack( push, 1 )
#endif

struct PixelFormat_s {
    uint8_t bpp;
    uint8_t depth;
    uint8_t big_endian;
    uint8_t true_color;
    uint16_t red_max;
    uint16_t green_max;
    uint16_t blue_max;
    uint8_t red_shift;
    uint8_t green_shift;
    uint8_t blue_shift;
    uint8_t pad[3];
}
#if defined(_WIN32)
;
#pragma pack( pop )
#else
__attribute__((packed));
#endif

typedef struct PixelFormat_s PixelFormat;

typedef struct vnc_display_s vnc_display_t;
typedef struct vnc_screen_s vnc_screen_t;
typedef struct vnc_app_s vnc_app_t;

typedef struct vnc_client_s vnc_client_t;

typedef enum app_state_e app_state_t;
typedef enum app_action_e app_action_t;
typedef enum app_event_e app_event_t;

typedef uint16_t lcd_color_t;


#define heap_caps_malloc(x, y)		malloc(x)
#define heap_caps_free(ptr)			free(ptr)

//#define ESP_LOGI(TAG, fmt, ...)		printf(fmt, ##__VA_ARGS__)
