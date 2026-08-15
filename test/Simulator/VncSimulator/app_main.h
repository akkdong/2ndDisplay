// app_main.h
//

#pragma once

#include <stdbool.h>
#if defined(_SIMULATOR)
#include "FreeRTOS.h"
#include "FreeRTOSIPConfig.h"
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#endif

#include "vnc_display.h"
#include "vnc_screen.h"
#include "net_state.h"
#include "extern.h"


BEGIN_EXTERN_C()

//
typedef enum app_state
{
    APP_STATE_INIT,
    APP_STATE_STANDBY,    // initialzied, has no network
    APP_STATE_READY,      // has network capability
    APP_STATE_PLAY,       // vnc connected
} app_state_t;

typedef enum app_action
{
    APP_ACTION_NONE,
    APP_ACTION_NETIF_UP,
    APP_ACTION_WIFI_CONNECT,
    APP_ACTION_VNC_CONNECT,
    APP_ACTION_VNC_HANDSHAKE,
    APP_ACTION_VNC_CLOSE,
} app_action_t;

typedef enum app_event
{
    NETWORK_CONNECTED = 1000,
    NETWORK_DISCONNECTED,

    VNC_CONNECTED = 2000,
    VNC_DISCONNECTED,

} app_event_t;


//
typedef struct vnc_app vnc_app_t;

typedef struct vnc_app
{
    //
    vnc_display_t* disp;
    vnc_screen_t* scrn;

#if defined(_SIMULATOR)
    uint8_t net_ip[4];
    uint8_t net_mask[4];
    uint8_t net_gw[4];
    uint8_t net_dns[4];
    uint8_t net_mac[6];

    NetworkInterface_t net_if[1];
    NetworkEndPoint_t net_ep[4];
#else
    char wifi_name[32]; // ssid
    char wifi_pass[16]; // authentication password
#endif

    //
    app_state_t state;
    app_action_t action;

    QueueHandle_t event_queue;

} vnc_app_t;



//
//
//

/*

*/
void vnc_app_init();


/*

*/
vnc_app_t* vnc_app_instance(void);


/*

*/
bool vnc_app_event_send(app_event_t event, uint32_t data);




END_EXTERN_C()


