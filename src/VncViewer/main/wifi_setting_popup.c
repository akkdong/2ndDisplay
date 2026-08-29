// wifi_setting_popup.c
//

#include <stdlib.h>
#include <string.h>
#include "wifi_setting_popup.h"
#include "lvgl.h"


//
//
//

static vnc_wifi_popup_t vnc_popup;




//
//
//

// on click icon of wifi-item
static void wifi_icon_event_cb(lv_event_t * e)
{
    lv_obj_t * icon = lv_event_get_target(e);
    char * wifi_name = (char *)lv_event_get_user_data(e);

    LV_LOG_USER("click on icon: show detail information of %s", wifi_name);
}

// on click wifi-item itself
static void wifi_item_event_cb(lv_event_t * e)
{
    lv_obj_t * item = lv_event_get_target(e);
    char * wifi_name = (char *)lv_event_get_user_data(e);

    LV_LOG_USER("click on item: conntect to %s", wifi_name);
}

// create wifi-item
static lv_obj_t * create_wifi_item(lv_obj_t * parent, const char * name, const char * status, const char * right_icon_symbol)
{
    lv_obj_t * row = lv_obj_create(parent);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    //
    //lv_obj_set_style_bg_color(row, lv_color_hex(0xFFB0B0), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 8, 0);
    lv_obj_set_style_pad_column(row, 12, 0);

    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE); // lv_obj_set_clickable(row, true);
    lv_obj_add_event_cb(row, wifi_item_event_cb, LV_EVENT_CLICKED, (void *)name);

    // wifi-state(strenth)
    lv_obj_t * icon_left = lv_label_create(row);
    lv_label_set_text(icon_left, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(icon_left, lv_color_hex(0x4A90E2), 0);
    lv_obj_set_style_text_font(icon_left, &lv_font_montserrat_16, 0);
    lv_obj_remove_flag(icon_left, LV_OBJ_FLAG_CLICKABLE); // lv_obj_set_clickable(icon_left, false);

    // wifi name (& description)
    lv_obj_t * text_area = lv_obj_create(row);
    lv_obj_set_flex_grow(text_area, 1);
    lv_obj_set_height(text_area, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(text_area, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_opa(text_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(text_area, 0, 0);
    lv_obj_set_style_pad_all(text_area, 0, 0);
    lv_obj_set_style_pad_row(text_area, 1, 0);
    lv_obj_remove_flag(text_area, LV_OBJ_FLAG_CLICKABLE); // lv_obj_set_clickable(text_area, false);

    lv_obj_t * lbl_name = lv_label_create(text_area);
    lv_label_set_text(lbl_name, name);
    lv_obj_set_style_text_color(lbl_name, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lbl_name, &lv_font_montserrat_14, 0);
    lv_obj_remove_flag(lbl_name, LV_OBJ_FLAG_CLICKABLE); // lv_obj_set_clickable(lbl_name, false);

    if (status && strlen(status) > 0) 
    {
        lv_obj_t * lbl_status = lv_label_create(text_area);
        lv_label_set_text(lbl_status, status);
        lv_obj_set_style_text_color(lbl_status, lv_color_hex(0xAAAAAA), 0);
        lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_12, 0);
        lv_obj_remove_flag(lbl_status, LV_OBJ_FLAG_CLICKABLE); // lv_obj_set_clickable(lbl_status, false);
    }

    // wifi-icon
    if (right_icon_symbol) 
    {
        lv_obj_t * sep = lv_obj_create(row);
        lv_obj_set_size(sep, 1, 20);
        lv_obj_set_style_bg_color(sep, lv_color_hex(0x444444), 0);
        lv_obj_set_style_border_width(sep, 0, 0);

        lv_obj_t * btn_right = lv_button_create(row);
        lv_obj_set_size(btn_right, 26, 26);
        lv_obj_set_style_bg_opa(btn_right, LV_OPA_TRANSP, 0);
        lv_obj_set_style_shadow_width(btn_right, 0, 0);

        lv_obj_t * icon_right = lv_label_create(btn_right);
        lv_label_set_text(icon_right, right_icon_symbol);
        lv_obj_center(icon_right);
        lv_obj_set_style_text_color(icon_right, lv_color_hex(0x888888), 0);

        lv_obj_add_flag(icon_right, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(icon_right, wifi_icon_event_cb, LV_EVENT_CLICKED, (void *)name);
    }

    return row;
}

/**
 *
 */
static void msgbox_cleanup_event_cb(lv_event_t * e)
{
    /*
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
    */
}






/**
 *
 */
static void show_wifi_popup(vnc_wifi_popup_t* popup)
{
    //lv_obj_t * scr = lv_display_get_screen_active(NULL);

    // 1. creat modal popup
    lv_obj_t * mbox = lv_msgbox_create(NULL);
    lv_msgbox_add_title(mbox, "Wi-Fi Setting");
    lv_msgbox_add_close_button(mbox);
    lv_obj_set_user_data(mbox, popup);
    lv_obj_add_event_cb(mbox, msgbox_cleanup_event_cb, LV_EVENT_DELETE, popup);
    lv_obj_set_style_bg_color(mbox, lv_color_hex(0x181818), 0);

    // layout message-box
    lv_obj_set_size(mbox, lv_pct(90), lv_pct(80));
    lv_obj_align(mbox, LV_ALIGN_TOP_MID, 0, LV_PCT(4));
    lv_obj_set_style_border_width(mbox, 2, 0);
    lv_obj_set_style_radius(mbox, 16, 0);

    /*
    // layout content-area
    lv_obj_t * content = lv_msgbox_get_content(mbox);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(content, 10, 0);
    lv_obj_set_style_pad_row(content, 12, 0);

    //
    // Connected Network
    //

    // title
    lv_obj_t * title1 = lv_label_create(content);
    lv_label_set_text(title1, "Connected Network");
    lv_obj_set_style_text_color(title1, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(title1, &lv_font_montserrat_12, 0);

    // wifi-item container
    lv_obj_t * block_connected = lv_obj_create(content);
    lv_obj_set_width(block_connected, lv_pct(100));
    lv_obj_set_height(block_connected, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(block_connected, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(block_connected, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_width(block_connected, 0, 0);
    lv_obj_set_style_radius(block_connected, 10, 0);
    lv_obj_set_style_pad_all(block_connected, 6, 0);

    // for example: 
    create_wifi_item(block_connected, "AP Active", "Connected", LV_SYMBOL_SETTINGS);


    //
    // Available Networks
    //

    // title
    lv_obj_t * title2 = lv_label_create(content);
    lv_label_set_text(title2, "Available Networks");
    lv_obj_set_style_text_color(title2, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(title2, &lv_font_montserrat_12, 0);

    // 
    lv_obj_t * block_available = lv_obj_create(content);
    lv_obj_set_width(block_available, lv_pct(100));
    lv_obj_set_flex_grow(block_available, 1);
    lv_obj_set_flex_flow(block_available, LV_FLEX_FLOW_COLUMN);

    // 
    lv_obj_set_style_bg_color(block_available, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_width(block_available, 0, 0);
    lv_obj_set_style_radius(block_available, 10, 0);
    lv_obj_set_style_pad_all(block_available, 6, 0);

    // explicitly enable scroll 
    lv_obj_set_scroll_dir(block_available, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(block_available, LV_SCROLLBAR_MODE_AUTO);

    // for example
    create_wifi_item(block_available, "AP 1", "Hidden", LV_SYMBOL_IMAGE);
    create_wifi_item(block_available, "AP 2", NULL, NULL);
    create_wifi_item(block_available, "AP 3", NULL, NULL);
    create_wifi_item(block_available, "AP 4", NULL, NULL);
    create_wifi_item(block_available, "AP 5", NULL, NULL);
    create_wifi_item(block_available, "AP 6", NULL, NULL);
    create_wifi_item(block_available, "AP 7", "Strong signal", NULL);
    create_wifi_item(block_available, "AP 8", NULL, NULL);
    create_wifi_item(block_available, "AP 9", NULL, NULL);
    create_wifi_item(block_available, "AP 2.4G", NULL, NULL);
    create_wifi_item(block_available, "AP 5G", NULL, NULL);
    create_wifi_item(block_available, "AP 2G", NULL, NULL);
    create_wifi_item(block_available, "AP GiGA", "Strong signal", NULL);
    */
}



vnc_wifi_popup_t* vnc_wifi_popup_init(vnc_screen_t* scrn, on_wifi_connect_cb callback)
{
    //
    vnc_popup.ssid[0] = 0;

    //
    vnc_popup.scrn = scrn;

    vnc_popup.on_connect = callback;
    vnc_popup.show_popup = show_wifi_popup;


    return &vnc_popup;
}
