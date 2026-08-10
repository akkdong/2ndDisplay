// wifi_setting_popup.c
//

#include <stdlib.h>
#include <string.h>
#include "wifi_setting_popup.h"
#include "lvgl/lvgl.h"


// 1. 아이콘 클릭 시 이벤트 콜백 (예: Wi-Fi 상세 정보창 띄우기)
static void wifi_icon_event_cb(lv_event_t * e)
{
    lv_obj_t * icon = lv_event_get_target(e);
    // 유저 데이터로 전달한 와이파이 ID나 데이터 구조체 추출 가능
    char * wifi_name = (char *)lv_event_get_user_data(e);

    LV_LOG_USER("아이콘 클릭: %s 의 상세 설정 진입", wifi_name);
    // 여기에 상세 설정창 띄우는 코드 작성
}

// 2. 아이템 전체 클릭 시 이벤트 콜백 (예: Wi-Fi 바로 연결 시도)
static void wifi_item_event_cb(lv_event_t * e)
{
    lv_obj_t * item = lv_event_get_target(e);
    char * wifi_name = (char *)lv_event_get_user_data(e);

    LV_LOG_USER("아이템 클릭: %s 에 연결 시도", wifi_name);
    // 여기에 Wi-Fi 연결 로직 작성
}

// 와이파이 리스트 개별 항목(Row)을 생성하는 헬퍼 함수
lv_obj_t * create_wifi_item(lv_obj_t * parent, const char * name, const char * status, const char * right_icon_symbol)
{
    lv_obj_t * row = lv_obj_create(parent);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 개별 아이템 내부 배경은 투명하게 설정하여 블록 배경을 따르도록 함
    //lv_obj_set_style_bg_color(row, lv_color_hex(0xFFB0B0), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 8, 0);
    lv_obj_set_style_pad_column(row, 12, 0);

    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE); // lv_obj_set_clickable(row, true);
    lv_obj_add_event_cb(row, wifi_item_event_cb, LV_EVENT_CLICKED, (void *)name);

    // 1. 좌측 와이파이 아이콘
    lv_obj_t * icon_left = lv_label_create(row);
    lv_label_set_text(icon_left, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(icon_left, lv_color_hex(0x4A90E2), 0);
    lv_obj_set_style_text_font(icon_left, &lv_font_montserrat_16, 0);
    lv_obj_remove_flag(icon_left, LV_OBJ_FLAG_CLICKABLE); // lv_obj_set_clickable(icon_left, false);

    // 2. 중앙 텍스트 영역
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

    if(status && strlen(status) > 0) {
        lv_obj_t * lbl_status = lv_label_create(text_area);
        lv_label_set_text(lbl_status, status);
        lv_obj_set_style_text_color(lbl_status, lv_color_hex(0xAAAAAA), 0);
        lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_12, 0);
        lv_obj_remove_flag(lbl_status, LV_OBJ_FLAG_CLICKABLE); // lv_obj_set_clickable(lbl_status, false);
    }

    // 3. 우측 액션 아이콘
    if(right_icon_symbol) {
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

// 전체 메시지박스 생성 함수
void show_wifi_setting_popup(void)
{
    lv_obj_t * scr = lv_display_get_screen_active(NULL);

    // 1. LVGL 9.x 스타일의 표준 Msgbox 생성 (기본 모달/바탕레이어 타겟)
    lv_obj_t * mbox = lv_msgbox_create(scr);
    lv_msgbox_add_title(mbox, "Wi-Fi Setting"); // 상단 헤더 타이틀 추가
    lv_msgbox_add_close_button(mbox);       // 우측 상단 닫기 X 버튼 추가

    // Msgbox 메인 배경 스타일 설정 (어두운 테마)
    lv_obj_set_size(mbox, lv_pct(90), lv_pct(80));
    lv_obj_align(mbox, LV_ALIGN_TOP_MID, 0, LV_PCT(4));
    lv_obj_set_style_bg_color(mbox, lv_color_hex(0x121212), 0); // 메인 팝업 딥블랙 배경
    lv_obj_set_style_border_width(mbox, 2, 0);
    lv_obj_set_style_radius(mbox, 16, 0);

    // 2. Msgbox 본문(Content) 영역 추출 후 레이아웃 세팅
    lv_obj_t * content = lv_msgbox_get_content(mbox);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(content, 10, 0);
    lv_obj_set_style_pad_row(content, 12, 0); // 상하 블록 간 간격

    // ==========================================
    // BLOCK 1: 연결된 네트워크 (상단 고정 블록)
    // ==========================================

    // 타이틀
    lv_obj_t * title1 = lv_label_create(content);
    lv_label_set_text(title1, "Connected Network");
    lv_obj_set_style_text_color(title1, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(title1, &lv_font_montserrat_12, 0);

    // 팝업 배경과 구분하기 위해 더 밝은 톤의 독립 컨테이너 적용
    lv_obj_t * block_connected = lv_obj_create(content);
    lv_obj_set_width(block_connected, lv_pct(100));
    lv_obj_set_height(block_connected, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(block_connected, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_color(block_connected, lv_color_hex(0x222222), 0); // 확연히 구별되는 회색 블록 배경
    lv_obj_set_style_border_width(block_connected, 0, 0);
    lv_obj_set_style_radius(block_connected, 10, 0);
    lv_obj_set_style_pad_all(block_connected, 6, 0);

    // 실제 아이템 생성 및 삽입
    create_wifi_item(block_connected, "AP Active", "Connected", LV_SYMBOL_SETTINGS);


    // ==========================================
    // BLOCK 2: 사용 가능한 네트워크 (하단 가변 스크롤 블록)
    // ==========================================

    // 타이틀
    lv_obj_t * title2 = lv_label_create(content);
    lv_label_set_text(title2, "Available Networks");
    lv_obj_set_style_text_color(title2, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(title2, &lv_font_montserrat_12, 0);

    // 팝업 배경과 구분하기 위해 배경색을 주고 가변 스크롤이 되도록 높이를 고정(또는 flex_grow)
    lv_obj_t * block_available = lv_obj_create(content);
    lv_obj_set_width(block_available, lv_pct(100));
    lv_obj_set_flex_grow(block_available, 1); // 남은 수직 공간을 유연하게 꽉 채우도록 설정
    lv_obj_set_flex_flow(block_available, LV_FLEX_FLOW_COLUMN);

    // 시각적 디자인 요소 세팅
    lv_obj_set_style_bg_color(block_available, lv_color_hex(0x222222), 0); // 상단과 동일한 구분 회색 배경
    lv_obj_set_style_border_width(block_available, 0, 0);
    lv_obj_set_style_radius(block_available, 10, 0);
    lv_obj_set_style_pad_all(block_available, 6, 0);

    // 스크롤 관련 명시적 활성화 (LVGL 9 제어 방식)
    lv_obj_set_scroll_dir(block_available, LV_DIR_VER); // 세로방향 스크롤 강제 활성
    lv_obj_set_scrollbar_mode(block_available, LV_SCROLLBAR_MODE_AUTO); // 항목이 넘칠 때만 스크롤바 노출

    // 동적 다중 데이터 가상 삽입 (스크롤 연출을 위해 대량 추가)
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
}
