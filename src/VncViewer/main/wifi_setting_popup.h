// wifi_setting_popup.h
//

#pragma once

#include <stdint.h>
#include "vnc_screen.h"


#ifdef __cplusplus
extern "C"
{
#endif

typedef struct vnc_wifi_popup_s vnc_wifi_popup_t;

typedef void (*on_wifi_connect_cb)(vnc_screen_t* scrn, const char* ssid);
typedef void (*show_wifi_popup_cb)(vnc_wifi_popup_t* popup);


struct vnc_wifi_popup_s
{
    //
    char ssid[32];

    //
    vnc_screen_t* scrn;

    on_wifi_connect_cb on_connect;
    show_wifi_popup_cb show_popup;
};




/**
 * 
 */
vnc_wifi_popup_t* vnc_wifi_popup_init(vnc_screen_t* scrn, on_wifi_connect_cb callback);


#ifdef __cplusplus
}
#endif
