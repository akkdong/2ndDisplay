// VncScreen.cpp
//

#include <stdlib.h>
#include <string.h>
#include "VncScreen.h"
#include "VncApplication.h"


#ifdef __cplusplus
extern "C" {
#endif

LV_IMG_DECLARE(vnc_logo);

#ifdef __cplusplus
}
#endif



//
//
//

Uint32 VncScreen::EventType = -1;


VncScreen::VncScreen(VncApplication* app)
    : m_addr("")
    , m_port(5900)
    , m_pass("")
    , m_app(app)
    , m_active(NULL)
    , m_mainLayer(NULL)
    , m_canvas(NULL)
    , m_keyboard(NULL)
    , m_canvasBuf(NULL)
{
    //
    if (EventType == -1)
        EventType = SDL_RegisterEvents(1);

    // Apply dark theme
    lv_display_t* disp = lv_display_get_default();

    // Reset the theme to dark mode (Dark theme if the 4th argument is true)
    lv_theme_t* theme = lv_theme_default_init(disp,
        lv_palette_main(LV_PALETTE_BLUE),
        lv_palette_main(LV_PALETTE_GREEN),
        true,
        LV_FONT_DEFAULT);

    lv_display_set_theme(disp, theme);


    //
    m_active = lv_screen_active();

    lv_obj_clean(m_active);
    lv_obj_set_style_bg_color(m_active, lv_color_hex(0x1E1E2E), 0);

    m_mainLayer = createMainLayer(this, m_active);
    m_canvas = createCanvasLayer(this, m_active);
    /*
    m_keyboard = createKeyboard(this);
    */

    lv_obj_add_flag(m_canvas, LV_OBJ_FLAG_HIDDEN);
}

VncScreen::~VncScreen()
{
}


void VncScreen::setConnectInfo(const char* addr, unsigned short port, const char* pass)
{    
    if (addr && addr[0])
        strcpy(m_addr, addr);
    if (port > 0)
        m_port = port;
    if (pass && pass[0])
        strcpy(m_pass, pass);
    //LV_LOG("set connect-info: %s, %d, %s\n", m_addr, m_port, m_pass);

    if (m_active)
    {
        //
        lv_obj_t* obj_addr = lv_obj_find_by_name(m_active, "server_addr");
        if (obj_addr && m_addr && m_addr[0])
            lv_textarea_set_text(obj_addr, m_addr);

        //
        lv_obj_t* obj_port = lv_obj_find_by_name(m_active, "server_port");
        if (obj_port && m_port > 0)
        {
            char port_str[16];
            itoa(m_port, port_str/*, sizeof(port_str)*/, 10);
            lv_textarea_set_text(obj_port, port_str);
        }

        //
        lv_obj_t* obj_pass = lv_obj_find_by_name(m_active, "server_pass");
        if (obj_pass && m_pass && m_pass[0])
            lv_textarea_set_text(obj_pass, m_pass);
    }
}

