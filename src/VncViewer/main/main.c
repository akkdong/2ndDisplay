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
#include "esp_netif.h"
#include "nvs_flash.h"

#include "sdkconfig.h"
#include "bsp_display.h"
#include "vnc_display.h"
#include "vnc_screen.h"
#include "app_main.h"


//
//
//

static const char* TAG = "Main";

static vnc_screen_t* g_scrn;


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

static void print_scan_result(vnc_app_t* app)
{
    uint16_t number = 0;
    esp_wifi_scan_get_ap_num(&number);
    
    if (number == 0) {
        ESP_LOGW(TAG, "발견된 AP가 없습니다.");
        //vnc_log_append(g_scrn, "AP was not found.\n");
        return;
    }

    wifi_ap_record_t *ap_info = malloc(sizeof(wifi_ap_record_t) * number);
    if (ap_info == NULL) return;

    if (esp_wifi_scan_get_ap_records(&number, ap_info) == ESP_OK) {
        for (int i = 0; i < number; i++) {
            ESP_LOGI(TAG, "SSID %s, RSSI: %d, Auth: %d",
                ap_info[i].ssid, ap_info[i].rssi, ap_info[i].authmode);
            /*
            vnc_log_printf(g_scrn, "[%s] %d%%, %s\n",
                ap_info[i].ssid, 
                rssi_to_percentage_linear(ap_info[i].rssi), 
                get_auth_mode_name(ap_info[i].authmode));
            */
        }
    }
    free(ap_info);
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
        ESP_LOGI(TAG, "Wi-Fi 백그라운드 스캔 완료! 데이터를 가져옵니다.");
        print_scan_result(0);        
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
                //vnc_app_send_event(app, NETWORK_CONNECTED, ip_info.ip.addr, 0, 0);
            }
        }
    }    
    
}


static int retry = 5;

static void wifi_and_ip_event_handler(void* arg, esp_event_base_t event_base,
                                      int32_t event_id, void* event_data)
{
    vnc_app_t* app = (vnc_app_t *)arg;

    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                //vnc_log_append(g_scrn, "WiFi started\n");
                // Wi-Fi 가동 시작됨
                /*
                if (has_saved_credentials()) {
                    // 저장된 정보가 있다면 호스트/슬레이브 드라이버가 자동으로 연결을 시도함
                    g_wifi_state = WIFI_STATE_CONNECTING;
                    ESP_LOGI("WIFI", "저장된 접속 정보가 있어 자동으로 연결을 시도합니다...");
                    //vnc_log_append(g_scrn, "  --> connecting...\n");
                } else {
                    g_wifi_state = WIFI_STATE_DISCONNECTED;
                    ESP_LOGI("WIFI", "저장된 접속 정보가 없습니다. 대기 상태.");                    
                    //vnc_log_append(g_scrn, "  --> standby\n");

                    //
                    wifi_scan_config_t scan_config = { .show_hidden = true };
                    esp_wifi_scan_start(&scan_config, false);
                    //vnc_log_append(g_scrn, "Scan started...\n");
                }
                */
                ESP_LOGI(TAG, "WiFi STA statrted! try connect...");
                esp_wifi_connect();
                break;

            case WIFI_EVENT_STA_CONNECTED:
                // AP와 링크는 연결되었으나 아직 IP는 없는 상태
                g_wifi_state = WIFI_STATE_CONNECTING; 
                //vnc_log_append(g_scrn, "WiFi connected\n");
                ESP_LOGI(TAG, "WiFi STA connected!");
                break;

            case WIFI_EVENT_STA_DISCONNECTED:
                // 연결이 끊겼거나 실패함
                g_wifi_state = WIFI_STATE_DISCONNECTED;
                ESP_LOGW(TAG, "Wi-Fi 연결 해제됨 (또는 연결 실패)");
                //vnc_log_append(g_scrn, "WiFi diconnected\n");
                if (retry-- > 0)
                {
                    ESP_LOGI(TAG, "WiFi reconnect");
                    esp_wifi_connect();
                }
                break;
                
            case WIFI_EVENT_SCAN_DONE:
                ESP_LOGI(TAG, "스캔 완료 이벤트 수신");
                //vnc_log_append(g_scrn, "Scan completed\n");
                print_scan_result(app);
                break;
        }
    } 
    else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            // IP까지 완벽하게 할당받음
            g_wifi_state = WIFI_STATE_CONNECTED;
            ESP_LOGI(TAG, "IP 할당 완료. 네트워크 안정화 상태.");

            //bool is_up = esp_netif_is_up(c6_netif);
            //ESP_LOGI("NET", "Netif is UP: %s", is_up ? "YES" : "NO");

            char ip[16] = { 0 };
            esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
            if (netif)
            {
                esp_netif_ip_info_t ip_info;
                if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) 
                    snprintf(ip, sizeof(ip), IPSTR, IP2STR(&ip_info.ip));

                vnc_app_send_event(app, NETWORK_CONNECTED, ip_info.ip.addr, 0, 0);
                vnc_log_printf(app->scrn, "Got IP: %s\n", ip);
            }


            //
            //wifi_scan_config_t scan_config = { .show_hidden = true };
            //esp_wifi_scan_start(&scan_config, false);
            //vnc_log_append(app->scrn, "Scan Started...\n");
        }
    }
}





