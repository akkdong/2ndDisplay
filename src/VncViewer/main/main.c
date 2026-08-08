// main.c
//

#include "esp_lcd_types.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_mipi_dsi.h"

#include "sdkconfig.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "bsp_display.h"
#include "vnc_display.h"


#define MAX_LOG_LINES 50  // 유지할 최대 로그 줄 수



typedef struct vnc_screen
{
    lv_obj_t* active_scrn;
    int disp_width;
    int disp_height;

    lv_obj_t* layer_canvas;
    lv_obj_t* layer_main;
    lv_obj_t* layer_wifi;
    lv_obj_t* layer_conn;

    
} vnc_screen_t;


enum event_command
{
    EVT_SHOW_WIFI,
    EVT_SHOW_CONN,
    EVT_HIDE_WIFI,
    EVT_HIDE_CONN
};



//
//
//

LV_IMG_DECLARE(vnc_logo);

static const char* TAG = "Main";

static vnc_screen_t vnc_scrn;



/**
 * 
 * 
 */


static void clear_log_event_cb(lv_event_t * e)
{
    // 1. 등록할 때 함께 넘겨준 유저 데이터(log_ta)를 가져옵니다.
    lv_obj_t * log_ta = (lv_obj_t *)lv_event_get_user_data(e);

    // 2. 텍스트 영역의 글자들을 모두 지웁니다.
    lv_textarea_set_text(log_ta, "");
}

static void generic_timer_cb(lv_timer_t * timer)
{
  lv_obj_t* log_ta = (lv_obj_t *)lv_timer_get_user_data(timer);
  lv_textarea_add_text(log_ta, "HoHoHO\n");
  lv_textarea_set_cursor_pos(log_ta, LV_TEXTAREA_CURSOR_LAST);
}

void append_log_with_limit(lv_obj_t * ta, const char * new_text)
{
    // 1. 새 로그를 먼저 추가합니다.
    lv_textarea_add_text(ta, new_text);

    // 2. 현재 텍스트 영역의 전체 문자열을 가져옵니다.
    const char * current_text = lv_textarea_get_text(ta);
    if(current_text == NULL) return;

    // 3. 전체 텍스트에서 줄 바꿈('\n') 개수를 셉니다.
    int line_count = 0;
    for(int i = 0; current_text[i] != '\0'; i++) {
        if(current_text[i] == '\n') {
            line_count++;
        }
    }

    // 4. 만약 설정한 최대 줄 수를 초과했다면 옛날 로그를 잘라냅니다.
    if(line_count > MAX_LOG_LINES) {
        int lines_to_remove = line_count - MAX_LOG_LINES;
        int remove_index = 0;
        int found_lines = 0;

        // 지워야 할 줄 수만큼의 '\n' 위치를 찾습니다.
        for(int i = 0; current_text[i] != '\0'; i++) {
            if(current_text[i] == '\n') {
                found_lines++;
                if(found_lines == lines_to_remove) {
                    remove_index = i + 1; // '\n' 바로 다음 글자부터 남김
                    break;
                }
            }
        }

        // 옛날 로그가 잘려 나간 새 문자열 구조로 업데이트합니다.
        if(remove_index > 0) {
            // lv_textarea_set_text는 텍스트를 새로 덮어씌우는 함수입니다.
            lv_textarea_set_text(ta, &current_text[remove_index]);
        }
    }

    // 5. 이전 단계에서 구현한 '자동 스크롤' 연동 (커서를 맨 뒤로 보냄)
    lv_textarea_set_cursor_pos(ta, LV_TEXTAREA_CURSOR_LAST);
}


void vnc_shift_ready(lv_anim_t * anim)
{
    lv_obj_t* layer = (lv_obj_t *)anim->var;
    lv_obj_set_style_bg_opa(layer, LV_OPA_30, 0);
}

void vnc_shift_layer(lv_obj_t* layer, int pos)
{
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, layer);
    lv_anim_set_exec_cb(&anim, (lv_anim_exec_xcb_t)lv_obj_set_x);
    
    // Smooth movement from current location to target location
    lv_anim_set_values(&anim, lv_obj_get_x(layer), pos);
    lv_anim_set_duration(&anim, 500);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);
    
    lv_obj_set_style_bg_opa(layer, LV_OPA_0, 0);
    lv_anim_set_completed_cb(&anim, vnc_shift_ready);
    lv_anim_start(&anim);
}

