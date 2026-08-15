// vnc_screen.h
//

#pragma once

#include "vnc_display.h"
#include "lvgl/lvgl.h"

#define VNC_MAX_LOGS        50  // 유지할 최대 로그 줄 수
#define VNC_CACHE_OBJECTS   1


#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct vnc_screen vnc_screen_t;
    typedef struct vnc_app vnc_app_t;

    struct vnc_screen
    {
        vnc_display_t* disp_handle;
        int disp_width;
        int disp_height;

        /*
        lv_obj_t* active_scrn;
        */
        lv_obj_t* layer_canvas;
        lv_obj_t* layer_main;
#if VNC_CACHE_OBJECTS
        lv_obj_t* obj_logs;
        lv_obj_t* obj_state;
        lv_obj_t* btn_connect;
#endif

        // command
        void (*connect_server)(const char* ip, uint16_t port, const char* pass);
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
    vnc_screen_t* vnc_screen_init(vnc_display_t* disp);

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
    void vnc_update_state(vnc_screen_t* scrn, uint32_t state, uint32_t action);



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
