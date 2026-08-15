// vnc_connect_popup.h
//

#pragma once

#include <stdint.h>
#include "extern.h"
#include "vnc_types.h"


BEGIN_EXTERN_C();



/**
 *
 *
 */

typedef struct vnc_connect_popup_s vnc_connect_popup_t;

typedef void (*on_connect_cb)(vnc_screen_t* scrn, const char* addr, uint16_t port, const char* pass);
typedef void (*show_popup_cb)(vnc_connect_popup_t* popup);


struct vnc_connect_popup_s
{
    //
    char address[16];
    uint16_t port;
    char password[32];

    //
    lv_obj_t* ta_address;
    lv_obj_t* ta_port;
    lv_obj_t* ta_password;
    lv_obj_t* kb;

    //
    vnc_screen_t* scrn;

    on_connect_cb on_connect;
    show_popup_cb show_popup;

};



vnc_connect_popup_t* vnc_connect_popup_init(vnc_screen_t* scrn, on_connect_cb callback);

/*
void vnc_connect_popup_show(vnc_connect_popup_t* popup, const char* addr, uint16_t port, on_connect_cb callback);
void show_vnc_connect_popup(const char* addr, uint16_t port, on_connect_cb callback);
*/


END_EXTERN_C();
