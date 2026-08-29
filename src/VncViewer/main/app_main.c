// app_main.c
//

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"

#include "nvs_flash.h"
#include "app_main.h"
#include "vnc_client.h"


static const char* TAG = "MAIN";


/*
vnc_app_t* vnc_app_get_instance(void);
bool vnc_app_send_event(vnc_app_t* app, app_event_t event, uint32_t data1, uint32_t data2, uint32_t data3);
void vnc_app_connect_server(vnc_app_t* app, const char* addr, uint16_t port, const char* pass);
void vnc_app_get_server(vnc_app_t* app, char* addr, uint16_t* port, char* pass);

void vnc_app_set_state(vnc_app_t* app, app_state_t state, app_action_t action);
*/


vnc_app_t* vnc_app_get_instance(void);
void vnc_app_connect_server(vnc_app_t* app, const char* addr, uint16_t port, const char* pass);
void vnc_app_get_server(vnc_app_t* app, char* addr, uint16_t* port, char* pass);

/*
void vnc_app_set_state(vnc_app_t* app, app_state_t state, app_action_t action);
*/




//
//
//

static void net_init(vnc_app_t* app)
{
}

static void task_netif_up(void* param)
{
    vnc_app_t* app = (vnc_app_t*)param;

    

    //
    vTaskDelete(NULL);
}

void net_start(vnc_app_t* app)
{
}



//
// ====================================================================
//

vnc_app_t vnc_app =
{
    .disp = NULL,
    .scrn = NULL,
    .client = NULL,

    .wifi_name = { 0 },
    .wifi_pass = { 0 },

    .server_addr = "192.168.219.201", // { 0 },
    .server_port = 5900,
    .server_pass = "password", // { 0 },

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
        ESP_LOGI(TAG, "PostEvent(%d, %u, %u, %u)", id, data1, data2, data3);
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
    ESP_LOGI(TAG, "Connect To: %s#%u (%s)", addr, port, pass);
    // vnc_app_data_lock(app);
    {
        strcpy(app->server_addr, addr/*, sizeof(app->server_addr)*/);
        app->server_port = port;
        strcpy(app->server_pass, pass/*, sizeof(app->server_pass)*/);
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
            strcpy(addr, app->server_addr/*, sizeof(app->server_addr)*/);
        if (port)
            *port = app->server_port;
        if (pass)
            strcpy(pass, app->server_pass/*, sizeof(app->server_pass)*/);
    }
    // vnc_app_data_unlock(app);
}


void vnc_app_set_state(vnc_app_t* app, app_state_t state, app_action_t action)
{
    if (state >= 0)
        app->state = state;
    if (action >= 0)
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
            ESP_LOGI(TAG, "RecvEvent(%d, %d, %d, %d)", msg.id, (int)msg.data1, (int)msg.data2, (int)msg.data3);

            switch (msg.id)
            {
            case NETWORK_CONNECTED:
                strcpy(buf, inet_ntoa(msg.data1));
                vnc_log_printf(app->scrn, "[I] IP = % s\n", buf);
                vnc_app_set_state(app, APP_STATE_READY, APP_ACTION_NONE);
                break;
            case NETWORK_DISCONNECTED:
                vnc_log_printf(app->scrn, "[I] Network Disconnected!\n");
                vnc_app_set_state(app, APP_STATE_STANDBY, APP_ACTION_NONE);
                break;

            case VNC_CONNECT_TO_SERVER:
                if (1 || (app->state == APP_STATE_READY && app->action == APP_ACTION_NONE))
                {
                    app->client = vnc_client_start(app);
                    if (!app->client)
                        vnc_log_printf(app->scrn, "[E] Failed to start client\n", buf);
                }
                else
                {
                    vnc_log_printf(app->scrn, "[W] You can't connect to server\n", buf);
                }
                break;
            case VNC_DISCONNECT_SERVER:
                if (app->state == APP_STATE_PLAY && app->action == APP_ACTION_NONE)
                {
                    vnc_client_stop(app->client);
                    vnc_app_set_state(app, -1, APP_ACTION_VNC_DISCONNECT);
                    //vnc_app_send_event(app, VNC_SERVER_DISCONNECTED, -1, -1, -1);
                }
                else
                {
                    ESP_LOGI(TAG, "[W] Ignore disconnect command");
                }
                break;

            case VNC_SERVER_CONNECTED:
                if (msg.data1 == 0)
                    vnc_log_printf(app->scrn, "[I] Server connected: %s\n", app->server_addr);
                break;
            case VNC_HANDSHAKE_FINISHED:
                if (msg.data1 != -1)
                {
                    vnc_log_printf(app->scrn, "[I] Negotiated: %dx%d %dbps\n", (int)msg.data1, (int)msg.data2, (int)msg.data3 * 8);
                    vnc_log_append(app->scrn, "[I] Start Play\n");

                    if (vnc_screen_start_play(app->scrn, (int)msg.data1, (int)msg.data2, (int)msg.data3))
                    {
                        vnc_app_set_state(app, APP_STATE_PLAY, APP_ACTION_NONE);
                    }
                    else
                    {
                        // disconnect vnc-server
                        // ...
                    }
                }
                else
                {
                    vnc_log_append(app->scrn, "[E] Failed negotiation\n");
                }
                break;
            case VNC_SERVER_DISCONNECTED:
                if (msg.data1 != -1)
                {
                    ESP_LOGI(TAG, "Stop Play");
                    vnc_log_append(app->scrn, "[I] Stop Play\n");
                    vnc_screen_stop_play(app->scrn);
                    vnc_app_set_state(app, APP_STATE_READY, APP_ACTION_NONE);
                }
                else
                {
                    vnc_log_append(app->scrn, "[E] Failed to connect to server\n");
                }
                break;


            case OPEN_WIFI_SETTING:
                vnc_screen_open_wifi_setting(app->scrn);
                break;

            case OPEN_CONNECT_POPUP:
                vnc_screen_open_connect_popup(app->scrn);
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
    // Initialize BSP/LVGL display & Start LVGL Task
    vnc_app.disp = vnc_display_start();
    vTaskDelay(pdMS_TO_TICKS(100));
    // Initialize vnc screen
    vnc_app.scrn = vnc_screen_init(&vnc_app);

    /*
    vnc_app.client = NUll;
    */

    vnc_app.state = APP_STATE_STANDBY;
    vnc_app.action = APP_ACTION_NONE;

    vnc_app.app_mux = xSemaphoreCreateMutex();
    vnc_app.event_queue = xQueueCreate(5, sizeof(AppEventMsg_t));

    /*
    vnc_app.scrn->create(vnc_app.scrn);
    */

    // start main-task
    xTaskCreate(app_task, "main", 8 * 1024, &vnc_app, tskIDLE_PRIORITY + 2, NULL);
}
