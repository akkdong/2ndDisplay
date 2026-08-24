// vnc_screen.c
//

#include <stdarg.h>
#include <stdlib.h>
#include <memory.h>
#include "esp_log.h"
#include "vnc_screen.h"
#include "vnc_connect_popup.h"
#include "wifi_setting_popup.h"

#include "app_main.h"




//
//
//

static const char* TAG = "VNC_Scrn";

static vnc_screen_t vnc_screen;

static char log_buf[96];


LV_IMG_DECLARE(vnc_logo);






/**
 *
 *
 */


static void clear_log_event_cb(lv_event_t* e)
{
    lv_obj_t* log_ta = (lv_obj_t*)lv_event_get_user_data(e);
    if (log_ta)
        lv_textarea_set_text(log_ta, "");
}

static void append_log_with_limit(lv_obj_t* ta, const char* new_text)
{
    // first append log
    lv_textarea_add_text(ta, new_text);

    // 
    const char* current_text = lv_textarea_get_text(ta);
    if (current_text == NULL) 
        return;

    // count lines
    int line_count = 0;
    for (int i = 0; current_text[i] != '\0'; i++) 
    {
        if (current_text[i] == '\n')
            line_count++;
    }

    // check limit
    if (line_count > VNC_MAX_LOGS)
    {
        int lines_to_remove = line_count - VNC_MAX_LOGS;
        int remove_index = 0;
        int found_lines = 0;

        // remove overflow lines
        for (int i = 0; current_text[i] != '\0'; i++) 
        {
            if (current_text[i] == '\n') 
            {
                found_lines++;
                if (found_lines == lines_to_remove) 
                {
                    remove_index = i + 1;
                    break;
                }
            }
        }

        // update by removed text
        if (remove_index > 0) 
            lv_textarea_set_text(ta, &current_text[remove_index]);
    }

    // move cursor to the end
    lv_textarea_set_cursor_pos(ta, LV_TEXTAREA_CURSOR_LAST);
}



static void vnc_handler_on_connect(vnc_screen_t* scrn, const char* addr, uint16_t port, const char* pass)
{
    if (scrn && scrn->app && scrn->app->connect_server)
        scrn->app->connect_server(scrn->app, addr, port, pass);
}

static void on_clicked_wifi(lv_event_t* evt)
{
    lv_obj_t* button = (lv_obj_t*)lv_event_get_target(evt);
    vnc_screen_t* scrn = (vnc_screen_t*)lv_event_get_user_data(evt);

    /*
    wnc_wifi_popup_t* popup = vnc_wifi_popup_init(scrn);
    if (popup)
        popup->show_popup();
    */

    show_wifi_setting_popup();
}

static void on_clicked_connect(lv_event_t* evt)
{
    lv_obj_t* button = (lv_obj_t*)lv_event_get_target(evt);
    vnc_screen_t* scrn = (vnc_screen_t*)lv_event_get_user_data(evt);

    vnc_connect_popup_t* popup = vnc_connect_popup_init(scrn, vnc_handler_on_connect);
    if (popup)
    {
        if (scrn->app->get_server_info)
            scrn->app->get_server_info(scrn->app, popup->address, &popup->port, popup->password);

        popup->show_popup(popup);
    }
}


static void vnc_size_changed(lv_event_t* evt)
{

}


static void vnc_handler_hide_timer(lv_timer_t* timer)
{
    vnc_screen_t* scrn = (vnc_screen_t*)lv_timer_get_user_data(timer);

    if (lv_obj_is_valid(scrn->btn_disconnect))
        lv_obj_fade_out(scrn->btn_disconnect, 500, 0);

    lv_timer_del(timer);
}

static void vnc_handler_on_click_canvas(lv_event_t* evt)
{
    lv_event_code_t code = lv_event_get_code(evt);
    vnc_screen_t* scrn = (vnc_screen_t*)lv_event_get_user_data(evt);

    if (code == LV_EVENT_CLICKED && lv_obj_is_valid(scrn->btn_disconnect))
    {
        //bool is_visible = lv_obj_is_visible(scrn->btn_disconnect);
        //if (!is_visible)
        {
            lv_obj_fade_in(scrn->btn_disconnect, 500, 0);
            lv_timer_create(vnc_handler_hide_timer, 10000, scrn);
        }
    }
}

