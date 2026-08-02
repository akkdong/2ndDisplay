// Display.cpp
//

#include <iostream>
#include "Display.h"
#include "VncViewer.h"



//
//
//

lv_obj_t* Display::s_canvas = nullptr;
lv_obj_t* Display::s_tab_bar = nullptr;
lv_obj_t* Display::s_settings_panel = nullptr;
lv_timer_t* Display::s_hide_timer = nullptr;


Display::Display() 
    : m_disp(nullptr)
    , m_mouse(nullptr)
    , m_clientPtr(nullptr)
    , m_clientRunning(false)
    , m_hostAddr("127.0.0.1")
    , m_hostPort(5900)
    , m_password("")
    , m_eventQueue()
    , m_eventMutex(nullptr)
{
    m_eventMutex = SDL_CreateMutex();
}

Display::~Display()
{
    SDL_DestroyMutex(m_eventMutex);
}



Display& Display::Get()
{
    static Display disp;
    return disp;
}


bool Display::begin(std::string& host, int port, std::string& password)
{
    if (m_disp)
        return false;

    //
    lv_init();

    m_disp = lv_sdl_window_create(800, 600);
    m_mouse = lv_sdl_mouse_create();
    lv_indev_set_display(m_mouse, m_disp);

    //
    {
        lv_obj_t *screen = lv_screen_active();

        lv_obj_set_style_bg_color(screen, lv_color_hex(0x1E1E2E), 0);
    }


    //
    m_clientPtr = std::make_shared<VncClient>(this);
    if (m_clientPtr) {
        /*
        if (!m_clientPtr->connect(host, port)) {
            std::cerr << "Connection failed" << std::endl;
            return 1;
        }
        std::cout << "Connected to " << host << ":" << port << std::endl;

        if (!m_clientPtr->handshake(password)) {
            std::cerr << "Handshake failed" << std::endl;
            return 1;
        }

        std::cout << "Enter VncClient::run()" << std::endl;
        m_clientPtr->run();
        std::cout << "Exit VncClient::run()" << std::endl;
        */

        m_hostAddr = host;
        m_hostPort = port;
        m_password = password;

        SDL_CreateThread(VncClientWorker, "VncClient", (void *)this);

        return true;
    }

    return false;
}

bool Display::loop()
{
    uint32_t ms = lv_timer_handler();
    lv_sleep_ms(ms);

    return true;
}

int Display::VncClientWorker(void* data)
{
    Display* display = reinterpret_cast<Display *>(data);
    std::shared_ptr<VncClient> clientPtr = display->m_clientPtr;
    display->m_clientRunning = true;

    if (clientPtr->connect(display->m_hostAddr, display->m_hostPort))
    {
        std::cout << "Connected to " << display->m_hostAddr << ":" << display->m_hostPort << std::endl;
        display->pushEvent(Display::EVT_CONNECTED);
        lv_async_call(Display::OnEvent, display);

        if (clientPtr->handshake(display->m_password))
        {
            display->pushEvent(Display::EVT_NEGOTIATION_ESTABLISHED);
            lv_async_call(Display::OnEvent, display);

            //
            std::cout << "Enter VncClient::run()" << std::endl;
            #if 0
            while (clientPtr && clientPtr->isOk())
                clientPtr->loop();
            #else
            clientPtr->run();
            #endif
            std::cout << "Exit VncClient::run()" << std::endl;            
        }
        else
        {
            std::cerr << "Handshake failed" << std::endl;
            display->pushEvent(Display::EVT_HANDSHAKE_FAILED);
            lv_async_call(Display::OnEvent, display);
        }
    } 
    else
    {
        std::cerr << "Connection failed" << std::endl;
        display->pushEvent(Display::EVT_CONNECTION_FAILED);
        lv_async_call(Display::OnEvent, display);
    }

    display->pushEvent(Display::EVT_DISCONNECTED);
    lv_async_call(Display::OnEvent, display);

    display->m_clientRunning = false;
}

void Display::OnEvent(void* userData)
{
    Display* display = reinterpret_cast<Display *>(userData);
    static int count = 0;
    switch (display->popEvent())
    {
    case Display::EVT_CONNECTION_FAILED:

        break;

    case Display::EVT_NEGOTIATION_ESTABLISHED: {
        auto& ptr = display->m_clientPtr;
        lv_display_set_resolution(display->m_disp, ptr->fbw_, ptr->fbh_);
        create_vnc_ui(ptr->fb_.data(), ptr->fbw_, ptr->fbh_);
        std::cout << "Screen: " << ptr->fbw_ << " x " << ptr->fbh_ << std::endl;
        break;
    }

    case Display::EVT_UPDATE_FRAMEBUFFER: {
        std::cout << "update framebuffer(" << count++ << ")\n";
        lv_obj_invalidate(s_canvas);
        break;
    }
    }
}

void Display::pushEvent(EventType event)
{
    SDL_LockMutex(m_eventMutex);
    m_eventQueue.push(event);
    SDL_UnlockMutex(m_eventMutex);
}

Display::EventType Display::popEvent()
{
    EventType event = EVT_NONE;

    SDL_LockMutex(m_eventMutex);
    if (m_eventQueue.size() > 0)
    {
        event = m_eventQueue.front();
        m_eventQueue.pop();
    }
    SDL_UnlockMutex(m_eventMutex);

    return event;
}







void Display::animate_y(lv_obj_t *obj, int32_t start, int32_t end, uint32_t duration) 
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_values(&a, start, end);
    lv_anim_set_time(&a, duration);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

