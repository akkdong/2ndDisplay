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
#include "vnc_types.h"


BEGIN_EXTERN_C();


//
//
//

enum app_state_e
{
    APP_STATE_INIT,
    APP_STATE_STANDBY,    // initialzied, has no network
    APP_STATE_READY,      // has network capability
    APP_STATE_PLAY,       // vnc connected
};

enum app_action_e
{
    APP_ACTION_NONE,
    APP_ACTION_NETIF_UP,
    APP_ACTION_WIFI_CONNECT,
    APP_ACTION_VNC_CONNECT,
    APP_ACTION_VNC_HANDSHAKE,
    APP_ACTION_VNC_CLOSE,
};

enum app_event_e
{
    NETWORK_CONNECTED = 1000,
    NETWORK_DISCONNECTED,

    VNC_CONNECT_TO_SERVER = 2000,
    VNC_SERVER_CONNECTED,           // data1 == -1 on fail
    VNC_HANDSHAKE_FINISHED,         // data1 == -1 on fail
    VNC_SERVER_DISCONNECTED,


};



//
//
//

typedef struct AppEventMsg
{
    app_event_t id;

    uint32_t data1;
    uint32_t data2;
    uint32_t data3;
} AppEventMsg_t;



//
//

struct vnc_app_s
{
    //
    vnc_display_t* disp;
    vnc_screen_t* scrn;
    vnc_client_t* client;

#if defined(_SIMULATOR)
    uint8_t net_ip[4];
    uint8_t net_mask[4];
    uint8_t net_gw[4];
    uint8_t net_dns[4];
    uint8_t net_mac[6];
    char net_if_name[96];

    NetworkInterface_t net_if[1];
    NetworkEndPoint_t net_ep[4];
#else
    char wifi_name[32]; // ssid
    char wifi_pass[16]; // authentication password
#endif

    char server_addr[16]; // xxx.xxx.xxx.xxx
    uint16_t server_port;
    char server_pass[32];

    //
    app_state_t state;
    app_action_t action;

    SemaphoreHandle_t app_mux;
    QueueHandle_t event_queue;

    //
    void (*send_event)(vnc_app_t* app, app_event_t event, uint32_t data1, uint32_t data2, uint32_t data3);
    void (*connect_server)(vnc_app_t* app, const char* ip, uint16_t port, const char* pass);

    void (*get_server_info)(vnc_app_t* app, char* ip, uint16_t* port, char* pass);

};



//
//
//

/*
 *
 */
void vnc_app_init();


/*
 *
 */
vnc_app_t* vnc_app_get_instance(void);


bool vnc_app_send_event(vnc_app_t* app, app_event_t event, uint32_t data1, uint32_t data2, uint32_t data3);

void vnc_app_connect_server(vnc_app_t* app, const char* addr, uint16_t port, const char* pass);

void vnc_app_get_server(vnc_app_t* app, char* addr, uint16_t* port, char* pass);

/*
void vnc_app_set_state(vnc_app_t* app, app_state_t state, app_action_t action);
*/

END_EXTERN_C();