void vnc_start_client(vnc_screen_t* scrn, const char* addr, uint16_t port)
{
    /*
    vnc_connect_info_t* cinfo = (vnc_connect_info_t *)arg;

    vnc_client_t* client = malloc(vcn_client_t);
    vnc_client_init(client);
    vnc_client_start(client, cinfo->addr, cinfo->port);
    */
}



static esp_err_t nvs_init()
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    return ret;
}


#define CONFIG_EH_EXAMPLE_WIFI_COUNTRY          "KR"
#define CONFIG_EH_EXAMPLE_WIFI_MAXIMUM_RETRY    6

/* --- STA PHY knobs: one small setter per concern (each a no-op when its knob
 * isn't selected), a read-back reporter, and an orderer. Applied between
 * esp_wifi_start() and connect; all RPC-forwarded to the CP. --- */

/* Regulatory country — gates which channels (esp. 5 GHz) are legal. Empty string
 * keeps the CP default. Needs init only; non-fatal on an unsupported code. */
static void sta_set_country(void)
{
    if (!CONFIG_EH_EXAMPLE_WIFI_COUNTRY[0])
        return;
    esp_err_t rc = esp_wifi_set_country_code(CONFIG_EH_EXAMPLE_WIFI_COUNTRY, true);
    if (rc != ESP_OK)
        ESP_LOGW(TAG, "set_country_code(%s) not applied: %s",
                 CONFIG_EH_EXAMPLE_WIFI_COUNTRY, esp_err_to_name(rc));
}

/* Band select — dual-band CP (e.g. C5) comes up 5 GHz-only, so set before connect.
 * MUST run after esp_wifi_start() (else ESP_ERR_WIFI_NOT_STARTED). */
static void sta_set_band(void)
{
#if CONFIG_SLAVE_SOC_WIFI_SUPPORT_5G
    ESP_ERROR_CHECK(esp_wifi_set_band_mode(EH_EXAMPLE_WIFI_BAND_MODE));
#endif
}

/* PHY protocol (11b/g/n/ax). Set BEFORE bandwidth — HT40 requires 11n, and 11AX
 * caps the single-band set_bandwidth at HT20, so a 40 MHz test pins "up to 11n".
 * Default (knob unset) leaves the CP's max protocol. Non-fatal. */
static void sta_set_protocol(void)
{
#ifdef EH_EXAMPLE_WIFI_PROTO
    esp_err_t rc = esp_wifi_set_protocol(WIFI_IF_STA, EH_EXAMPLE_WIFI_PROTO);
    if (rc != ESP_OK)
        ESP_LOGW(TAG, "set_protocol(0x%02x) not applied: %s",
                 (unsigned)EH_EXAMPLE_WIFI_PROTO, esp_err_to_name(rc));
    else
        ESP_LOGI(TAG, "STA protocol set to 0x%02x", (unsigned)EH_EXAMPLE_WIFI_PROTO);
#endif
}

/* Fixed 20/40 MHz width for throughput characterization. Must run after band is
 * selected. Invalid under AUTO band (ESP_ERR_NOT_SUPPORTED → use set_bandwidths);
 * on 11AX/AC only HT20 is settable. Non-fatal — falls back to negotiated width. */
