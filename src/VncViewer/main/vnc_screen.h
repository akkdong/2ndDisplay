// vnc_screen.h
//

#pragma once

#include "lvgl.h"
#include "extern.h"
#include "vnc_types.h"
#include "vnc_display.h"


#define VNC_MAX_LOGS        50  // 유지할 최대 로그 줄 수
#define VNC_CACHE_OBJECTS   1


BEGIN_EXTERN_C();


//
//
//

struct vnc_screen_s
{
    // display
    vnc_display_t* disp_handle;
    int disp_width;
    int disp_height;

    // application
    vnc_app_t* app;

    // screen components
    /*
    lv_obj_t* active_scrn;
    */
    lv_obj_t* layer_canvas;
    lv_obj_t* layer_main;
#if VNC_CACHE_OBJECTS
    lv_obj_t* obj_logs;
    lv_obj_t* obj_state;
    lv_obj_t* btn_connect;
    lv_obj_t* btn_disconnect;

    lv_timer_t* hide_timer;
#endif

    uint8_t* disp_buf;

    // command
    void (*create)(vnc_screen_t* scrn);
    void (*update_state)(vnc_screen_t* scrn, uint32_t state, uint32_t action);
    void (*append_log)(vnc_screen_t* scrn, const char* text);
    void (*printf_log)(vnc_screen_t* scrn, const char* format, ...);
    void (*empty_log)(vnc_screen_t* scrn);
};


enum event_command_e
{
    EVT_SHOW_WIFI,
    EVT_SHOW_CONN,
    EVT_HIDE_WIFI,
    EVT_HIDE_CONN
};



/**
 *
 */
vnc_screen_t* vnc_screen_init(vnc_app_t* app);

/**
 *
 */
void vnc_screen_create(vnc_screen_t* scrn);



/**
 *
 */
vnc_screen_t* vnc_screen_get_handle();



/**
 *
 */
bool vnc_screen_start_play(vnc_screen_t* scrn, int width, int height, int bpp);

void vnc_screen_stop_play(vnc_screen_t* scrn);

void vnc_screen_update_state(vnc_screen_t* scrn, uint32_t state, uint32_t action);

void vnc_screen_publish_frame(vnc_screen_t* scrn, uint8_t* buf, uint32_t size);


/**
 * 
 */
void vnc_screen_open_wifi_setting(vnc_screen_t* scrn);

void vnc_screen_open_connect_popup(vnc_screen_t* scrn);



/**
 *
 */
void vnc_log_append(vnc_screen_t* scrn, const char* text);

/**
 *
 */
void vnc_log_printf(vnc_screen_t* scrn, const char* format, ...);

/**
 *
 */
void vnc_log_clear(vnc_screen_t* scrn);




END_EXTERN_C();
