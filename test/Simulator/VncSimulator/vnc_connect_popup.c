// vnc_connect_popup.c
//

#include <stdlib.h>
#include <string.h>
#include "lvgl/lvgl.h"

#include "vnc_connect_popup.h"



//
//
//

/*
typedef struct vnc_connect_popup
{
    //
    char address[16];
    uint16_t port;
    char password[32];

    //
    lv_obj_t* ta_address;
    lv_obj_t* ta_port;
    lv_obj_t* ta_password;
    lv_obj_t* kb;

    //
    on_connect_cb on_connect;
    show_popup_cb show_popup;

} vnc_connect_popup_t;
*/

static vnc_connect_popup_t vnc_popup;


/**
 *
 */

static lv_color_t get_default_button_color(void)
{
    // 1. 활성 화면에 디폴트 스타일을 가진 더미 버튼 임시 생성
    lv_obj_t * scr = lv_display_get_screen_active(NULL);
    lv_obj_t * dummy_btn = lv_button_create(scr);

    // 2. 버튼의 메인 파트 및 디폴트 상태(LV_PART_MAIN | LV_STATE_DEFAULT)의 배경색 추출
    //    (참고: 두 값이 모두 0이므로 0을 인자로 전달해도 됩니다)
    lv_color_t default_color = lv_obj_get_style_bg_color(dummy_btn, LV_PART_MAIN);

    // 3. 색상 추출 완료 후 메모리 해제를 위해 더미 버튼 삭제
    lv_obj_delete(dummy_btn);

    // 4. 추출된 lv_color_t 구조체 반환
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
    // 원래 위치 기준 오프셋 이동
    lv_obj_set_style_translate_x(obj, v, 0);
    // 중요: LVGL에게 이 객체가 이동했으니 이 부분 화면을 다시 그리라고 강제 지시 (Flex 간섭 우회)
    lv_obj_invalidate(obj);
}

/**
 *
 */
void shake_button(lv_obj_t * btn)
{
    if(btn == NULL) return;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, btn);

    // 일반 lv_obj_set_style_translate_x 대신 위에서 만든 커스텀 콜백 지정
    lv_anim_set_exec_cb(&a, custom_shake_exec_cb);

    lv_anim_set_values(&a, -12, 12); // 양옆으로 12픽셀씩 흔듦
    lv_anim_set_duration(&a, 50);    // 50ms 속도
    lv_anim_set_repeat_count(&a, 4); // 4번 왕복
    lv_anim_set_playback_duration(&a, 50);
    lv_anim_set_path_cb(&a, lv_anim_path_linear);
    lv_anim_set_completed_cb(&a, anim_ready_cb); // (전 단계의 0으로 초기화하는 콜백)

    lv_anim_start(&a);
}

/**
 *
 */
static void msgbox_cleanup_event_cb(lv_event_t * e)
{
    lv_obj_t * mbox = lv_event_get_current_target(e);

    if(lv_event_get_code(e) == LV_EVENT_DELETE)
    {
        // CLEANUP
        vnc_connect_popup_t* popup = (vnc_connect_popup_t *)lv_event_get_user_data(e);
        if(popup && popup->kb)
        {
            lv_obj_delete(popup->kb);
            popup->kb = NULL;
        }
    }
}

/**
 *
 */
static void ta_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * ta = lv_event_get_target(e);
    vnc_connect_popup_t* popup = (vnc_connect_popup_t *)lv_event_get_user_data(e);

    if(popup->kb == NULL)
        return;

    if (code == LV_EVENT_FOCUSED)
    {
        // Connect the currently selected input window to the keyboard
        lv_keyboard_set_textarea(popup->kb, ta);

        // Switch to the numeric keyboard for port inputs, and the default character keyboard for everything else.
        lv_keyboard_mode_t mode = ta == popup->ta_password ? LV_KEYBOARD_MODE_TEXT_LOWER : LV_KEYBOARD_MODE_NUMBER;
        lv_keyboard_set_mode(popup->kb, mode);
    }
}

/**
 *
 */
static bool msgbox_validate(vnc_connect_popup_t* popup)
{
    return true;
}

/**
 *
 */
static void msgbox_event_cb(lv_event_t * e)
{
    lv_obj_t * btn = lv_event_get_target(e);
    lv_obj_t * msgbox = lv_event_get_user_data(e);
    vnc_connect_popup_t* popup = (vnc_connect_popup_t *)lv_obj_get_user_data(msgbox);

    const char* name = lv_obj_get_name(btn);
    if (strcmp(name, "connect") == 0)
    {
        // extract & convert information
        strcpy_s(popup->address, sizeof(popup->address), lv_textarea_get_text(popup->ta_address));
        popup->port = atoi(lv_textarea_get_text(popup->ta_port));
        strcpy_s(popup->password, sizeof(popup->password), lv_textarea_get_text(popup->ta_password));

        if (msgbox_validate(popup))
        {
            printf("[popup] call connect-server\n");
            popup->on_connect(popup->scrn, popup->address, popup->port, popup->password);
            printf("[popup] after connect-server\n");
        }
        else
        {
            shake_button(btn);
            lv_obj_set_style_bg_color(btn, lv_color_hex(0xD32F2F), 0);

            //
            return;
        }
    }

    // Popup and Keyboard Cleanup (Memory Free)
    lv_msgbox_close(msgbox);
}