static void sta_set_bandwidth(void)
{
#ifdef EH_EXAMPLE_WIFI_BW
    esp_err_t rc;
#if CONFIG_EH_EXAMPLE_WIFI_BAND_AUTO
    /* AUTO (2.4G+5G): the singular set_bandwidth is ESP_ERR_NOT_SUPPORTED, so use
     * the per-band plural API — apply the chosen width to both bands. */
    wifi_bandwidths_t bws = { .ghz_2g = EH_EXAMPLE_WIFI_BW, .ghz_5g = EH_EXAMPLE_WIFI_BW };
    rc = esp_wifi_set_bandwidths(WIFI_IF_STA, &bws);
#else
    /* Single band: the plain per-interface API (note: 11AX/AC only accepts HT20). */
    rc = esp_wifi_set_bandwidth(WIFI_IF_STA, EH_EXAMPLE_WIFI_BW);
#endif
    if (rc != ESP_OK)
        ESP_LOGW(TAG, "set bandwidth not applied: %s (negotiated width kept)",
                 esp_err_to_name(rc));
    else
        ESP_LOGI(TAG, "STA channel width pinned to HT%s",
                 EH_EXAMPLE_WIFI_BW == WIFI_BW40 ? "40" : "20");
#endif
}

/* Read the knobs back and report the effective PHY in one line — the gets can
 * differ from the sets (AP-negotiated width, or a CP that lacks a getter). */
static void sta_report_phy(void)
{
    char cc[4] = "?";
    wifi_band_mode_t bm = 0;
    (void)esp_wifi_get_country_code(cc);
    bool have_bm = (esp_wifi_get_band_mode(&bm) == ESP_OK);

    /* Choose the getter by the RUNTIME band, not a compile-time guess: the
     * singular get_bandwidth is ESP_ERR_NOT_SUPPORTED under AUTO (2.4G+5G), so
     * the per-band plural must be used there. (Compile-time selection breaks
     * when the effective band differs from Kconfig — e.g. band-set gated out.) */
    char wbuf[20] = "n/a";
    if (have_bm && bm == WIFI_BAND_MODE_AUTO) {
        wifi_bandwidths_t bws = { 0 };
        if (esp_wifi_get_bandwidths(WIFI_IF_STA, &bws) == ESP_OK)
            snprintf(wbuf, sizeof(wbuf), "2G:HT%s/5G:HT%s",
                     bws.ghz_2g == WIFI_BW40 ? "40" : "20",
                     bws.ghz_5g == WIFI_BW40 ? "40" : "20");
    } else {
        wifi_bandwidth_t bw = 0;
        if (esp_wifi_get_bandwidth(WIFI_IF_STA, &bw) == ESP_OK)
            snprintf(wbuf, sizeof(wbuf), "HT%s", bw == WIFI_BW40 ? "40" : "20");
    }
    char pbuf[12] = "n/a";
    uint8_t proto = 0;
    if (esp_wifi_get_protocol(WIFI_IF_STA, &proto) == ESP_OK)
        snprintf(pbuf, sizeof(pbuf), "0x%02x", proto);

    ESP_LOGI(TAG, "STA PHY effective: country=%s band=%s proto=%s width=%s", cc,
             !have_bm ? "n/a" :
                 (bm == WIFI_BAND_MODE_5G_ONLY ? "5G" :
                  bm == WIFI_BAND_MODE_AUTO    ? "AUTO" : "2G"),
             pbuf, wbuf);
}

/* Order matters: country (region) → band → protocol → width, then read-back
 * (protocol before width: HT40 needs 11n). */
static void apply_sta_phy_cfg(void)
{
    sta_set_country();
    sta_set_band();
    sta_set_protocol();
    sta_set_bandwidth();
    sta_report_phy();
}


#include "eh_host_port_sync.h"
typedef eh_host_port_sem_t *eh_ex_sem_t;
#define EH_EX_SEM_CREATE()    eh_host_port_sem_create()
#define EH_EX_SEM_POST(s)     eh_host_port_sem_post(s)
#define EH_EX_SEM_WAIT(s)     eh_host_port_sem_wait_ms((s), EH_HOST_PORT_WAIT_FOREVER)
#define EH_EX_SEM_DESTROY(s)  eh_host_port_sem_destroy(s)

static eh_ex_sem_t         s_got_ip_sem = NULL;
static int                 s_retry_num = 0;
static bool                s_handlers_registered;

/* Human-readable disconnect reason + an actionable hint — the silent stall at
 * STA_START is the worst UX; surfacing the reason tells the user what to fix. */