void VncScreen::startPlay(int width, int height, int bpp)
{
    if (m_canvasBuf)
        free(m_canvasBuf);

    m_canvasBuf = (uint8_t *)malloc(width * height * bpp);
    if (m_canvasBuf)
    {
        uint8_t* ptr = m_canvasBuf;
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

        lv_canvas_set_buffer(m_canvas, m_canvasBuf, width, height, LV_COLOR_FORMAT_ARGB8888);
    }

    lv_obj_clear_flag(m_canvas, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(m_mainLayer, LV_OBJ_FLAG_HIDDEN);
}

void VncScreen::startPlay()
{
    // 
}

void VncScreen::stopPlay()
{
    if (m_canvasBuf)
    {

    }

    lv_obj_clear_flag(m_mainLayer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(m_canvas, LV_OBJ_FLAG_HIDDEN);
}


void VncScreen::refreshFrame(uint32_t* fb, size_t size)
{
    if (m_canvasBuf)
    {
        memcpy(m_canvasBuf, fb, size);

        lv_obj_invalidate(m_canvas);
    }
}

void VncScreen::pointInputCallback(lv_event_t* evt)
{
    lv_event_code_t code = lv_event_get_code(evt);
    vnc_screen_t* scrn = (vnc_screen_t *)lv_event_get_user_data(evt);
    if (code == LV_EVENT_PRESSED)
    {
        lv_obj_t* obj_active = lv_indev_get_active_obj();
        if (scrn->m_keyboard != NULL && !lv_obj_has_flag(scrn->m_keyboard, LV_OBJ_FLAG_HIDDEN))
        {
            if (scrn->m_keyboard == obj_active || scrn->m_keyboard == lv_obj_get_parent(obj_active))
                return;

            lv_obj_add_flag(scrn->m_keyboard, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

bool VncScreen::extractAndValidate()
{
    // extract & convert information
    lv_obj_t* obj_addr = lv_obj_find_by_name(m_active, "server_addr");
    if (obj_addr)
        strcpy(m_addr/*, sizeof(m_addr)*/, lv_textarea_get_text(obj_addr));
    lv_obj_t* obj_port = lv_obj_find_by_name(m_active, "server_port");
    if (obj_port)
        m_port = atoi(lv_textarea_get_text(obj_port));
    lv_obj_t* obj_pass = lv_obj_find_by_name(m_active, "server_pass");
    if (obj_pass)
        strcpy(m_pass/*, sizeof(m_pass)*/, lv_textarea_get_text(obj_pass));

    if (!m_addr[0])
        return false;
    if (m_port == 0)
        return false;    

    LV_LOG("Validate: %s, %d, %s\n", m_addr, m_port, m_pass);
    return true;
}

static lv_color_t get_default_button_color(void)
{
    lv_obj_t * scr = lv_display_get_screen_active(NULL);
    lv_obj_t * dummy_btn = lv_button_create(scr);
    lv_color_t default_color = lv_obj_get_style_bg_color(dummy_btn, LV_PART_MAIN);

    lv_obj_delete(dummy_btn);

    return default_color;
}

static void anim_ready_cb(lv_anim_t * a)
{
    lv_obj_t * obj = (lv_obj_t *)a->var;
    lv_obj_set_style_translate_x(obj, 0, 0);
    lv_obj_set_style_bg_color(obj, get_default_button_color(), 0);
}

static void custom_shake_exec_cb(void * var, int32_t v)
{
    lv_obj_t * obj = (lv_obj_t *)var;
    lv_obj_set_style_translate_x(obj, v, 0);
    lv_obj_invalidate(obj);
}

void VncScreen::shakeButton(lv_obj_t* btn)
{
    if(btn == NULL) 
        return;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, btn);

    // Specify the custom callback created above instead of the standard `lv_obj_set_style_translate_x`
    lv_anim_set_exec_cb(&a, custom_shake_exec_cb);

    lv_anim_set_values(&a, -2, 2);
    lv_anim_set_duration(&a, 50);
    lv_anim_set_repeat_count(&a, 2);
    lv_anim_set_playback_duration(&a, 50);
    lv_anim_set_path_cb(&a, lv_anim_path_linear);
    lv_anim_set_completed_cb(&a, anim_ready_cb);

    lv_anim_start(&a);
}

lv_obj_t* VncScreen::createMainLayer(vnc_screen_t* scrn, lv_obj_t* parent)
{
    lv_obj_t* layer =  lv_obj_create(parent);

    lv_obj_remove_style_all(layer);
    //lv_obj_set_style_bg_opa(layer, LV_OPA_0, 0);
    //lv_obj_set_style_border_opa(layer, LV_OPA_0, 0);
    lv_obj_set_flex_flow(layer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_size(layer, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_all(layer, 10, 0);
    lv_obj_set_style_pad_gap(layer, 10, 0);

    // First Row: Logo & Title, Program Information
    lv_obj_t* top_header = lv_obj_create(layer);
    lv_obj_set_size(top_header, lv_pct(100), LV_SIZE_CONTENT);
    /*
    lv_obj_remove_style_all(top_header);
    */
    lv_obj_set_style_bg_opa(top_header, LV_OPA_0, 0);
    lv_obj_set_style_border_opa(top_header, LV_OPA_0, 0);
    lv_obj_set_layout(top_header, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(top_header, LV_FLEX_FLOW_ROW); // horizontal align
    //lv_obj_set_style_flex_cross_place(top_header, LV_FLEX_ALIGN_START, 0);
    lv_obj_set_style_pad_all(top_header, 0, 0);
    lv_obj_set_style_pad_column(top_header, 12, 0); // space in logo and text

    // logo image
    lv_obj_t* logo_img = lv_image_create(top_header);
    if (logo_img)
    {
        lv_image_set_src(logo_img, &vnc_logo);
        lv_image_set_scale(logo_img, 132);
        lv_obj_set_size(logo_img, 160, 160);
    }

    // title & program information
    lv_obj_t* text_box = lv_obj_create(top_header);
    if (text_box)
    {
        lv_obj_remove_style_all(text_box);
        lv_obj_set_height(text_box, lv_pct(100));
        lv_obj_set_flex_grow(text_box, 3);
        lv_obj_set_layout(text_box, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(text_box, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(text_box, 0, 0);
        lv_obj_set_style_pad_row(text_box, 4, 0);

        // title
        lv_obj_t* label_title = lv_label_create(text_box);
        lv_label_set_text(label_title, "VNC Viewer");
        //lv_obj_set_size(label_title, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_grow(label_title, 1);
        lv_obj_set_style_text_color(label_title, lv_color_white(), 0);
        lv_obj_set_style_text_font(label_title, &lv_font_montserrat_32, 0);
        // program information
        lv_obj_t* label_info = lv_label_create(text_box);
        lv_label_set_text(label_info, "\nVersion 1.0.0 (Alpha)\nAll rights is reserved");
        lv_obj_set_size(label_info, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_style_text_color(label_info, lv_color_hex(0x888888), 0);
    }

    // 
    lv_obj_t* target_box = lv_obj_create(top_header);
    if (target_box)
    {
        lv_obj_remove_style_all(target_box);
        lv_obj_set_name(target_box, "connect_panel");
        lv_obj_clear_flag(target_box, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_height(target_box, lv_pct(100));
        lv_obj_set_flex_grow(target_box, 6); 
        lv_obj_set_layout(target_box, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(target_box, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(target_box, 0, 0);
        //lv_obj_set_style_pad_row(target_box, 6, 0);

        lv_obj_set_style_bg_opa(target_box, LV_OPA_60, 0);
        lv_obj_set_style_bg_color(target_box, lv_color_hex(0x131722), 0);
        lv_obj_set_style_radius(target_box, 8, 0);
        /*
        lv_obj_set_style_text_color(target_box, lv_color_white(), 0);
        //lv_obj_set_style_border_color(target_box, lv_color_hex(0x288CF4), 0);
        //lv_obj_set_style_border_width(target_box, 2, 0);
        lv_obj_set_style_radius(target_box, 6, 0);
        */

        // title
        lv_obj_t* label_title = lv_label_create(target_box);
        lv_label_set_text(label_title, "Connection");
        lv_obj_set_size(label_title, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(label_title, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(label_title, lv_color_hex(0x394470), 0);
        lv_obj_set_style_text_color(label_title, lv_color_white(), 0);
        lv_obj_set_style_text_font(label_title, &lv_font_montserrat_20, 0);
        lv_obj_set_style_radius(label_title, 0, 0);
        lv_obj_set_style_pad_all(label_title, 8, 0);
        lv_obj_set_style_pad_top(label_title, 10, 0);
        lv_obj_set_style_pad_bottom(label_title, 10, 0);
        lv_obj_set_style_margin_bottom(label_title, 4, 0);

        // Connect Information
        lv_obj_t* row1 = lv_obj_create(target_box);
        lv_obj_remove_style_all(row1);
        lv_obj_set_size(row1, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_layout(row1, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(row1, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row1, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_bg_opa(row1, LV_OPA_TRANSP, 0);
        lv_obj_set_style_margin_all(row1, 0, 0);
        lv_obj_set_style_pad_all(row1, 8, 0);
        lv_obj_set_style_pad_column(row1, 8, 0);
        {
            lv_obj_t* label = lv_label_create(row1);
            lv_label_set_text(label, "Server");
            lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
            lv_obj_set_flex_grow(label, 2);
            lv_obj_set_height(label, LV_SIZE_CONTENT);

            lv_obj_t* addr = lv_textarea_create(row1);
            lv_textarea_set_one_line(addr, true);
            lv_textarea_set_max_length(addr, 16);
            lv_textarea_set_placeholder_text(addr, "192.168.100.2");
            lv_obj_set_name(addr, "server_addr");
            lv_obj_set_flex_grow(addr, 5);
            lv_obj_set_height(addr, LV_SIZE_CONTENT);
            lv_obj_set_style_pad_right(addr, 6, 0);
            lv_obj_set_style_bg_color(addr, lv_color_hex(0x222222), 0);
            lv_obj_set_style_text_color(addr, lv_color_hex(0xE0E0E0), 0);
            lv_obj_set_style_text_color(addr, lv_color_hex(0x444444), LV_PART_TEXTAREA_PLACEHOLDER);
            lv_obj_set_style_border_color(addr, lv_color_hex(0x888888), LV_PART_CURSOR);
            lv_obj_add_event_cb(addr, onEditEvent, LV_EVENT_ALL, this);

            lv_obj_t* port = lv_textarea_create(row1);
            lv_textarea_set_one_line(port, true);
            lv_textarea_set_max_length(port, 10);
            lv_textarea_set_placeholder_text(port, "5900");
            lv_obj_set_name(port, "server_port");
            lv_obj_set_flex_grow(port, 3);
            lv_obj_set_height(port, LV_SIZE_CONTENT);
            lv_obj_set_style_bg_color(port, lv_color_hex(0x222222), 0);
            lv_obj_set_style_text_color(port, lv_color_hex(0xE0E0E0), 0);
            lv_obj_set_style_text_color(port, lv_color_hex(0x444444), LV_PART_TEXTAREA_PLACEHOLDER);
            lv_obj_set_style_border_color(port, lv_color_hex(0x888888), LV_PART_CURSOR);
            lv_obj_add_event_cb(port, onEditEvent, LV_EVENT_ALL, this);
        }

        lv_obj_t* row2 = lv_obj_create(target_box);
        lv_obj_remove_style_all(row2);
        lv_obj_set_size(row2, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_layout(row2, LV_LAYOUT_FLEX);
        lv_obj_set_flex_flow(row2, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row2, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_bg_opa(row2, LV_OPA_TRANSP, 0);
        lv_obj_set_style_margin_all(row2, 0, 0);
        lv_obj_set_style_pad_all(row2, 8, 0);
        lv_obj_set_style_pad_top(row2, 4, 0);
        lv_obj_set_style_pad_column(row2, 8, 0);
        {
            lv_obj_t* label = lv_label_create(row2);
            lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
            lv_label_set_text(label, "Password");
            lv_obj_set_flex_grow(label, 2);
            lv_obj_set_height(label, LV_SIZE_CONTENT);

            lv_obj_t* pass = lv_textarea_create(row2);
            lv_textarea_set_one_line(pass, true);
            lv_textarea_set_max_length(pass, 32);
            lv_textarea_set_password_mode(pass, true);
            lv_textarea_set_placeholder_text(pass, "Enter Password");
            lv_obj_set_name(pass, "server_pass");
            lv_obj_set_flex_grow(pass, 8);
            lv_obj_set_height(pass, LV_SIZE_CONTENT);
            lv_obj_set_style_bg_color(pass, lv_color_hex(0x222222), 0);
            lv_obj_set_style_text_color(pass, lv_color_hex(0xE0E0E0), 0);
            lv_obj_set_style_text_color(pass, lv_color_hex(0x444444), LV_PART_TEXTAREA_PLACEHOLDER);
            lv_obj_set_style_border_color(pass, lv_color_hex(0x888888), LV_PART_CURSOR);
            lv_obj_add_event_cb(pass, onEditEvent, LV_EVENT_ALL, this);
        }
    }


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
    lv_obj_add_event_cb(btn_wifi, onClickedWifi, LV_EVENT_CLICKED, scrn);

    lv_obj_t* wifi_icon = lv_image_create(btn_wifi);
    lv_image_set_src(wifi_icon, LV_SYMBOL_WIFI);

    // Button: Connect To Server
    lv_obj_t* btn_connect = lv_button_create(bottom_container);
    lv_obj_set_name(btn_connect, "btn_connect");
    lv_obj_set_flex_flow(btn_connect, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(btn_connect, 12, 0);
    lv_obj_add_event_cb(btn_connect, onClickedConnect, LV_EVENT_CLICKED, scrn);
    /*
    lv_obj_add_state(btn_connect, LV_STATE_DISABLED);
    */

    lv_obj_t* connect_icon = lv_image_create(btn_connect);
    lv_image_set_src(connect_icon, LV_SYMBOL_PLAY);    


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
    lv_obj_add_event_cb(btn_clear, onClickedClearLog, LV_EVENT_CLICKED, log_ta);

    lv_textarea_set_text(log_ta, "VNC Viewer started!\n");

    return layer;
}

lv_obj_t* VncScreen::createCanvasLayer(vnc_screen_t* scrn, lv_obj_t* parent)
{
    lv_obj_t* canvas = lv_canvas_create(parent);
    lv_obj_set_user_data(canvas, scrn);
    //
    // ...
    // 
    lv_obj_add_flag(canvas, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(canvas, onClickedCanvas, LV_EVENT_CLICKED, scrn);

    //
    lv_obj_t* exit = lv_obj_create(canvas);
    lv_obj_set_user_data(exit, scrn);
    lv_obj_set_name(exit, "disconnect");
    lv_obj_set_size(exit, 72, 32);
    lv_obj_align(exit, LV_ALIGN_BOTTOM_RIGHT, -12, -12);
    lv_obj_set_style_radius(exit, LV_RADIUS_CIRCLE, 0);
    /*
    lv_obj_set_style_border_width(exit, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(exit, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(exit, 0, LV_PART_MAIN);
    */

    lv_obj_t* label = lv_label_create(exit);
    lv_label_set_text(label, LV_SYMBOL_STOP);
    lv_obj_center(label);

    //lv_obj_add_flag(exit, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(exit, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(exit, onClickedDisconnect, LV_EVENT_CLICKED, scrn);

    //
    lv_timer_create(onTimer, 100, scrn);

    return canvas;
}

lv_obj_t* VncScreen::createKeyboard(vnc_screen_t* scrn)
{
    // Create virtual keyboard
    lv_obj_t* kb = lv_keyboard_create(lv_screen_active()); //*/lv_layer_top());
    lv_keyboard_set_popovers(kb, true);
    //lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* ref = lv_obj_find_by_name(scrn->m_active, "connect_panel");
    if (ref)
    {
        int32_t ref_width = lv_obj_get_width(ref);
        lv_obj_set_size(kb, ref_width, LV_PCT(30));
        lv_obj_align_to(kb, ref, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
    }
    /*
    else
    {
        lv_obj_set_size(kb, LV_PCT(40), LV_PCT(30));
        lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    }
    */

    lv_obj_t* obj_addr = lv_obj_find_by_name(scrn->m_active, "server_addr");
    if (obj_addr && 0)
    {
        LV_LOG("show keyboard for address textarea\n");
        lv_obj_add_state(obj_addr, LV_STATE_FOCUSED);
        lv_keyboard_set_textarea(kb, obj_addr);
        lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_NUMBER);
    }

    return kb;
}

void VncScreen::onTimer(lv_timer_t* timer)
{
    vnc_screen_t* scrn = (vnc_screen_t*)lv_timer_get_user_data(timer);
    lv_obj_t* exit = lv_obj_find_by_name(scrn->m_active, "disconnect");

    if (exit)
    {
        LV_LOG_INFO("Face out DISCONNECT button.\n");
        lv_obj_fade_out(exit, 500, 0);
    }

    lv_timer_del(timer);
}



void VncScreen::onEditEvent(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * ta = (lv_obj_t *)lv_event_get_target(e);
    vnc_screen_t* scrn = (vnc_screen_t *)lv_event_get_user_data(e);

    if (code == LV_EVENT_FOCUSED)
    {
        if(scrn->m_keyboard == NULL)
            scrn->m_keyboard = scrn->createKeyboard(scrn);
        else
            lv_obj_remove_flag(scrn->m_keyboard, LV_OBJ_FLAG_HIDDEN);

        if(scrn->m_keyboard != NULL)
        {
            const char* name = lv_obj_get_name(ta);
            LV_LOG("show keyboard for address textarea: %s\n", name);

            // Connect the currently selected input window to the keyboard
            lv_keyboard_set_textarea(scrn->m_keyboard, ta);

            // Switch to the numeric keyboard for port inputs, and the default character keyboard for everything else.
            lv_obj_t* obj_pass = lv_obj_find_by_name(scrn->m_active, "server_pass");
            lv_keyboard_mode_t mode = ta == obj_pass ? LV_KEYBOARD_MODE_TEXT_LOWER : LV_KEYBOARD_MODE_NUMBER;
            lv_keyboard_set_mode(scrn->m_keyboard, mode);
        }
    }
    else if (code == LV_EVENT_DEFOCUSED)
    {
        if(scrn->m_keyboard != NULL)
        {
            lv_obj_add_flag(scrn->m_keyboard, LV_OBJ_FLAG_HIDDEN);
        }
    }
}



void VncScreen::onClickedWifi(lv_event_t* evt)
{
    //lv_obj_t* button = (lv_obj_t*)lv_event_get_target(evt);
    vnc_screen_t* scrn = (vnc_screen_t*)lv_event_get_user_data(evt);

    scrn->m_app->postUserEvent(EventType, EVENT_WIFI_CONFIG, 0, 0);

    /*
    vnc_app_send_event(scrn->app, OPEN_WIFI_SETTING, 0, 0, 0);
    */
    /*
    vnc_wifi_popup_t* popup = vnc_wifi_popup_init(scrn, NULL);
    if (popup)
        popup->show_popup(popup);
    */

    /*
    ESP_LOGI(TAG, "show_wifi_setting_popup [IN]");
    show_wifi_setting_popup();
    ESP_LOGI(TAG, "show_wifi_setting_popup [OUT]");
    */
}

void VncScreen::onClickedConnect(lv_event_t* evt)
{
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(evt);
    vnc_screen_t* scrn = (vnc_screen_t*)lv_event_get_user_data(evt);

    // extract(get) connect information & validate
    if (scrn->extractAndValidate())
    {
        LV_LOG("Request connection: %s:%d\n", scrn->m_addr, scrn->m_port);
        scrn->m_app->postUserEvent(EventType, EVENT_CONNECT, 0, 0);
    }
    else
    {
        scrn->shakeButton(btn);

        lv_obj_set_style_bg_color(btn, lv_color_hex(0xD32F2F), 0);
    }
    
}

void VncScreen::onClickedDisconnect(lv_event_t* evt)
{
    LV_LOG_INFO("Clicked on disconnect button\n");
    //ESP_LOGI(TAG, "*");
    //ESP_LOGI(TAG, "* vnc_handler_on_disconnect");
    //ESP_LOGI(TAG, "*");

    vnc_screen_t* scrn = (vnc_screen_t*)lv_event_get_user_data(evt);
    scrn->m_app->postUserEvent(EventType, EVENT_DISCONNECT, 0, 0);
}

void VncScreen::onClickedClearLog(lv_event_t* evt)
{
    lv_obj_t* log_ta = (lv_obj_t*)lv_event_get_user_data(evt);
    if (log_ta)
        lv_textarea_set_text(log_ta, "");
}

void VncScreen::onClickedCanvas(lv_event_t* evt)
{
    LV_LOG_INFO("Clicked on canvas\n");
    lv_event_code_t code = lv_event_get_code(evt);
    vnc_screen_t* scrn = (vnc_screen_t*)lv_event_get_user_data(evt);
    lv_obj_t* exit = lv_obj_find_by_name(scrn->m_active, "disconnect");

    if (code == LV_EVENT_CLICKED && exit)
    {
        //lv_obj_remove_flag(exit, LV_OBJ_FLAG_HIDDEN);
        lv_obj_fade_in(exit, 500, 0);
        lv_timer_create(onTimer, 10000, scrn);
    }
}
