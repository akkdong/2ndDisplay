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



/*
vnc_app_t* vnc_app_get_instance(void);
bool vnc_app_send_event(vnc_app_t* app, app_event_t event, uint32_t data1, uint32_t data2, uint32_t data3);
void vnc_app_connect_server(vnc_app_t* app, const char* addr, uint16_t port, const char* pass);
void vnc_app_get_server(vnc_app_t* app, char* addr, uint16_t* port, char* pass);
*/
void vnc_app_set_state(vnc_app_t* app, app_state_t state, app_action_t action);



//
//
//

#if defined(_SIMULATOR)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>

#define VALID_IPNUM(x)      (((x) >= 0) && ((x) <= 255))
#define VALID_HEXDIGIT(x)  ((((x) >= '0') && ((x) <= '9')) || (((x) >= 'a') && ((x) <= 'f')) || (((x) >= 'A') && ((x) <= 'F')))

static uint8_t HEX2NUM(uint8_t c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return 0;
}

static bool parse_network_address(char* str, uint8_t* addr)
{
    char* t1 = strtok(str, ".");
    char* t2 = strtok(NULL, ".");
    char* t3 = strtok(NULL, ".");
    char* t4 = strtok(NULL, ".");

    if (t1 && t2 && t3 && t4)
    {
        int n1 = atoi(t1);
        int n2 = atoi(t2);
        int n3 = atoi(t3);
        int n4 = atoi(t4);

        if (VALID_IPNUM(n1) && VALID_IPNUM(n2) && VALID_IPNUM(n3) && VALID_IPNUM(n4))
        {
            addr[0] = n1;
            addr[1] = n2;
            addr[2] = n3;
            addr[3] = n4;

            return true;
        }
    }

    return false;
}

static bool parse_mac_address(char* str, uint8_t* addr)
{
    uint8_t mac[6];
    char* tok = strtok(str, "-");
    int i = 0;

    while (tok && i < sizeof(mac) / sizeof(mac[0]))
    {
        if (!VALID_HEXDIGIT(tok[0]) || !VALID_HEXDIGIT(tok[1]))
            break;

        uint8_t h = HEX2NUM(tok[0]);
        uint8_t l = HEX2NUM(tok[1]);
        mac[i] = (h << 4) | l;

        tok = strtok(NULL, "-");
        ++i;
    }

    if (tok == NULL && i == sizeof(mac) / sizeof(mac[0]))
    {
        memcpy(addr, mac, sizeof(mac));
        return true;
    }

    return false;
}


static bool parse_server_address(char* str, vnc_app_t* app)
{
    char* addr = strtok(str, "#");
    char* port = strtok(NULL, "#");

    if (addr)
    {
        char temp[24];
        uint8_t ip[4];
        strncpy(temp, addr, sizeof(temp) - 1);
        if (parse_network_address(temp, ip))
        {
            strncpy(app->server_addr, addr, sizeof(app->server_addr));
            app->server_port = (port != NULL ? (uint16_t)atoi(port) : 5900);
            return true;
        }
    }

    return false;
}

#endif



static nvs_init(vnc_app_t* app)
{
#if defined(_SIMULATOR)
    
    FILE* fp = fopen("vnc_client.cfg", "r");
    if (fp)
    {
        char sz[96];
        while (!feof(fp))
        {
            memset(sz, 0, sizeof(sz));
            fgets(sz, sizeof(sz), fp);

            char* tok1 = strtok(sz, "=\n");
            char* tok2 = strtok(NULL, "=\n");
            if (!tok1 || !tok2)
                continue;

            if (_stricmp(tok1, "ipaddr") == 0)
            {
                parse_network_address(tok2, app->net_ip);
            }
            else if (_stricmp(tok1, "gateway") == 0)
            {
                parse_network_address(tok2, app->net_gw);
            }
            else if (_stricmp(tok1, "netmask") == 0)
            {
                parse_network_address(tok2, app->net_mask);
            }
            else if (_stricmp(tok1, "dns") == 0)
            {
                parse_network_address(tok2, app->net_dns);
            }
            else if (_stricmp(tok1, "macaddr") == 0)
            {
                parse_mac_address(tok2, app->net_mac);
            }
            else if (_stricmp(tok1, "vnc_server") == 0)
            {
                parse_server_address(tok2, app);
            }
            else if (_stricmp(tok1, "vnc_password") == 0)
            {
                strncpy(app->server_pass, tok2, sizeof(app->server_pass) - 1);
            }
            else if (_stricmp(tok1, "net_if") == 0)
            {
                strncpy(app->net_if_name, tok2, sizeof(app->net_if_name) - 1);
            }
        }
        fclose(fp);
    }
#else
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
#endif
}

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
        NetworkInterface_t * pxInterface, const char* pActiveIfName);

    //
    const char* ifName = app->net_if_name[0] ? (const char*)&app->net_if_name[0] : NULL;
    pxWinPcap_FillInterfaceDescriptor(0, &(app->net_if[0]), ifName);

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
        vnc_app_set_state(app, app->state, APP_ACTION_NETIF_UP);
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
// ====================================================================
//