static const char *disc_reason_str(uint8_t r)
{
    switch (r) {
    case WIFI_REASON_NO_AP_FOUND:
        return "NO_AP_FOUND — SSID not seen; check the SSID and the band "
               "(2.4 vs 5 GHz: set CONFIG_EH_EXAMPLE_WIFI_BAND_*)";
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
        return "AUTH/handshake — wrong password or auth mode";
    case WIFI_REASON_ASSOC_FAIL:
        return "ASSOC_FAIL — AP rejected association";
    case WIFI_REASON_BEACON_TIMEOUT:
        return "BEACON_TIMEOUT — weak signal / AP out of range";
    default:
        return "see wifi_err_reason_t";
    }
}

static void on_disconnect(void *arg, esp_event_base_t base,
                          int32_t id, void *event_data)
{
    (void)arg; (void)base; (void)id;
    wifi_event_sta_disconnected_t *e = (wifi_event_sta_disconnected_t *)event_data;
    uint8_t reason = e ? e->reason : 0;
    s_retry_num++;
    if (s_retry_num > CONFIG_EH_EXAMPLE_WIFI_MAXIMUM_RETRY) {
        ESP_LOGE(TAG, "connect FAILED after %d tries — reason %u: %s",
                 CONFIG_EH_EXAMPLE_WIFI_MAXIMUM_RETRY, reason, disc_reason_str(reason));
        if (s_got_ip_sem) EH_EX_SEM_POST(s_got_ip_sem);
        return;
    }
    ESP_LOGW(TAG, "disconnected (reason %u: %s) — retry %d/%d",
             reason, disc_reason_str(reason),
             s_retry_num, CONFIG_EH_EXAMPLE_WIFI_MAXIMUM_RETRY);
    esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED) {
        ESP_ERROR_CHECK(err);
    }
}

/* Association succeeded — make success visible (DHCP then fires automatically). */
static void on_connected(void *arg, esp_event_base_t base,
                         int32_t id, void *event_data)
{
    (void)arg; (void)base; (void)id;
    wifi_event_sta_connected_t *e = (wifi_event_sta_connected_t *)event_data;
    if (e)
        ESP_LOGI(TAG, "associated to \"%.*s\" ch %u — waiting for DHCP (auto)",
                 e->ssid_len, (const char *)e->ssid, e->channel);
}

static void on_got_ip(void *arg, esp_event_base_t base,
                      int32_t id, void *event_data)
{
    (void)arg; (void)base; (void)id;
    ip_event_got_ip_t *e = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "got IPv4 " IPSTR, IP2STR(&e->ip_info.ip));
    s_retry_num = 0;
    if (s_got_ip_sem) EH_EX_SEM_POST(s_got_ip_sem);
}




/**
 * @brief Application entry point
 */
void app_main(void) 
{
    // Initialize NVS
    nvs_init();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_LOGI(TAG, "init ok");


    esp_netif_t* netif = esp_netif_create_default_wifi_sta();

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, on_connected, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, on_disconnect, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_got_ip, NULL));


    wifi_config_t wcfg = { 
        .sta = {
            .ssid = "nauty24",
            .password = "homesweethome",
            .threshold = {
                .authmode = WIFI_AUTH_WPA2_PSK,
            }
        }
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wcfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    apply_sta_phy_cfg();


     esp_err_t cerr = esp_wifi_connect();
    if (cerr != ESP_OK && cerr != ESP_ERR_WIFI_CONN)
        ESP_ERROR_CHECK(cerr);


    // Initialize VNC application
    vnc_app_t* app = vnc_app_init();

    // turn on backlight: brightness 30%
    bsp_display_brightness_set(30);  

#if 0
    //
    // Discovery WIFI
    //
    //
    //

    // 네트워크 인터페이스 및 이벤트 루프 초기화
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    //
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_and_ip_event_handler,
                                                        app,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, 
                                                        ESP_EVENT_ANY_ID, 
                                                        &wifi_and_ip_event_handler, 
                                                        app, 
                                                        NULL));
    //
    // ESP-Hosted 드라이버가 초기화된 후 생성된 netif를 바인딩합니다.
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    // LwIP의 기본 라우팅 경로로 강제 지정
    esp_netif_set_default_netif(sta_netif);

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
    */
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "nauty24",
            .password = "homesweethome",
        },
    };

    // method2: 빈 설정을 명시적으로 주입
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

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
#endif

    // main task
    // ...
}