/**
 *
 *
 *
 */
static void vnc_connect_popup_show(vnc_connect_popup_t* popup)
{
    // 0. initialize popup state
    /*
    memset(&popup, 0, sizeof(popup));
    if (addr)
        strcpy_s(popup.address, sizeof(popup.address), addr);
    popup.port = port == 0 ? 5900 : port;
    popup.on_connect = callback;
    */
    if (popup->port == 0)
        popup->port = 5900; // default port number

    // 1. create modal popup
    lv_obj_t * mbox = lv_msgbox_create(NULL);
    lv_msgbox_add_title(mbox, "Connect To Server");
    lv_msgbox_add_close_button(mbox);
    lv_obj_set_user_data(mbox, popup);
    lv_obj_add_event_cb(mbox, msgbox_cleanup_event_cb, LV_EVENT_DELETE, popup);
    lv_obj_set_style_bg_color(mbox, lv_color_hex(0x181818), 0);

    // layout message-box
    lv_obj_set_width(mbox, LV_PCT(90));
    lv_obj_set_height(mbox, LV_PCT(48));
    lv_obj_align(mbox, LV_ALIGN_TOP_MID, 0, LV_PCT(4));

    // content area: flelx layout, vertical align(column)
    lv_obj_t * content = lv_msgbox_get_content(mbox);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(content, 16, 0);

    // IP Address
    lv_obj_t* group = lv_obj_create(content);
    lv_obj_set_flex_flow(group, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(group, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(group, LV_OPA_TRANSP, 0);
    lv_obj_set_width(group, LV_PCT(100));
    lv_obj_set_height(group, LV_SIZE_CONTENT);
    lv_obj_set_style_border_opa(group, LV_OPA_0, 0);
    lv_obj_set_style_margin_all(group, 0, 0);
    lv_obj_set_style_pad_all(group, 0, 0);
    lv_obj_set_style_pad_row(group, 2, 0);
    //lv_obj_set_style_bg_color(group, lv_color_hex(0xFFE0E0), 0);

    lv_obj_t* label = lv_label_create(group);
    lv_label_set_text(label, "IP Address");
    lv_obj_set_style_text_color(label, lv_color_hex(0xD0D0D0), 0);

    popup->ta_address = lv_textarea_create(group);
    lv_textarea_set_one_line(popup->ta_address, true);
    lv_textarea_set_max_length(popup->ta_address, sizeof(popup->address) - 1);
    lv_textarea_set_placeholder_text(popup->ta_address, "192.168.100.2");
    lv_obj_set_style_bg_color(popup->ta_address, lv_color_hex(0x222222), 0);
    lv_obj_set_style_text_color(popup->ta_address, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_text_color(popup->ta_address, lv_color_hex(0x444444), LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_border_color(popup->ta_address, lv_color_hex(0x888888), LV_PART_CURSOR);
    lv_obj_set_width(popup->ta_address, LV_PCT(100));
    lv_obj_add_event_cb(popup->ta_address, ta_event_cb, LV_EVENT_ALL, popup);

    // Port
    group = lv_obj_create(content);
    lv_obj_set_flex_flow(group, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(group, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_bg_opa(group, LV_OPA_TRANSP, 0);
    lv_obj_set_width(group, LV_PCT(100));
    lv_obj_set_height(group, LV_SIZE_CONTENT);
    lv_obj_set_style_border_opa(group, LV_OPA_0, 0);
    lv_obj_set_style_margin_all(group, 0, 0);
    lv_obj_set_style_pad_all(group, 0, 0);
    lv_obj_set_style_pad_row(group, 2, 0);
    //lv_obj_set_style_bg_color(group, lv_color_hex(0xE0FFE0), 0);

    label = lv_label_create(group);
    lv_label_set_text(label, "Port");
    lv_obj_set_style_text_color(label, lv_color_hex(0xD0D0D0), 0);

    popup->ta_port = lv_textarea_create(group);
    lv_textarea_set_one_line(popup->ta_port, true);
    lv_textarea_set_max_length(popup->ta_port, 5);
    lv_textarea_set_placeholder_text(popup->ta_port, "5900");
    lv_obj_set_style_bg_color(popup->ta_port, lv_color_hex(0x222222), 0);
    lv_obj_set_style_text_color(popup->ta_port, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_text_color(popup->ta_port, lv_color_hex(0x444444), LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_border_color(popup->ta_port, lv_color_hex(0x888888), LV_PART_CURSOR);
    lv_obj_set_width(popup->ta_port, LV_PCT(80));
    lv_obj_add_event_cb(popup->ta_port, ta_event_cb, LV_EVENT_ALL, popup);

    // Password
    group = lv_obj_create(content);
    lv_obj_set_flex_flow(group, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(group, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(group, LV_OPA_TRANSP, 0);
    lv_obj_set_width(group, LV_PCT(100));
    lv_obj_set_height(group, LV_SIZE_CONTENT);
    lv_obj_set_style_border_opa(group, LV_OPA_0, 0);
    lv_obj_set_style_margin_all(group, 0, 0);
    lv_obj_set_style_pad_all(group, 0, 0);
    lv_obj_set_style_pad_row(group, 2, 0);
    //lv_obj_set_style_bg_color(group, lv_color_hex(0xE0E0FF), 0);

    label = lv_label_create(group);
    lv_label_set_text(label, "Password");
    lv_obj_set_style_text_color(label, lv_color_hex(0xD0D0D0), 0);

    popup->ta_password = lv_textarea_create(group);
    lv_textarea_set_one_line(popup->ta_password, true);
    lv_textarea_set_max_length(popup->ta_password, sizeof(popup->password) - 1);
    lv_textarea_set_password_mode(popup->ta_password, true);
    lv_textarea_set_placeholder_text(popup->ta_password, "Password");
    lv_obj_set_style_bg_color(popup->ta_password, lv_color_hex(0x222222), 0);
    lv_obj_set_style_text_color(popup->ta_password, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_text_color(popup->ta_password, lv_color_hex(0x444444), LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_border_color(popup->ta_password, lv_color_hex(0x888888), LV_PART_CURSOR);
    lv_obj_set_width(popup->ta_password, LV_PCT(100));
    lv_obj_add_event_cb(popup->ta_password, ta_event_cb, LV_EVENT_ALL, popup);


    // Footer: Connect, Cancel
    lv_obj_t * btn_connect = lv_msgbox_add_footer_button(mbox, "Connect");
    lv_obj_set_name(btn_connect, "connect");
    lv_obj_add_event_cb(btn_connect, msgbox_event_cb, LV_EVENT_CLICKED, mbox);
#if USE_CANCEL_BUTTON
    lv_obj_t * btn_cancel = lv_msgbox_add_footer_button(mbox, "Cancel");
    lv_obj_set_name(btn_cancel, "cancel");
    lv_obj_add_event_cb(btn_cancel, msgbox_event_cb, LV_EVENT_CLICKED, mbox);
#endif

    lv_obj_t* footer = lv_msgbox_get_footer(mbox);
    if (footer)
    {
        lv_obj_set_style_bg_color(footer, lv_color_hex(0x202020), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(footer, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_pad_all(footer, 10, LV_PART_MAIN);

        lv_obj_update_layout(mbox);
        lv_obj_set_height(content, lv_obj_get_height(content) - 10);
        lv_obj_set_height(footer, lv_obj_get_height(footer) + 10);
    }

    //
    if (popup->address[0])
        lv_textarea_set_text(popup->ta_address, popup->address);
    if (popup->port > 0)
    {
        char port[16];
        _itoa_s(popup->port, port, sizeof(port), 10);
        lv_textarea_set_text(popup->ta_port, port);
    }
    if (popup->password[0])
        lv_textarea_set_text(popup->ta_password, popup->password);

    // Create virtual keyboard
    popup->kb = lv_keyboard_create(/*lv_screen_active()*/lv_layer_top());
    lv_keyboard_set_popovers(popup->kb, true);
    lv_obj_set_size(popup->kb, LV_PCT(100), LV_PCT(30));
    lv_obj_align(popup->kb, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_obj_add_state(popup->ta_address, LV_STATE_FOCUSED);
    lv_keyboard_set_textarea(popup->kb, popup->ta_address);
    lv_keyboard_set_mode(popup->kb, LV_KEYBOARD_MODE_NUMBER);
}






/**
 *
 *
 */

vnc_connect_popup_t* vnc_connect_popup_init(vnc_screen_t* scrn, on_connect_cb connect_cb)
{
    //
    vnc_popup.address[0] = 0;
    vnc_popup.port = 0;
    vnc_popup.password[0] = 0;

    vnc_popup.ta_address = NULL;
    vnc_popup.ta_port = NULL;
    vnc_popup.ta_password = NULL;
    vnc_popup.kb = NULL;

    //
    vnc_popup.scrn = scrn;

    vnc_popup.on_connect = connect_cb;
    vnc_popup.show_popup = vnc_connect_popup_show;


    return &vnc_popup;
}
