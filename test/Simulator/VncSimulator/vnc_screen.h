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

    typedef struct vnc_screen_config vnc_screen_config_t;
    typedef struct vnc_screen vnc_screen_t;
    //typedef struct vnc_display vnc_display_t;



    struct vnc_screen_config
    {
        // callback
        void (*on_connect)(vnc_screen_t* scrn, const char* addr, uint16_t port);

        vnc_display_t* disp;
    };

    struct vnc_screen
    {
        vnc_display_t* disp_handle;
        int disp_width;
        int disp_height;

        lv_obj_t* active_scrn;
        lv_obj_t* layer_canvas;
        lv_obj_t* layer_main;
        lv_obj_t* layer_wifi;
        lv_obj_t* layer_conn;

        // command
        void (*create)(vnc_screen_t* scrn);
        void (*destroy)(vnc_screen_t* scrn);

        // callback
        void (*on_connect)(vnc_screen_t* scrn, const char* addr, uint16_t port);
    };


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
     //void vnc_screen_init(vnc_display_t* vnc_disp);
    vnc_screen_t* vnc_screen_init(vnc_screen_config_t* cfg);

    /**
     *
     */
    vnc_screen_t* vnc_screen_get_handle();


    /**
     *
     */
    void vnc_screen_create(vnc_screen_t* scrn);


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




#ifdef __cplusplus
}
#endif