static void vnc_event_handler(lv_event_t* evt)
{
    lv_event_code_t code = lv_event_get_code(evt);
    void* data = lv_event_get_user_data(evt);

    if (code == LV_EVENT_CLICKED)
    {
        if (data == (void *)EVT_SHOW_WIFI)
        {
            vnc_shift_layer(vnc_scrn.layer_wifi, 0);
        }
        else if (data == (void *)EVT_SHOW_CONN)
        {
            vnc_shift_layer(vnc_scrn.layer_conn, 0);
        }
        else if (data == (void *)EVT_HIDE_WIFI)
        {
            vnc_shift_layer(vnc_scrn.layer_wifi, -vnc_scrn.disp_width);
        }
        else if (data == (void *)EVT_HIDE_CONN)
        {
            vnc_shift_layer(vnc_scrn.layer_conn, vnc_scrn.disp_width);
        }
    }
}

static void vnc_size_changed(lv_event_t* evt)
{

}


static void vnc_create_layer_canvas(vnc_screen_t* scrn)
{    
    //scrn->layer_canvas = lv_canvas_create(scrn->active_scrn);
    scrn->layer_canvas = NULL;
}

static void vnc_create_layer_main(vnc_screen_t* scrn)
{
    lv_obj_t* layer = scrn->layer_main = lv_obj_create(scrn->active_scrn);

    lv_obj_remove_style_all(layer);
    //lv_obj_set_style_bg_opa(layer, LV_OPA_0, 0);
    //lv_obj_set_style_border_opa(layer, LV_OPA_0, 0);
    lv_obj_set_flex_flow(layer, LV_FLEX_FLOW_COLUMN);
    //lv_obj_set_size(layer, scrn->disp_width, scrn->disp_height);
    lv_obj_set_size(layer, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_all(layer, 15, 0);
    lv_obj_set_style_pad_gap(layer, 15, 0);

    // First Row: Logo & Title, Program Information
    lv_obj_t* top_header = lv_obj_create(layer);
    lv_obj_set_size(top_header, LV_PCT(100), LV_SIZE_CONTENT);
    /*
    lv_obj_remove_style_all(top_header);
    */
    lv_obj_set_style_bg_opa(top_header, LV_OPA_0, 0);
    lv_obj_set_style_border_opa(top_header, LV_OPA_0, 0);
    lv_obj_set_flex_flow(top_header, LV_FLEX_FLOW_ROW); // horizontal align
    lv_obj_set_style_flex_cross_place(top_header, LV_FLEX_ALIGN_START, 0);
    lv_obj_set_style_pad_all(top_header, 0, 0);
    lv_obj_set_style_pad_column(top_header, 12, 0); // space in logo and text

    // logo image
    lv_obj_t * logo_img = lv_image_create(top_header);
    lv_image_set_src(logo_img, &vnc_logo);
    lv_image_set_scale(logo_img, 128);
    lv_obj_set_size(logo_img, 150, 150);

    // title & program information
    lv_obj_t* text_box = lv_obj_create(top_header);
    lv_obj_remove_style_all(text_box);
    lv_obj_set_flex_grow(text_box, 1);
    lv_obj_set_height(text_box, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(text_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(text_box, 0, 0);
    lv_obj_set_style_pad_row(text_box, 6, 0);
    /*
    lv_obj_set_style_border_width(text_box, 1, 0);
    lv_obj_set_style_border_color(text_box, lv_color_make(0xFF, 0xA0, 0xA0), 0);
    */

    // title
    lv_obj_t* label_title = lv_label_create(text_box);
    lv_label_set_text(label_title, "VNC Viewer");
    lv_obj_set_size(label_title, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_text_color(label_title, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_title, &lv_font_montserrat_32, 0);
    // program information
    lv_obj_t* label_info = lv_label_create(text_box);
    lv_label_set_text(label_info, "\nVersion 1.0.0 (Alpha)\nAll rights is reserved");
    lv_obj_set_size(label_info, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_text_color(label_info, lv_color_hex(0x888888), 0);

    // Middle Content: System Status Log
    lv_obj_t * log_ta = lv_textarea_create(layer);
    lv_textarea_set_cursor_click_pos(log_ta, false);
    //lv_obj_set_clickable(log_ta, false);
    lv_obj_add_state(log_ta, LV_STATE_DISABLED);
    lv_obj_set_width(log_ta, LV_PCT(100));
    lv_obj_set_flex_grow(log_ta, 1);
    lv_obj_set_style_bg_opa(log_ta, LV_OPA_10, 0);
    lv_obj_set_style_border_opa(log_ta, LV_OPA_60, 0);
    lv_obj_set_style_bg_color(log_ta, lv_color_hex(0xC0C0C0), 0);
    lv_obj_set_style_text_color(log_ta, lv_color_white(), 0);
    lv_obj_set_style_border_color(log_ta, lv_color_hex(0x288CF4), 0);

    //lv_timer_create(generic_timer_cb, 1000, log_ta);

    // Clear Log Button
    lv_obj_t* btn_clear = lv_button_create(log_ta);
    lv_obj_t* clear_icon = lv_image_create(btn_clear);
    lv_image_set_src(clear_icon, LV_SYMBOL_REFRESH);
    /*
    lv_obj_align_to(btn_clear, log_ta, LV_ALIGN_BOTTOM_RIGHT, -12, -12);
    */
    lv_obj_align(btn_clear, LV_ALIGN_BOTTOM_RIGHT, -12, -12);
    lv_obj_set_style_opa(btn_clear, 120, LV_PART_MAIN);
    /*
     * 9.6 --> 9.5
     *
    lv_obj_set_scroll_chain(btn_clear, false);
    lv_obj_set_floating(btn_clear, true);
    */
    lv_obj_remove_flag(btn_clear, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_add_flag(btn_clear, LV_OBJ_FLAG_FLOATING);
    // >
    lv_obj_add_event_cb(btn_clear, clear_log_event_cb, LV_EVENT_CLICKED, log_ta);

    lv_textarea_set_text(log_ta, "VNC Viewer started!\n");

    // Last Row: Buttons
    lv_obj_t * bottom_container = lv_obj_create(layer);
    lv_obj_set_size(bottom_container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(bottom_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_flex_main_place(bottom_container, LV_FLEX_ALIGN_SPACE_BETWEEN, 0); // LV_FLEX_ALIGN_END
    lv_obj_set_style_bg_opa(bottom_container, LV_OPA_0, 0);
    lv_obj_set_style_border_width(bottom_container, 0, 0);
    lv_obj_set_style_pad_all(bottom_container, 0, 0);
    lv_obj_set_style_pad_gap(bottom_container, 12, 0);

    // Button: WIFI Setting
    lv_obj_t * btn_wifi = lv_button_create(bottom_container); // v9: lv_btn_create -> lv_button_create
    lv_obj_set_flex_flow(btn_wifi, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(btn_wifi, 12, 0);
    lv_obj_add_event_cb(btn_wifi, vnc_event_handler, LV_EVENT_CLICKED, (void *)EVT_SHOW_WIFI);

    lv_obj_t * wifi_icon = lv_image_create(btn_wifi);
    lv_image_set_src(wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_t * wifi_label = lv_label_create(btn_wifi);
    lv_label_set_text(wifi_label, "Setting");

    // Button: Connect To Server
    lv_obj_t * btn_connect = lv_button_create(bottom_container);
    lv_obj_set_flex_flow(btn_connect, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(btn_connect, 12, 0);
    lv_obj_add_event_cb(btn_connect, vnc_event_handler, LV_EVENT_CLICKED, (void *)EVT_SHOW_CONN);

    lv_obj_t * connect_icon = lv_image_create(btn_connect);
    lv_image_set_src(connect_icon, LV_SYMBOL_PLAY);
    lv_obj_t * connect_label = lv_label_create(btn_connect);
    lv_label_set_text(connect_label, "Connect");
}

static void vnc_create_layer_wifi(vnc_screen_t* scrn)
{
    lv_obj_t* layer = scrn->layer_wifi = lv_obj_create(scrn->active_scrn);

    lv_obj_remove_style_all(layer);
    //lv_obj_set_size(layer, scrn->disp_width, scrn->disp_height);
    //lv_obj_set_pos(layer, -scrn->disp_width, 0);
    lv_obj_set_size(layer, LV_PCT(100), LV_PCT(100));
    lv_obj_align(layer, LV_ALIGN_TOP_LEFT, LV_PCT(-100), 0);
    lv_obj_set_style_bg_opa(layer, LV_OPA_0, 0);
    lv_obj_set_style_border_opa(layer, LV_OPA_0, 0);
    /*
    lv_obj_set_flex_flow(layer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(layer, 15, 0);
    lv_obj_set_style_pad_gap(layer, 15, 0);
    */

    lv_obj_t * btn = lv_button_create(layer);
    lv_obj_set_size(btn, 150, 60);
    lv_obj_align(btn, LV_ALIGN_CENTER, -100, 0);
    lv_obj_add_event_cb(btn, vnc_event_handler, LV_EVENT_CLICKED, (void*)EVT_HIDE_WIFI);

    lv_obj_t * label = lv_label_create(btn);
    lv_label_set_text(label, "Go Back");
    lv_obj_center(label);
}

static void vnc_create_layer_conn(vnc_screen_t* scrn)
{
    lv_obj_t* layer = scrn->layer_conn = lv_obj_create(scrn->active_scrn);

    lv_obj_remove_style_all(layer);
    //lv_obj_set_size(layer, scrn->disp_width, scrn->disp_height);
    //lv_obj_set_pos(layer, scrn->disp_width, 0);
    lv_obj_set_size(layer, LV_PCT(100), LV_PCT(100));
    lv_obj_align(layer, LV_ALIGN_TOP_RIGHT, LV_PCT(100), 0);
    lv_obj_set_style_bg_opa(layer, LV_OPA_0, 0);
    lv_obj_set_style_border_opa(layer, LV_OPA_0, 0);
    /*
    lv_obj_set_flex_flow(layer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(layer, 15, 0);
    lv_obj_set_style_pad_gap(layer, 15, 0);
    */

    lv_obj_t * btn = lv_button_create(layer);
    lv_obj_set_size(btn, 150, 60);
    lv_obj_align(btn, LV_ALIGN_CENTER, -100, 0);
    lv_obj_add_event_cb(btn, vnc_event_handler, LV_EVENT_CLICKED, (void*)EVT_HIDE_CONN);

    lv_obj_t * label = lv_label_create(btn);
    lv_label_set_text(label, "Go Back");
    lv_obj_center(label);
} 


/**
 * 
 * 
 */

void vnc_screen_init(vnc_display_t* vnc_disp)
{
    ESP_LOGI(TAG, "Initialize VNC screen...");
    if (vnc_display_lock(true)) 
    {
        //
        vnc_scrn.active_scrn = lv_screen_active(); // v9: lv_scr_act() 대신 사용 권장
        vnc_scrn.disp_width = lv_display_get_horizontal_resolution(NULL); // lv_obj_get_width(lv_screen_active())
        vnc_scrn.disp_height = lv_display_get_vertical_resolution(NULL); // lv_obj_get_height(lv_screen_active())

        lv_obj_clean(vnc_scrn.active_scrn);
        lv_obj_set_style_bg_color(vnc_scrn.active_scrn, lv_color_hex(0x0E0E1E), 0);

        vnc_create_layer_canvas(&vnc_scrn);
        vnc_create_layer_main(&vnc_scrn);
        vnc_create_layer_wifi(&vnc_scrn);
        vnc_create_layer_conn(&vnc_scrn);

        /*
         * 9.6 --> 9.5
         * 
        lv_obj_set_scrollable(vnc_scrn.active_scrn, false);
        */
        lv_obj_remove_flag(vnc_scrn.active_scrn, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_add_event_cb(vnc_scrn.active_scrn, vnc_size_changed, LV_EVENT_SIZE_CHANGED, 0);
        
        //
        vnc_display_lock(false);
    }
}


/**
 * @brief Application entry point
 */

void app_main(void) {

    //
    vnc_display_start(vnc_screen_init);

    //
    bsp_display_brightness_set(30);  
}
