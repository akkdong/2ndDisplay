// app_main.c
//


#include "FreeRTOS.h"
#include "queue.h"
#include "timers.h"
#if defined(_SIMULATOR)
#include "FreeRTOSIPConfig.h"
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#endif

#include "app_main.h"
#include "vnc_client.h"





//
//
//

void net_init(vnc_app_t* app)
{
#if !defined(_SIMULATOR)
    // 네트워크 인터페이스 및 이벤트 루프 초기화
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &wifi_and_ip_event_handler,
        NULL,
        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
        ESP_EVENT_ANY_ID,
        &wifi_and_ip_event_handler,
        NULL,
        NULL));

    // ESP-Hosted 드라이버가 초기화된 후 생성된 netif를 바인딩합니다.
    esp_netif_t* sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    // Wi-Fi 초기화 (Hosted 모드 설정 반영)
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));


    // check wifi config
    wifi_config_t conf;
    // 현재 메모리/NVS에 설정된 STA 설정값 읽기
    if (esp_wifi_get_config(WIFI_IF_STA, &conf) == ESP_OK) {
        // SSID 길이가 0보다 크면 저장된 정보가 있는 것임
        if (strlen((char*)conf.sta.ssid) > 0) {
            return true;
        }
    }
#endif
}

static void task_netif_up(void* param)
{
    vnc_app_t* app = (vnc_app_t*)param;

    //
    extern NetworkInterface_t* pxWinPcap_FillInterfaceDescriptor(BaseType_t xEMACIndex,
        NetworkInterface_t * pxInterface);

    //
    pxWinPcap_FillInterfaceDescriptor(0, &(app->net_if[0]));

    //
    FreeRTOS_FillEndPoint(&(app->net_if[0]), &(app->net_ep[0]), app->net_ip, app->net_mask, app->net_gw, app->net_dns, app->net_mac);
#if ( ipconfigUSE_DHCP != 0 )
    {
        /* End-point 0 wants to use DHCPv4. */
        app->net_ep[0].bits.bWantDHCP = pdTRUE;
    }
#endif /* ( ipconfigUSE_DHCP != 0 ) */

    //
    BaseType_t xResult = FreeRTOS_IPInit_Multi();
    configASSERT(xResult == pdTRUE);

    if (xResult == pdTRUE)
    {
        app->action = APP_ACTION_NETIF_UP;
        /*
        app->timeout = 5000;
        */

        vnc_update_state(app->scrn, app->state, app->action);
    }
    else
    {
        //
        // ....
        //
    }

    //
    vTaskDelete(NULL);
}

void net_start(vnc_app_t* app)
{
#if defined(_SIMULATOR)
    xTaskCreate(task_netif_up, "netif_up", 1 * 1024, app, tskIDLE_PRIORITY + 2, NULL);
#else
#endif
}





//
//
//

static void vnc_app_connect_server(vnc_app_t* app, const char* addr, uint16_t port, const char* pass)
{
    printf("Connect To: %s#%u (%s)\n", addr, port, pass);
    // vnc_app_data_lock(app);
    {
        strncpy(app->server_addr, addr, sizeof(app->server_addr));
        app->server_port = port;
        strncpy(app->server_pass, pass, sizeof(app->server_pass));
    }
    // vnc_app_data_unlock(app);

    //
    /*
    app->client = vnc_client_start(app);
    if (app->client)
    {
    }
    else
    {

    }
    */
    vnc_app_event_send(VNC_CONNECT_SERVER, 0);
}

static void vnc_app_get_server(vnc_app_t* app, char* addr, uint16_t* port, char* pass)
{
    // vnc_app_data_lock(app);
    {
        if (addr)
            strncpy(addr, app->server_addr, sizeof(app->server_addr));
        if (port)
            *port = app->server_port;
        if (pass)
            strncpy(pass, app->server_pass, sizeof(app->server_pass));
    }
    // vnc_app_data_unlock(app);
}




//
//
//

