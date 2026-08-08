// vnc_client.h
//

#pragma once


#ifdef __cplusplus
extern "C"
{
#endif


struct PixelFormat {
    uint8_t bpp;
    uint8_t depth;
    uint8_t big_endian;
    uint8_t true_color;
    uint16_t red_max;
    uint16_t green_max;
    uint16_t blue_max;
    uint8_t red_shift;
    uint8_t green_shift;
    uint8_t blue_shift;
    uint8_t pad[3];
} __attribute__((packed));

typedef struct PixelForamt PixelForamt_t;

struct ServerInit {
    uint16_t fb_width;
    uint16_t fb_height;
    PixelFormat fmt;
    uint32_t name_len;
} __attribute__((packed));

typedef struct ServerInit ServerInit_t;


#include <stdatomic.h>

typedef struct vnc_client
{
    int fd_;
    int bpp_;
    uint16_t fbw_;
    uint16_t fbh_;
    PixelFormat fmt_;   
    uint32_t* fb_; // std::vector<uint32_t> fb_;
    // Display* disp;

    bool need_update_;

    z_stream zstream_[4];

    uint8_t* rbuf_; // std::vector<uint8_t> rbuf_;
    size_t rpos_;

    atomic_bool breakLoop; // std::atomic<bool> breakLoop;
    #if 0
    void do_scan() {
        atomic_store(&is_scanning, true);
        // ...
        if (atomic_load(&is_scanning)) { /* ... */ }
    }    
    #endif

} vnc_client_t;

#if 0 // std::atomic<boo> 대체 방법

// 대기하는 태스크 (예: Main Task)
TaskHandle_t main_task_handle;

void app_main() {
    main_task_handle = xTaskGetCurrentTaskHandle();
    
    // Wi-Fi 스캔 시작 명령...
    esp_wifi_scan_start(&config, false);

    // bool 플래그를 계속 체크하는 대신, 알림이 올 때까지 멈춰서 대기 (CPU 점유 0%)
    uint32_t notification_value;
    if (xTaskNotifyWait(0, ULONG_MAX, &notification_value, pdMS_TO_TICKS(5000)) == pdTRUE) {
        // 알림을 받음! 스캔 결과 처리 진행
    }
}

// 이벤트를 보내는 곳 (예: Wi-Fi Event Handler)
static void wifi_event_handler(...) {
    if (event_id == WIFI_EVENT_SCAN_DONE) {
        // 대기 중인 메인 태스크에 '완료' 알림 전송 (bool 플래그 세팅 역할을 대신함)
        xTaskNotifyGive(main_task_handle);
    }
}




#include "freertos/event_groups.h"

EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT  (1 << 0)
#define WIFI_SCANNING_BIT   (1 << 1)

void init() {
    wifi_event_group = xEventGroupCreate();
}

// 특정 비트 세팅 (Atomic하게 비트 수정)
xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
// 특정 비트 클리어
xEventGroupClearBits(wifi_event_group, WIFI_SCANNING_BIT);



#endif


void vnc_client_create(vnc_client* client);
bool vnc_client_connect(vnc_client* client, const char* ip, uint16_t port);
bool vnc_client_handshake(vnc_client* client);
void vnc_client_loop(vnc_client* client);
void vnc_client_run(vnc_client* client);

bool vnc_client_isOk();

void vnc_client_send_pointer(); // ??

/*

vnc_client client;

vnc_client_create(&client);

void vnc_client_task(void* arg)
{
    Display* display = reinterpret_cast<Display *>(data);
    std::shared_ptr<VncClient> clientPtr = display->m_clientPtr;
    vnc_screen_run_state(true); // display->m_clientRunning = true;

    //if (clientPtr->connect(display->m_hostAddr, display->m_hostPort))
    if (vnc_client_connect(client, addr, port))
    {
        std::cout << "Connected to " << display->m_hostAddr << ":" << display->m_hostPort << std::endl;
        vnc_screen_push_event(EVT_CONNECTED); //display->pushEvent(Display::EVT_CONNECTED);


        //if (clientPtr->handshake(display->m_password))
        if (vnc_client_handshake(client))
        {
            vnc_screen_push_event(EVT_NEGOTIATION_ESTABLISHED); // display->pushEvent(Display::EVT_NEGOTIATION_ESTABLISHED);

            //
            std::cout << "Enter VncClient::run()" << std::endl;            
            vnc_client_run(client); //clientPtr->run();
            std::cout << "Exit VncClient::run()" << std::endl;            
        }
        else
        {
            std::cerr << "Handshake failed" << std::endl;
            vnc_screen_push_event(EVT_HANDSHAKE_FAILED); //display->pushEvent(Display::EVT_HANDSHAKE_FAILED);
        }
    } 
    else
    {
        std::cerr << "Connection failed" << std::endl;
        vnc_screen_push_event(EVT_CONNECTION_FAILED); //display->pushEvent(Display::EVT_CONNECTION_FAILED);
    }

    vnc_screen_push_event(EVT_DISCONNECTED); //display->pushEvent(Display::EVT_DISCONNECTED);

    vnc_screen_run_state(false); // display->m_clientRunning = false;

    return 0;
}



*/


#ifdef __cplusplus
}
#endif
