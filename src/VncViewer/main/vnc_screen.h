// vnc_screen.h
//

#pragma once

#include "vnc_display.h"
#include "lvgl.h"

#define MAX_LOG_LINES 50  // 유지할 최대 로그 줄 수



#ifdef __cplusplus
extern "C"
{
#endif

typedef struct vnc_screen
{
    lv_obj_t* active_scrn;
    int disp_width;
    int disp_height;

    lv_obj_t* layer_canvas;
    lv_obj_t* layer_main;
    lv_obj_t* layer_wifi;
    lv_obj_t* layer_conn;

    
} vnc_screen_t;


enum event_command
{
    EVT_SHOW_WIFI,
    EVT_SHOW_CONN,
    EVT_HIDE_WIFI,
    EVT_HIDE_CONN
};



/**
 * 
 */
void vnc_screen_init(vnc_display_t* vnc_disp);


/**
  * 
  */
void vnc_log_append(const char* text);

/**
 * 
 */
void vnc_log_printf(const char* format, ...);

/**
 * 
 */
void vnc_log_empty();




#ifdef __cplusplus
}
#endif