void Display::toggle_tab_bar(void) 
{
    lv_anim_t *current_anim = lv_anim_get(s_tab_bar, (lv_anim_exec_xcb_t)lv_obj_set_y);

    int32_t current_y = lv_obj_get_y(s_tab_bar);
    int32_t target_y;
    uint32_t duration = 250;

    if (current_anim) {
        // [Case A] animation in progress -> reverse direction
        target_y = (current_anim->end_value == Y_HIDDEN) ? Y_VISIBLE : Y_HIDDEN;
        lv_anim_delete(s_tab_bar, (lv_anim_exec_xcb_t)lv_obj_set_y);
    } else {
        // [Case B] idle -> toggle based on current position
        target_y = (current_y < Y_VISIBLE) ? Y_VISIBLE : Y_HIDDEN;
    }

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_tab_bar);
    lv_anim_set_values(&a, current_y, target_y);
    lv_anim_set_time(&a, duration);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);

    if (target_y == Y_VISIBLE) {
        if (s_hide_timer) {
            lv_timer_reset(s_hide_timer);
            lv_timer_resume(s_hide_timer);
        }
    } else {
        if (s_hide_timer) {
            lv_timer_pause(s_hide_timer);
        }
    }
}

void Display::hide_timer_cb(lv_timer_t *timer) 
{
    int32_t current_y = lv_obj_get_y(s_tab_bar);
    lv_anim_t *current_anim = lv_anim_get(s_tab_bar, (lv_anim_exec_xcb_t)lv_obj_set_y);

    if (current_y <= Y_HIDDEN || (current_anim && current_anim->end_value == Y_HIDDEN)) {
        lv_timer_pause(timer);
        return;
    }

    if (current_anim) {
        lv_anim_delete(s_tab_bar, (lv_anim_exec_xcb_t)lv_obj_set_y);
    }

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_tab_bar);
    lv_anim_set_values(&a, current_y, Y_HIDDEN);
    lv_anim_set_time(&a, 250);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);

    lv_timer_pause(timer);
}

void Display::ui_event_handler(lv_event_t *e) 
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *target = lv_event_get_target_obj(e);

    // Any click on the UI toggles the auto-hiding tab bar
    if (code == LV_EVENT_CLICKED) {
        toggle_tab_bar();
        return;
    }

    // Forward pointer events on the VNC canvas to the remote server
#if SUPPORT_POINTER_FORWARDING
    Display& display = Display::Get();
    VncClient* client = display.m_clientPtr.get();
    if (client)
    {
        if (target == s_canvas && client) {
            lv_indev_t *indev = lv_indev_active();
            if (!indev) return;
            lv_point_t p;
            lv_indev_get_point(indev, &p);
            switch (code) {
            case LV_EVENT_PRESSED:
                client->send_pointer(p.x, p.y, 0x01);
                break;
            case LV_EVENT_PRESSING:
                client->send_pointer(p.x, p.y, 0x01);
                break;
            case LV_EVENT_RELEASED:
                client->send_pointer(p.x, p.y, 0x00);
                break;
            default:
                break;
            }
        }
    }
#endif    
}

void Display::create_vnc_ui(void *buf, int32_t w, int32_t h) 
{
    lv_obj_t *screen = lv_screen_active();

    // ----------------------------------------------------
    // LAYER 1: VNC canvas (full screen, backed by the client framebuffer)
    // ----------------------------------------------------
    s_canvas = lv_canvas_create(screen);
    lv_canvas_set_buffer(s_canvas, buf, w, h, LV_COLOR_FORMAT_ARGB8888);
    lv_obj_align(s_canvas, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_add_flag(s_canvas, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_canvas, ui_event_handler, LV_EVENT_ALL, NULL);

    // ----------------------------------------------------
    // LAYER 2: Settings panel (hidden above the screen)
    // ----------------------------------------------------
    s_settings_panel = lv_obj_create(screen);
    lv_obj_set_size(s_settings_panel, w, h - 100);
    lv_obj_set_pos(s_settings_panel, 0, -h);
    lv_obj_set_style_bg_color(s_settings_panel, lv_color_hex(0x2C3E50), 0);
    lv_obj_set_style_bg_opa(s_settings_panel, LV_OPA_90, 0);
    lv_obj_add_event_cb(s_settings_panel, ui_event_handler, LV_EVENT_ALL, NULL);
    lv_obj_t *set_label = lv_label_create(s_settings_panel);
    lv_label_set_text(set_label, "Settings Menu\n(Swipe UP to close)");
    lv_obj_center(set_label);

    // ----------------------------------------------------
    // LAYER 3: Auto-hiding tab bar
    // ----------------------------------------------------
    s_tab_bar = lv_obj_create(screen);
    lv_obj_set_size(s_tab_bar, w, TAB_BAR_HEIGHT);
    lv_obj_set_pos(s_tab_bar, 0, 0);
    lv_obj_set_style_bg_color(s_tab_bar, lv_color_hex(0x1ABC9C), 0);
    lv_obj_set_style_pad_all(s_tab_bar, 5, 0);
    lv_obj_add_flag(s_tab_bar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_tab_bar, ui_event_handler, LV_EVENT_ALL, NULL);

    for (int i = 0; i < 3; i++) {
        lv_obj_t *btn = lv_button_create(s_tab_bar);
        lv_obj_set_size(btn, 100, LV_PCT(100));
        lv_obj_set_pos(btn, i * 110 + 10, 0);
        lv_obj_t *btn_label = lv_label_create(btn);
        lv_label_set_text_fmt(btn_label, "Tab %d", i + 1);
        lv_obj_center(btn_label);
    }

    // Auto-hide the tab bar 3 seconds after it is shown
    s_hide_timer = lv_timer_create(hide_timer_cb, 3000, NULL);
}
