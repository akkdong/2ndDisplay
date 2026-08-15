// vnc_types.h
//

#pragma once

#include <stdlib.h>
#include <stdint.h>


typedef struct vnc_display_s vnc_display_t;
typedef struct vnc_screen_s vnc_screen_t;
typedef struct vnc_app_s vnc_app_t;

typedef struct vnc_client_s vnc_client_t;

typedef enum app_state_e app_state_t;
typedef enum app_action_e app_action_t;
typedef enum app_event_e app_event_t;

typedef uint16_t lcd_color_t;
