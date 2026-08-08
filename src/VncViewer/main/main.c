// main.c
//

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "sdkconfig.h"
#include "bsp_display.h"
#include "vnc_display.h"
#include "vnc_screen.h"



//
//
//

static const char* TAG = "Main";



/**
 * 
 * 
 * 
 * 
 */

typedef enum {
    WIFI_STATE_INIT,          // 초기화 중
    WIFI_STATE_DISCONNECTED,  // 연결 없음 (스캔하기 가장 좋은 상태)
    WIFI_STATE_CONNECTING,    // 공유기에 접속 시도 중 (스캔 보류 필요)
    WIFI_STATE_CONNECTED,     // 연결됨 (IP 획득 완료, 스캔 가능)
} wifi_custom_state_t;

// 전역 또는 컴포넌트 내 상태 변수
static wifi_custom_state_t g_wifi_state = WIFI_STATE_INIT;




 // 보안 모드를 문자열로 변환하는 함수
static const char* get_auth_mode_name(wifi_auth_mode_t auth_mode) {
    switch (auth_mode) {
        case WIFI_AUTH_OPEN:            return "OPEN";
        case WIFI_AUTH_WEP:             return "WEP";
        case WIFI_AUTH_WPA_PSK:         return "WPA_PSK";
        case WIFI_AUTH_WPA2_PSK:        return "WPA2_PSK";
        case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA_WPA2_PSK";
        case WIFI_AUTH_WPA3_PSK:        return "WPA3_PSK";
        case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2_WPA3_PSK";
        default:                        return "UNKNOWN";
    }
}

static int rssi_to_percentage_linear(int rssi) {
    // 1. 최악의 신호(-100 dBm 이하)는 0% 처리
    if (rssi <= -100) {
        return 0;
    }
    // 2. 최상의 신호(-50 dBm 이상)는 100% 처리
    if (rssi >= -50) {
        return 100;
    }
    // 3. -50 ~ -100 사이 선형 계산
    return 2 * (rssi + 100);
}

static void print_scan_result()
{
    uint16_t number = 0;
    esp_wifi_scan_get_ap_num(&number);
    
    if (number == 0) {
        ESP_LOGW(TAG, "발견된 AP가 없습니다.");
        vnc_log_append("AP was not found.\n");
        return;
    }

    wifi_ap_record_t *ap_info = malloc(sizeof(wifi_ap_record_t) * number);
    if (ap_info == NULL) return;

    if (esp_wifi_scan_get_ap_records(&number, ap_info) == ESP_OK) {
        for (int i = 0; i < number; i++) {
            ESP_LOGI(TAG, "SSID %s, RSSI: %d, Auth: %d",
                ap_info[i].ssid, ap_info[i].rssi, ap_info[i].authmode);
            vnc_log_printf("[%s] %d%%, %s\n",
                ap_info[i].ssid, 
                rssi_to_percentage_linear(ap_info[i].rssi), 
                get_auth_mode_name(ap_info[i].authmode));
        }
    }
    free(ap_info);
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
        ESP_LOGI(TAG, "Wi-Fi 백그라운드 스캔 완료! 데이터를 가져옵니다.");
        print_scan_result();        
    }
}

// IP 획득 이벤트 핸들러 추가
static void ip_event_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI("Main", "IP를 받았습니다! 이제 안정적으로 스캔을 시작합니다.");
        
        wifi_scan_config_t scan_config = { .show_hidden = true };
        esp_wifi_scan_start(&scan_config, false); // 연결 중이 아니므로 false도 안전
    }
}


bool has_saved_credentials(void) {
    wifi_config_t conf;
    // 현재 메모리/NVS에 설정된 STA 설정값 읽기
    if (esp_wifi_get_config(WIFI_IF_STA, &conf) == ESP_OK) {
        // SSID 길이가 0보다 크면 저장된 정보가 있는 것임
        if (strlen((char*)conf.sta.ssid) > 0) {
            return true; 
        }
    }
    return false;
}

bool is_wifi_connected(void)
{
    // 1. 기본 STA 모드의 네트워크 인터페이스 포인터 가져오기
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    
    if (netif != NULL) {
        // 2. 물리적/논리적 링크가 연결(Up) 상태인지 확인
        return esp_netif_is_netif_up(netif);
    }
    return false;
}

bool check_wifi_status(void)
{
    wifi_ap_record_t ap_info;
    // 현재 연결된 AP(공유기) 정보 가져오기 시도
    esp_err_t err = esp_wifi_sta_get_ap_info(&ap_info);
    
    if (err == ESP_OK) {
        // 연결 성공 상태 (ap_info.ssid로 공유기 이름도 확인 가능)
        return true; 
    }
    return false; // 연결되지 않음
}

bool is_ip_assigned(void)
{
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == NULL) return false;

    esp_netif_ip_info_t ip_info;
    // 현재 인터페이스의 IP 정보 직접 읽기
    if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        // IP 주소가 0.0.0.0이 아니라면 할당 완료된 상태
        if (ip_info.ip.addr != 0) {
            return true;
        }
    }
    return false;
}

void print_assigned_ip()
{
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif)
    {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) 
        {
            char sz[32];

            snprintf(sz, sizeof(sz), IPSTR, IP2STR(&ip_info.ip));
            
            // IP 주소가 0.0.0.0이 아니라면 할당 완료된 상태
            if (ip_info.ip.addr != 0) {
                
            }
        }
    }    
    
}



