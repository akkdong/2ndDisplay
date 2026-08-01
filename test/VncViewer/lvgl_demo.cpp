// lvgl_demo.cpp
//

#include <unistd.h>

#include "lvgl/lvgl.h"



// 화면 크기 정의 (예시: 800x480)
#define SCR_WIDTH  800
#define SCR_HEIGHT 480

#define TAB_BAR_HEIGHT 50

// 전역/멤버 변수 선언
static lv_obj_t * canvas;
static lv_obj_t * tab_bar;
static lv_obj_t * settings_panel;
static lv_timer_t * hide_timer = NULL;

// 탭바가 화면에 완전히 보이는 목표 Y 좌표와 완전히 숨겨지는 Y 좌표
const int32_t Y_VISIBLE = 0;
const int32_t Y_HIDDEN  = -TAB_BAR_HEIGHT;

// 24비트 캔버스 버퍼 (RGB888 = 3바이트)
static uint8_t canvas_buffer[SCR_WIDTH * SCR_HEIGHT * 3];

// --- 1. 애니메이션 헬퍼 함수 ---
// 객체의 Y 좌표를 부드럽게 이동시키는 애니메이션
static void animate_y(lv_obj_t * obj, int32_t start, int32_t end, uint32_t duration) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_values(&a, start, end);
    lv_anim_set_time(&a, duration);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out); // 부드럽게 멈춤
    lv_anim_start(&a);
}

// --- 정밀한 애니메이션 토글 제어 함수 ---
static void toggle_tab_bar(void) {
    // 1. 현재 탭바에 적용 중인 Y축 이동 애니메이션이 있는지 찾습니다.
    lv_anim_t * current_anim = lv_anim_get(tab_bar, (lv_anim_exec_xcb_t)lv_obj_set_y);
    
    int32_t current_y = lv_obj_get_y(tab_bar);
    int32_t target_y;
    uint32_t duration = 250; // 기본 애니메이션 속도 (ms)

    if (current_anim) {
        // [케이스 A] 애니메이션이 진행 중인 경우 -> 방향을 '반대'로 전환
        // 현재 애니메이션이 '숨겨지는 중(Y_HIDDEN이 목표)'이었다면 -> '보여주기(Y_VISIBLE)'로 전환
        // 반대로 '보여지는 중(Y_VISIBLE이 목표)'이었다면 -> '숨기기(Y_HIDDEN)'로 전환
        if (current_anim->end_value == Y_HIDDEN) {
            target_y = Y_VISIBLE;
        } else {
            target_y = Y_HIDDEN;
        }
        
        // 진행 중이던 기존 애니메이션은 즉시 삭제 (중복 처리 방지)
        lv_anim_delete(tab_bar, (lv_anim_exec_xcb_t)lv_obj_set_y);
    } 
    else {
        // [케이스 B] 정지 상태인 경우 -> 현재 위치를 기준으로 토글
        // Y 좌표가 0보다 작으면(숨겨져 있으면) -> 보여주기
        if (current_y < Y_VISIBLE) {
            target_y = Y_VISIBLE;
        } else {
            target_y = Y_HIDDEN;
        }
    }

    // 2. 결정된 target_y로 새로운 애니메이션을 부드럽게 실행
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, tab_bar);
    lv_anim_set_values(&a, current_y, target_y); // '현재 꼬여있는 위치'에서부터 출발
    lv_anim_set_time(&a, duration);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out); // 끝이 부드러운 감속 모션
    lv_anim_start(&a);

    // 3. 타이머 상태 제어 (보여질 때는 3초 카운트 작동, 숨겨질 때는 타이머 정지)
    if (target_y == Y_VISIBLE) {
        if (hide_timer) {
            lv_timer_reset(hide_timer);  // 3초 초기화
            lv_timer_resume(hide_timer); // 타이머 작동
        }
    } else {
        if (hide_timer) {
            lv_timer_pause(hide_timer);  // 이미 숨었으므로 타이머 일시정지
        }
    }
}

// 3초 뒤에 호출되어 탭을 자동으로 숨기는 타이머 콜백
static void hide_timer_cb(lv_timer_t * timer) {
    int32_t current_y = lv_obj_get_y(tab_bar);
    lv_anim_t * current_anim = lv_anim_get(tab_bar, (lv_anim_exec_xcb_t)lv_obj_set_y);

    // 이미 완전히 숨어있거나, 숨는 중인 애니메이션이 있다면 중복 실행하지 않음
    if (current_y <= Y_HIDDEN || (current_anim && current_anim->end_value == Y_HIDDEN)) {
        lv_timer_pause(timer);
        return;
    }

    // 정지 상태이거나 나타나는 중이었다면 강제로 숨김 애니메이션 가동
    if (current_anim) {
        lv_anim_delete(tab_bar, (lv_anim_exec_xcb_t)lv_obj_set_y);
    }

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, tab_bar);
    lv_anim_set_values(&a, current_y, Y_HIDDEN);
    lv_anim_set_time(&a, 250);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);

    lv_timer_pause(timer);
}