vnc_app_t vnc_app =
{
    .disp = NULL,
    .scrn = NULL,
    .client = NULL,

#if defined(_SIMULATOR)
    .net_ip = { configIP_ADDR0, configIP_ADDR1, configIP_ADDR2, configIP_ADDR3 },
    .net_mask = { configNET_MASK0, configNET_MASK1, configNET_MASK2, configNET_MASK3 },
    .net_gw = { configGATEWAY_ADDR0, configGATEWAY_ADDR1, configGATEWAY_ADDR2, configGATEWAY_ADDR3 },
    .net_dns = { configDNS_SERVER_ADDR0, configDNS_SERVER_ADDR1, configDNS_SERVER_ADDR2, configDNS_SERVER_ADDR3 },
    .net_mac = { configMAC_ADDR0, configMAC_ADDR1, configMAC_ADDR2, configMAC_ADDR3, configMAC_ADDR4, configMAC_ADDR5 },
#else
    .wifi_name = { 0 },
    .wifi_pass = { 0 },
#endif

    .server_addr = "192.168.219.128", // { 0 },
    .server_port = 5800,
    .server_pass = "password", // { 0 },

    .state = APP_STATE_INIT,
    .action = APP_ACTION_NONE,

    .event_queue = NULL,

    //
    .connect_server = vnc_app_connect_server,
    .get_server = vnc_app_get_server,
};





//
//
//

static void net_start_delayed(TimerHandle_t timer)
{
    vnc_app_t* app = (vnc_app_t*)pvTimerGetTimerID(timer);
    net_start(app);
}

static void app_task(void* param)
{
    vnc_app_t* app = (vnc_app_t*)param;
    AppEventMsg_t msg;
    char buf[16];

    //
    vTaskDelay(pdMS_TO_TICKS(500));
    app->scrn->create(app->scrn); // vnc_screen_create(app->scrn);
    vTaskDelay(pdMS_TO_TICKS(100));
    app->scrn->update_state(app->scrn, app->state, app->action); // vnc_update_state(app->scrn, app->state, app->action);
    vTaskDelay(pdMS_TO_TICKS(100));

    // prepare network
    {
        TimerHandle_t timer = xTimerCreate("one-shot",
            pdMS_TO_TICKS(2000),
            pdFALSE,
            app,
            net_start_delayed);
        if (timer)
            xTimerStart(timer, 0);

        //net_start(app);
    }

    while (1)
    {
        if (xQueueReceive(app->event_queue, &msg, 0) == pdTRUE)
        {
            printf("[APP] pre-RecvEvent\n");
            vnc_log_printf(app->scrn, "[APP] RecvEvent(%d, %u)\n", msg.id, msg.data);
            printf("[APP] RecvEvent(%d, %u)\n", msg.id, msg.data);

            switch (msg.id)
            {
            case NETWORK_CONNECTED:
                FreeRTOS_inet_ntoa(msg.data, buf);
                vnc_log_printf(app->scrn, "[APP] IP = % s\n", buf);

                app->state = APP_STATE_READY;
                app->action = APP_ACTION_NONE;
                vnc_update_state(app->scrn, app->state, app->action);
                break;
            case NETWORK_DISCONNECTED:
                break;
            case VNC_CONNECT_SERVER:
                app->client = vnc_client_start(app);
                if (app->client)
                {
                    app->action = APP_ACTION_VNC_CONNECT;
                    vnc_update_state(app->scrn, app->state, app->action);
                }
                else
                {

                }
                break;
            case VNC_CONNECTED:
                break;
            case VNC_DISCONNECTED:
                break;
            }
        }


        //
        vTaskDelay(pdMS_TO_TICKS(30));
    }

    vTaskDelete(NULL);
}



//
//
//

void vnc_app_init()
{
    // initialize net-state & acquire default settings
    net_init(&vnc_app);

    //
    vnc_app.disp = vnc_display_start();

    //
    vnc_app.scrn = vnc_screen_init(&vnc_app);

    /*
    vnc_app.scrn->connect_server = vnc_connect_server;
    vnc_app.scrn->xxx = xxx;
    */

    //
    vnc_app.state = APP_STATE_STANDBY;
    vnc_app.action = APP_ACTION_NONE;
    vnc_app.event_queue = xQueueCreate(10, sizeof(AppEventMsg_t));

    /*
    vnc_app.scrn->create(vnc_app.scrn);
    */

    // start main-task
    xTaskCreate(app_task, "main", 4 * 1024, &vnc_app, tskIDLE_PRIORITY + 2, NULL);
}


vnc_app_t* vnc_app_instance(void)
{
    return &vnc_app;
}


bool vnc_app_event_send(app_event_t id, uint32_t data)
{
    if (vnc_app.event_queue)
    {
        printf("[APP] PostEvent(%d, %u)\n", id, data);
        AppEventMsg_t msg = {
            .id = id,
            .data = data,
        };

        if (xQueueSend(vnc_app.event_queue, &msg, 0) == pdTRUE)
            return true;
    }

    return false;
}
