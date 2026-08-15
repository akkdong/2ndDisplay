// vnc_client.c
//

#include "vnc_client.h"
#include "app_main.h"

#include "FreeRTOS.h"
#if defined(_SIMULATOR)
#include "FreeRTOSIPConfig.h"
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"

#define socket          FreeRTOS_socket
#define bind            FreeRTOS_bind
#define setscokopt      FreeRTOS_setsockopt
#define closesocket     FreeRTOS_closesocket
#define sendto          FreeRTOS_sendto
#define recvfrom        FreeRTOS_recvfrom

#define connect         FreeRTOS_connect
#define listen          FreeRTOS_listen
#define accept          FreeRTOS_accept
#define send            FreeRTOS_send
#define recv            FreeRTOS_recv
#define shutdown        FreeRTOS_shutdown

#define inet_ntoa       FreeRTOS_inet_ntoa
#define inet_addr       FreeRTOS_inet_addr
#define inet_pton       FreeRTOS_inet_pton
#define inet_ntop       FreeRTOS_inet_ntop 

#define FD_SET          FreeRTOS_FD_SET
#define FD_CLR          FreeRTOS_FD_CLR
#define FD_ISSET        FreeRTOS_FD_ISSET

#endif



//
//
//

static vnc_client_t vnc_client;




//
//
//

static vnc_client_task(void* param)
{
    printf("[vnc_client] enter task.\n");
    vnc_client_t* client = (vnc_client_t*)param;

    uint32_t count = 0;
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(100));

        if (++count > 10)
            break;
    }

    printf("[vnc_client] leave task.\n");
    vTaskDelete(NULL);
}


//
//
//

vnc_client_t* vnc_client_start(vnc_app_t* app)
{
	vnc_client.app = app;


    BaseType_t result = xTaskCreate(vnc_client_task, "vnc_client", 1 * 1024, app, tskIDLE_PRIORITY + 4, NULL);
    if (result == pdTRUE)
    {
        /*
        app->scrn->update_state(app->scrn, 0, 0);
        app->scrn->append_log(app->scrn, "");
        app->scrn->printf_log(app->scrn, "");
        app->scrn->clear_log(app->scrn);
        */
        printf("[vnc_client] started.\n");
    }
    else
    {
        printf("[vnc_client] starting failed!\n");
    }

	return &vnc_client;
}