static void wifi_and_ip_event_handler(void* arg, esp_event_base_t event_base,
                                      int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                vnc_log_append("WiFi started\n");
                // Wi-Fi 가동 시작됨
                if (has_saved_credentials()) {
                    // 저장된 정보가 있다면 호스트/슬레이브 드라이버가 자동으로 연결을 시도함
                    g_wifi_state = WIFI_STATE_CONNECTING;
                    ESP_LOGI("WIFI", "저장된 접속 정보가 있어 자동으로 연결을 시도합니다...");
                    vnc_log_append("  --> connecting...\n");
                } else {
                    g_wifi_state = WIFI_STATE_DISCONNECTED;
                    ESP_LOGI("WIFI", "저장된 접속 정보가 없습니다. 대기 상태.");                    
                    vnc_log_append("  --> standby\n");

                    //
                    wifi_scan_config_t scan_config = { .show_hidden = true };
                    esp_wifi_scan_start(&scan_config, false);
                    vnc_log_append("Scan started...\n");
                }
                break;

            case WIFI_EVENT_STA_CONNECTED:
                // AP와 링크는 연결되었으나 아직 IP는 없는 상태
                g_wifi_state = WIFI_STATE_CONNECTING; 
                vnc_log_append("WiFi connected\n");
                break;

            case WIFI_EVENT_STA_DISCONNECTED:
                // 연결이 끊겼거나 실패함
                g_wifi_state = WIFI_STATE_DISCONNECTED;
                ESP_LOGW("WIFI", "Wi-Fi 연결 해제됨 (또는 연결 실패)");
                vnc_log_append("WiFi diconnected\n");
                break;
                
            case WIFI_EVENT_SCAN_DONE:
                ESP_LOGI("WIFI", "스캔 완료 이벤트 수신");
                vnc_log_append("Scan completed\n");
                print_scan_result();
                break;
        }
    } 
    else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            // IP까지 완벽하게 할당받음
            g_wifi_state = WIFI_STATE_CONNECTED;
            ESP_LOGI("WIFI", "IP 할당 완료. 네트워크 안정화 상태.");

            char ip[16] = { 0 };
            esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
            if (netif)
            {
                esp_netif_ip_info_t ip_info;
                if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) 
                    snprintf(ip, sizeof(ip), IPSTR, IP2STR(&ip_info.ip));
            }

            vnc_log_printf("Got IP: %s\n", ip);


            //
            wifi_scan_config_t scan_config = { .show_hidden = true };
            esp_wifi_scan_start(&scan_config, false);
            vnc_log_append("Scan Started...\n");
        }
    }
}



/**
 * @brief Application entry point
 */

void app_main(void) 
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);    

    // Initialize BSP/LVGL display & Start LVGL Task
    vnc_display_start(vnc_screen_init);

    // turn on backlight: brightness 30%
    bsp_display_brightness_set(30);  


    //
    // Discovery WIFI
    //
    //
    //

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
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    // Wi-Fi 초기화 (Hosted 모드 설정 반영)
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    /*
     * 구성을 NVS 플래시에 저장하지 않고 오직 RAM에서만 유지하도록 설정
     * 자동으로 연결하는 것을 막을 수 있다.
     * 
     * 
    // method1
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));    

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "",
            .password = "",
        },
    };

    // method2: 빈 설정을 명시적으로 주입
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    */
    ESP_ERROR_CHECK(esp_wifi_start());
    //esp_wifi_sconnect()
    //esp_wifi_disconnect();
    //if (has_saved_credentials())
    //    vnc_log_append("it has saved credentials\n");

    ESP_LOGI(TAG, "Wi-Fi 시작합니다...");

    #if 0
    // Wi-Fi 스캔 설정 (블로킹 방식으로 전체 채널 스캔)
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 120, // 채널당 최소 머무는 시간 (ms)
        .scan_time.active.max = 200  // 채널당 최대 머무는 시간 (ms)
    };
    
    // 스캔 실행 (true: 스캔이 끝날 때까지 대기)
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));
    ESP_LOGI(TAG, "esp_wifi_scan_start()가 바로 리턴되었습니다. 백그라운드 스캔 중...");
    #endif

    #if 0
    // 5. 발견된 AP 개수 확인
    uint16_t number = 0;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&number));
    
    if (number == 0) {
        ESP_LOGW(TAG, "주변에 발견된 Wi-Fi 네트워크가 없습니다.");
        return;
    }

    // 메모리 할당 및 AP 정보 가져오기
    wifi_ap_record_t *ap_info = malloc(sizeof(wifi_ap_record_t) * number);
    if (ap_info == NULL) {
        ESP_LOGE(TAG, "메모리가 부족합니다.");
        return;
    }
    
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));

    // 6. 결과 출력 (SSID, 신호 세기, 보안 모드)
    ESP_LOGI(TAG, "========================================================");
    ESP_LOGI(TAG, "%-32s | %-4s | %-15s", "SSID (Name)", "RSSI", "Security");
    ESP_LOGI(TAG, "========================================================");
    
    for (int i = 0; i < number; i++) {
        ESP_LOGI(TAG, "%-32s | %d dBm | %-15s",
                 (char *)ap_info[i].ssid,
                 ap_info[i].rssi,
                 get_auth_mode_name(ap_info[i].authmode));
    }
    ESP_LOGI(TAG, "========================================================");

    // 메모리 해제
    free(ap_info);
    #endif
}