// --- 3. 이벤트 핸들러 ---
static void ui_event_handler(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);

    // 화면(캔버스) 또는 탭바 자체를 클릭했을 때 토글 로직 발동
    if (code == LV_EVENT_CLICKED) {
        toggle_tab_bar();
    }
}

// --- 4. 전체 UI 초기화 메인 함수 ---
void init_vnc_display_ui(void) {
    lv_obj_t * screen = lv_screen_active();

    // ----------------------------------------------------
    // LAYER 1: 최하위 24비트 컬러 캔버스 (VNC 화면 출력용)
    // ----------------------------------------------------
    canvas = lv_canvas_create(screen);
    // LVGL v9에서는 RGB888용 포맷으로 LV_COLOR_FORMAT_RGB888을 사용합니다.
    lv_canvas_set_buffer(canvas, canvas_buffer, SCR_WIDTH, SCR_HEIGHT, LV_COLOR_FORMAT_RGB888);
    lv_obj_align(canvas, LV_ALIGN_TOP_LEFT, 0, 0);
    
    // 캔버스 레이어를 클릭해도 탭바가 나타나야 하므로 이벤트 등록
    lv_obj_add_flag(canvas, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(canvas, ui_event_handler, LV_EVENT_ALL, NULL);

    // ----------------------------------------------------
    // LAYER 2: 중간 레이어 - 내려오는 설정 창 (초기 위치는 화면 위 바깥)
    // ----------------------------------------------------
    settings_panel = lv_obj_create(screen);
    lv_obj_set_size(settings_panel, SCR_WIDTH, SCR_HEIGHT - 100); // 하단 일부는 남김
    lv_obj_set_pos(settings_panel, 0, -SCR_HEIGHT); // 화면 위로 완전히 숨김
    lv_obj_set_style_bg_color(settings_panel, lv_color_hex(0x2C3E50), 0); // 어두운 배경색
    lv_obj_set_style_bg_opa(settings_panel, LV_OPA_90, 0); // 살짝 반투명하게
    
    // 설정창 내부에 닫기 버튼이나 제스처 인식을 위한 설정
    lv_obj_add_event_cb(settings_panel, ui_event_handler, LV_EVENT_ALL, NULL);
    
    // 예시: 설정창 내부 텍스트
    lv_obj_t * set_label = lv_label_create(settings_panel);
    lv_label_set_text(set_label, "Settings Menu\n(Swipe UP to close)");
    lv_obj_center(set_label);

    // ----------------------------------------------------
    // LAYER 3: 최상위 레이어 - 자동 숨김 탭 버튼 바
    // ----------------------------------------------------
    tab_bar = lv_obj_create(screen);
    lv_obj_set_size(tab_bar, SCR_WIDTH, 50); // 높이 50의 상단 바
    lv_obj_set_pos(tab_bar, 0, 0);           // 초기 위치 상단 고정
    lv_obj_set_style_bg_color(tab_bar, lv_color_hex(0x1ABC9C), 0);
    lv_obj_set_style_pad_all(tab_bar, 5, 0);
    
    // 스위프 제스처와 클릭 이벤트를 받기 위해 설정
    lv_obj_add_flag(tab_bar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(tab_bar, ui_event_handler, LV_EVENT_ALL, NULL);

    // 탭바 내부에 들어갈 버튼들 생성 (예시)
    for(int i = 0; i < 3; i++) {
        lv_obj_t * btn = lv_button_create(tab_bar);
        lv_obj_set_size(btn, 100, LV_PCT(100));
        lv_obj_set_pos(btn, i * 110 + 10, 0);
        
        lv_obj_t * btn_label = lv_label_create(btn);
        lv_label_set_text_fmt(btn_label, "Tab %d", i+1);
        lv_obj_center(btn_label);
    }

    // ----------------------------------------------------
    // TIMER: 3초 자동 숨김 타이머 구동
    // ----------------------------------------------------
    hide_timer = lv_timer_create(hide_timer_cb, 3000, NULL);
}




int main()
{
    // 1. Initialize LVGL core
    lv_init();

    // 2. Initialize SDL display (e.g., 800x480 resolution)
    lv_display_t * disp = lv_sdl_window_create(800, 480);
    
    // 3. Initialize SDL input pointer (Mouse / Touchscreen)
    lv_indev_t * mouse = lv_sdl_mouse_create();
    lv_indev_set_display(mouse, disp);

    // 4. Create a basic Hello World widget
    /*
    lv_obj_t * label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "Hello, LVGL + SDL2!");
    lv_obj_center(label);
    */
    init_vnc_display_ui();

    // 5. Main Execution Loop
    while(1) {
        uint32_t time_till_next = lv_timer_handler();
        usleep(time_till_next * 1000); 
    }

    return 0;
}