vnc_app_t vnc_app =
{
    .disp = NULL,
    .scrn = NULL,
    .client = NULL,

#if defined(_SIMULATOR)
    .net_ip = { 192, 168, 219, 100 },
    .net_mask = { 255, 255, 255, 0 },
    .net_gw = { 192, 168, 219, 1 },
    .net_dns = { 8, 8, 8, 8 },
    .net_mac = { 0x14, 0x11, 0x11, 0x11, 0x11, 0x41 },
    .net_if_name = { 0 },
#else
    .wifi_name = { 0 },
    .wifi_pass = { 0 },
#endif

    .server_addr = "", // { 0 },
    .server_port = 590,
    .server_pass = "", // { 0 },

    .state = APP_STATE_INIT,
    .action = APP_ACTION_NONE,

    .app_mux = NULL,
    .event_queue = NULL,

    //
    .send_event = vnc_app_send_event,
    .connect_server = vnc_app_connect_server,

    .get_server_info = vnc_app_get_server,
};




//
//
//

vnc_app_t* vnc_app_get_instance(void)
{
    return &vnc_app;
}


bool vnc_app_send_event(vnc_app_t* app, app_event_t id, uint32_t data1, uint32_t data2, uint32_t data3)
{
    if (vnc_app.event_queue)
    {
        printf("[APP] PostEvent(%d, %u, %u, %u)\n", id, data1, data2, data3);
        AppEventMsg_t msg = {
            .id = id,
            .data1 = data1,
            .data2 = data2,
            .data3 = data3,
        };

        if (xQueueSend(vnc_app.event_queue, &msg, 0) == pdTRUE)
            return true;
    }

    return false;
}


void vnc_app_connect_server(vnc_app_t* app, const char* addr, uint16_t port, const char* pass)
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
    vnc_app_send_event(app, VNC_CONNECT_TO_SERVER, 0, 0, 0);
}

void vnc_app_get_server(vnc_app_t* app, char* addr, uint16_t* port, char* pass)
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


void vnc_app_set_state(vnc_app_t* app, app_state_t state, app_action_t action)
{
    app->state = state;
    app->action = action;

    vnc_screen_update_state(app->scrn, app->state, app->action);
}


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
    app->scrn->update_state(app->scrn, app->state, app->action); // vnc_screen_update_state(app->scrn, app->state, app->action);
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
            vnc_log_printf(app->scrn, "[APP] RecvEvent(%d, %d, %d, %d)\n", msg.id, (int)msg.data1, (int)msg.data2, (int)msg.data3);
            printf("[APP] RecvEvent(%d, %d, %d, %d)\n", msg.id, (int)msg.data1, (int)msg.data2, (int)msg.data3);

            switch (msg.id)
            {
            case NETWORK_CONNECTED:
                FreeRTOS_inet_ntoa(msg.data1, buf);
                vnc_log_printf(app->scrn, "[APP] IP = % s\n", buf);
                vnc_app_set_state(app, APP_STATE_READY, APP_ACTION_NONE);
                break;
            case NETWORK_DISCONNECTED:
                vnc_log_printf(app->scrn, "[APP] Network Disconnected!\n");
                vnc_app_set_state(app, APP_STATE_STANDBY, APP_ACTION_NONE);
                break;
            case VNC_CONNECT_TO_SERVER:
                if (app->state == APP_STATE_READY && app->action == APP_ACTION_NONE)
                {
                    app->client = vnc_client_start(app);
                    if (!app->client)
                        vnc_log_printf(app->scrn, "[APP] Failed to start client\n", buf);
                }
                else
                {
                    vnc_log_printf(app->scrn, "[APP] You can't connect to server\n", buf);
                }
                break;
            case VNC_SERVER_CONNECTED:
                if (msg.data1 == 0)
                    vnc_log_printf(app->scrn, "[VNC] Server connected: %s\n", app->server_addr);
                break;
            case VNC_HANDSHAKE_FINISHED:
                if (msg.data1 != -1)
                {
                    vnc_log_printf(app->scrn, "[VNC] Negotiated: %dx%d %dbps\n", (int)msg.data1, (int)msg.data2, (int)msg.data3 * 8);
                    vnc_log_append(app->scrn, "[VNC] Start Play\n");
                }
                else
                {
                    vnc_log_append(app->scrn, "[VNC] Failed negotiation\n");
                }
                break;
            case VNC_SERVER_DISCONNECTED:
                if (msg.data1 != -1)
                {
                    vnc_log_append(app->scrn, "[VNC] Stop Play\n");
                }
                else
                {
                    vnc_log_append(app->scrn, "[VNC] Failed to connect to server\n");
                }
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
    // Initialize NVS
    nvs_init(&vnc_app);


    //
    vnc_app.disp = vnc_display_start();
    vnc_app.scrn = vnc_screen_init(&vnc_app);
    /*
    vnc_app.client = NUll;
    */

    // initialize net-state & acquire default settings
    net_init(&vnc_app);

    //
    vnc_app.state = APP_STATE_STANDBY;
    vnc_app.action = APP_ACTION_NONE;

    vnc_app.app_mux = xSemaphoreCreateMutex();
    vnc_app.event_queue = xQueueCreate(5, sizeof(AppEventMsg_t));

    /*
    vnc_app.scrn->create(vnc_app.scrn);
    */

    // start main-task
    xTaskCreate(app_task, "main", 4 * 1024, &vnc_app, tskIDLE_PRIORITY + 2, NULL);
}