static void vnc_handler_on_disconnect(lv_event_t* evt)
{
    ESP_LOGI(TAG, "*");
    ESP_LOGI(TAG, "* vnc_handler_on_disconnect");
    ESP_LOGI(TAG, "*");

    vnc_screen_t* scrn = (vnc_screen_t*)lv_event_get_user_data(evt);
    vnc_app_send_event(scrn->app, VNC_DISCONNECT_SERVER, 0, 0, 0);
}



//
// canvas: bottom layer
//
static lv_obj_t* vnc_create_layer_canvas(vnc_screen_t* scrn, lv_obj_t* parent)
{
    lv_obj_t* canvas = lv_canvas_create(parent);
    lv_obj_set_user_data(canvas, scrn);
    //
    // ...
    // 
    lv_obj_add_flag(canvas, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(canvas, vnc_handler_on_click_canvas, LV_EVENT_CLICKED, scrn);

    //
    lv_obj_t* exit = lv_obj_create(canvas);
    lv_obj_set_user_data(exit, scrn);
    lv_obj_set_size(exit, 48, 48);
    lv_obj_align(exit, LV_ALIGN_BOTTOM_RIGHT, -12, -12);
    lv_obj_set_style_radius(exit, LV_RADIUS_CIRCLE, 0);

    lv_obj_t* label = lv_label_create(exit);
    lv_label_set_text(label, LV_SYMBOL_STOP);
    lv_obj_center(label);

    lv_obj_add_flag(exit, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(exit, vnc_handler_on_disconnect, LV_EVENT_CLICKED, scrn);


    scrn->btn_disconnect = exit;
    lv_timer_create(vnc_handler_hide_timer, 10000, scrn);

    return canvas;
}

//
// main ui
//
static lv_obj_t* vnc_create_layer_main(vnc_screen_t* scrn, lv_obj_t* parent)
{
    lv_obj_t* layer =  lv_obj_create(parent);

    lv_obj_remove_style_all(layer);
    //lv_obj_set_style_bg_opa(layer, LV_OPA_0, 0);
    //lv_obj_set_style_border_opa(layer, LV_OPA_0, 0);
    lv_obj_set_flex_flow(layer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_size(layer, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(layer, 15, 0);
    lv_obj_set_style_pad_gap(layer, 15, 0);

    // First Row: Logo & Title, Program Information
    lv_obj_t* top_header = lv_obj_create(layer);
    lv_obj_set_size(top_header, lv_pct(100), LV_SIZE_CONTENT);
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
    lv_obj_t* logo_img = lv_image_create(top_header);
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
    lv_obj_set_size(label_title, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_text_color(label_title, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_title, &lv_font_montserrat_32, 0);
    // program information
    lv_obj_t* label_info = lv_label_create(text_box);
    lv_label_set_text(label_info, "\nVersion 1.0.0 (Alpha)\nAll rights is reserved");
    lv_obj_set_size(label_info, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_text_color(label_info, lv_color_hex(0x888888), 0);

    // Middle Content: System Status Log
    lv_obj_t* log_ta = lv_textarea_create(layer);
    lv_obj_set_name(log_ta, "app_log");
    lv_textarea_set_cursor_click_pos(log_ta, false);
    //lv_obj_set_clickable(log_ta, false);
    lv_obj_add_state(log_ta, LV_STATE_DISABLED);
    lv_obj_set_width(log_ta, lv_pct(100));
    lv_obj_set_flex_grow(log_ta, 1);
    lv_obj_set_style_bg_opa(log_ta, LV_OPA_10, 0);
    lv_obj_set_style_border_opa(log_ta, LV_OPA_60, 0);
    lv_obj_set_style_bg_color(log_ta, lv_color_hex(0xC0C0C0), 0);
    lv_obj_set_style_text_color(log_ta, lv_color_white(), 0);
    lv_obj_set_style_border_color(log_ta, lv_color_hex(0x288CF4), 0);

    // Clear Log Button
    lv_obj_t* btn_clear = lv_button_create(log_ta);
    lv_obj_t* clear_icon = lv_image_create(btn_clear);
    lv_image_set_src(clear_icon, LV_SYMBOL_REFRESH);
    /*
    lv_obj_align_to(btn_clear, log_ta, LV_ALIGN_BOTTOM_RIGHT, -12, -12);
    */
    lv_obj_align(btn_clear, LV_ALIGN_BOTTOM_RIGHT, -12, -12);
    lv_obj_set_style_opa(btn_clear, 120, LV_PART_MAIN);
    // <<<<<<
    /*
     * 9.6 --> 9.5
     *
    lv_obj_set_scroll_chain(btn_clear, false);
    lv_obj_set_floating(btn_clear, true);
    */
    // ======
    lv_obj_remove_flag(btn_clear, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_add_flag(btn_clear, LV_OBJ_FLAG_FLOATING);
    // >>>>>>
    lv_obj_add_event_cb(btn_clear, clear_log_event_cb, LV_EVENT_CLICKED, log_ta);

    lv_textarea_set_text(log_ta, "VNC Viewer started!\n");

    // Last Row: State Lable & Buttons
    lv_obj_t* bottom_container = lv_obj_create(layer);
    lv_obj_set_size(bottom_container, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(bottom_container, LV_FLEX_FLOW_ROW);
    //lv_obj_set_style_flex_main_place(bottom_container, LV_FLEX_ALIGN_SPACE_BETWEEN, 0); // LV_FLEX_ALIGN_END
    lv_obj_set_flex_align(bottom_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(bottom_container, LV_OPA_0, 0);
    lv_obj_set_style_border_width(bottom_container, 0, 0);
    lv_obj_set_style_pad_all(bottom_container, 0, 0);
    lv_obj_set_style_pad_gap(bottom_container, 12, 0);
    lv_obj_set_style_pad_column(bottom_container, 8, 0);

    // Label: Application State
    lv_obj_t* label_box = lv_obj_create(bottom_container);
    lv_obj_remove_style_all(label_box);
    lv_obj_set_size(label_box, LV_SIZE_CONTENT, lv_pct(100));
    lv_obj_set_style_bg_opa(label_box, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(label_box, lv_color_hex(0x222222), 0);
    lv_obj_set_style_radius(label_box, 6, 0);
    lv_obj_set_layout(label_box, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(label_box, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(label_box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_flex_grow(label_box, 1);
    lv_obj_set_style_pad_left(label_box, 6, 0);
    lv_obj_set_style_pad_right(label_box, 6, 0);

    lv_obj_t* label = lv_label_create(label_box);
    lv_obj_set_name(label, "app_state");
    lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
    lv_label_set_text(label, "Initializing...");


    // Button: WIFI Setting
    lv_obj_t* btn_wifi = lv_button_create(bottom_container); // v9: lv_btn_create -> lv_button_create
    lv_obj_set_flex_flow(btn_wifi, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(btn_wifi, 12, 0);
    lv_obj_add_event_cb(btn_wifi, on_clicked_wifi, LV_EVENT_CLICKED, scrn);

    lv_obj_t* wifi_icon = lv_image_create(btn_wifi);
    lv_image_set_src(wifi_icon, LV_SYMBOL_WIFI);

    // Button: Connect To Server
    lv_obj_t* btn_connect = lv_button_create(bottom_container);
    lv_obj_set_name(btn_connect, "btn_connect");
    lv_obj_set_flex_flow(btn_connect, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(btn_connect, 12, 0);
    lv_obj_add_event_cb(btn_connect, on_clicked_connect, LV_EVENT_CLICKED, scrn);
    /*
    lv_obj_add_state(btn_connect, LV_STATE_DISABLED);
    */

    lv_obj_t* connect_icon = lv_image_create(btn_connect);
    lv_image_set_src(connect_icon, LV_SYMBOL_PLAY);

    return layer;
}



/**
 *
 *
 */
vnc_screen_t* vnc_screen_init(vnc_app_t* app/*vnc_display_t* disp*/)
{
    ESP_LOGI(TAG, "[S] Initialize VNC screen...");

    vnc_display_t* disp = app->disp;
    vnc_screen_t* scrn = &vnc_screen;

    //
    scrn->disp_handle = disp;
    scrn->disp_width = disp->disp_width;
    scrn->disp_height = disp->disp_height;
    //
    scrn->app = app;

    //
    scrn->layer_canvas = NULL;
    scrn->layer_main = NULL;
#if VNC_CACHE_OBJECTS
    scrn->obj_logs = NULL;
    scrn->obj_state = NULL;
    scrn->btn_connect = NULL;
#endif
    scrn->disp_buf = NULL;

    //
    scrn->create = vnc_screen_create;
    scrn->update_state = vnc_screen_update_state;
    scrn->append_log = vnc_log_append;
    scrn->printf_log = vnc_log_printf;
    scrn->empty_log = vnc_log_clear;

    ESP_LOGI(TAG, "[E] Initialize VNC screen...");

    return &vnc_screen;
}


/**
 *
 */
void vnc_screen_create(vnc_screen_t* scrn)
{
    ESP_LOGI(TAG, "[S] Create VNC screen...");

    if (vnc_display_lock(scrn->disp_handle))
    {
        //
        /*
        lv_display_set_rotation(vnc_disp->disp_handle, LV_DISPLAY_ROTATION_90);
        */

        // Get currently active display
        lv_display_t* disp = lv_display_get_default();

        // Reset the theme to dark mode (Dark theme if the 4th argument is true)
        lv_theme_t* th = lv_theme_default_init(disp,
            lv_palette_main(LV_PALETTE_BLUE),
            lv_palette_main(LV_PALETTE_GREEN),
            true,
            LV_FONT_DEFAULT);

        // Apply dark theme
        lv_display_set_theme(disp, th);


        //
        lv_obj_t* active = lv_screen_active();
#if 0
        // <<<<<<
        /*
        vnc_screen.disp_width = lv_display_get_horizontal_resolution(NULL);
        vnc_screen.disp_height = lv_display_get_vertical_resolution(NULL);
        */
        // ======
        vnc_screen.disp_width = lv_obj_get_width(active);
        vnc_screen.disp_height = lv_obj_get_height(active);
        // >>>>>>
        ESP_LOGI(TAG, "Screen Dimension: %d x %d", vnc_screen.disp_width, vnc_screen.disp_height);
#endif

        lv_obj_clean(active);
        lv_obj_set_style_bg_color(active, lv_color_hex(0x0E0E1E), 0);

        scrn->layer_main = vnc_create_layer_main(scrn, active);
        scrn->layer_canvas = vnc_create_layer_canvas(scrn, active);
#if VNC_CACHE_OBJECTS
        scrn->obj_logs = lv_obj_find_by_name(active, "app_log");
        scrn->obj_state = lv_obj_find_by_name(active, "app_state");
        scrn->btn_connect = lv_obj_find_by_name(active, "btn_connect");
#endif
        lv_obj_add_flag(scrn->layer_canvas, LV_OBJ_FLAG_HIDDEN);

        lv_obj_add_event_cb(active, vnc_size_changed, LV_EVENT_SIZE_CHANGED, 0);

        //
        vnc_display_unlock(scrn->disp_handle);
    }

    ESP_LOGI(TAG, "[E] Create VNC screen...");
}



/**
 *
 */
vnc_screen_t* vnc_screen_get_handle()
{
    return &vnc_screen;
}



/**
 *
 */
bool vnc_screen_start_play(vnc_screen_t* scrn, int width, int height, int bpp)
{
    vnc_display_lock(scrn->disp_handle);
    {
        if (scrn->disp_buf)
            heap_caps_free(scrn->disp_buf);

        scrn->disp_buf = heap_caps_malloc(width * height * bpp, MALLOC_CAP_SPIRAM);
        if (scrn->disp_buf)
        {
            uint8_t* ptr = scrn->disp_buf;
            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < width; ++x)
                {
                    ptr[0] = 0xFF;
                    ptr[1] = 0x00;
                    ptr[2] = 0x00;
                    ptr[3] = (uint8_t)(255 * y / height);

                    ptr += 4;
                }
            }

            lv_canvas_set_buffer(scrn->layer_canvas, scrn->disp_buf, width, height, LV_COLOR_FORMAT_ARGB8888);
        }

        lv_obj_clear_flag(scrn->layer_canvas, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(scrn->layer_main, LV_OBJ_FLAG_HIDDEN);
    }
    vnc_display_unlock(scrn->disp_handle);

    return true;
}

/**
 *
 */
void vnc_screen_stop_play(vnc_screen_t* scrn)
{
    vnc_display_lock(scrn->disp_handle);
    {
        if (scrn->disp_buf)
        {
            //ESP_LOGI(TAG, "Clear Canvas");
            //lv_canvas_fill_bg(scrn->layer_canvas, lv_color_white(), LV_OPA_COVER);
            //ESP_LOGI(TAG, "Clear Canvas Buffer");
            //lv_canvas_set_buffer(scrn->layer_canvas, NULL, 0, 0, LV_COLOR_FORMAT_ARGB8888);

            //heap_caps_free(scrn->disp_buf);
            //scrn->disp_buf = NULL;
        }

        lv_obj_clear_flag(scrn->layer_main, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(scrn->layer_canvas, LV_OBJ_FLAG_HIDDEN);
    }
    vnc_display_unlock(scrn->disp_handle);
}


/**
 *
 */
void vnc_screen_publish_frame(vnc_screen_t* scrn, uint8_t* buf, uint32_t size)
{
    vnc_display_lock(scrn->disp_handle);
    {
        if (scrn->disp_buf)
        {
            memcpy(scrn->disp_buf, buf, size);

            lv_obj_invalidate(scrn->layer_canvas);
        }
    }
    vnc_display_unlock(scrn->disp_handle);
}



/**
 *
 */
void vnc_screen_update_state(vnc_screen_t* scrn, uint32_t state, uint32_t action)
{
    vnc_display_lock(scrn->disp_handle);
    {
#if VNC_CACHE_OBJECTS
        lv_obj_t* label = scrn->obj_state;
#else
        lv_obj_t* label = lv_obj_find_by_name(lv_screen_active(), "app_state");
#endif
        char text[32];

        if (label)
        {

            switch (state)
            {
            case APP_STATE_INIT:
                strcpy(text, "Initializing...");
                break;
            case APP_STATE_STANDBY:
                strcpy(text, "Standby");
                break;
            case APP_STATE_READY:
                strcpy(text, "Ready");
                break;
            case APP_STATE_PLAY:
                strcpy(text, "Play");
                break;
            }

            if (action != APP_ACTION_NONE)
            {
                strcat(text, ": ");
                switch (action)
                {
                case APP_ACTION_NETIF_UP:
                    strcat(text, "NetIf Up");
                    break;
                case APP_ACTION_WIFI_CONNECT:
                    strcat(text, "NetIf Down");
                    break;
                case APP_ACTION_VNC_CONNECT:
                    strcat(text, "Connect Server");
                    break;
                case APP_ACTION_VNC_DISCONNECT:
                    strcat(text, "Disconnect Server");
                    break;
                }
            }

            lv_label_set_text(label, text);
            /*
            if (scrn->btn_connect)
                lv_obj_add_state(scrn->btn_connect, state == APP_STATE_READY ? LV_STATE_DEFAULT : LV_STATE_DISABLED);
            */
        }
    }
    vnc_display_unlock(scrn->disp_handle);
}


/**
 *
 */
void vnc_log_append(vnc_screen_t* scrn, const char* text)
{
    vnc_display_lock(scrn->disp_handle);
    {
#if VNC_CACHE_OBJECTS
        lv_obj_t* ta = scrn->obj_logs;
#else
        lv_obj_t* ta = lv_obj_find_by_name(lv_screen_active(), "app_log");
#endif
        if (ta)
            append_log_with_limit(ta, text);
    }
    vnc_display_unlock(scrn->disp_handle);
}

/**
 *
 */
void vnc_log_printf(vnc_screen_t* scrn, const char* format, ...)
{
    va_list args;

    va_start(args, format);
    vsnprintf(log_buf, sizeof(log_buf) - 1, format, args);
    va_end(args);

    vnc_log_append(scrn, log_buf);
}

/**
 *
 */
void vnc_log_clear(vnc_screen_t* scrn)
{
    vnc_display_lock(scrn->disp_handle);
    {
#if VNC_CACHE_OBJECTS
        lv_obj_t* ta = scrn->obj_logs;
#else
        lv_obj_t* ta = lv_obj_find_by_name(lv_screen_active(), "app_log");
#endif
        if (ta)
            lv_textarea_set_text(ta, "");
    }
    vnc_display_unlock(scrn->disp_handle);
}
