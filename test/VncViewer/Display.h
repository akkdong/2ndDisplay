// Display.h
//

#pragma once

#include <string>
#include <queue>
#include <memory>
#include <atomic>
#include <SDL2/SDL.h>

#include "lvgl/lvgl.h"

#define TAB_BAR_HEIGHT 50


//
//
//

class VncClient;



//
//
//

class Display
{
    friend class VncClient;

protected:
    Display();
public:
    virtual ~Display();
   
    enum EventType {
        EVT_NONE = 0,
        EVT_CONNECTED,
        EVT_NEGOTIATION_ESTABLISHED,
        EVT_UPDATE_FRAMEBUFFER,
        EVT_DISCONNECTED,
        EVT_CONNECTION_FAILED,
        EVT_HANDSHAKE_FAILED,
    };

public:
    static Display& Get();

    bool begin(std::string& host, int port, std::string& password);
    bool loop();

protected:
    operator lv_display_t*() {
        return m_disp;
    }

    //
    static void animate_y(lv_obj_t *obj, int32_t start, int32_t end, uint32_t duration);
    static void toggle_tab_bar(void);
    static void hide_timer_cb(lv_timer_t *timer);
    static void ui_event_handler(lv_event_t *e);
    static void create_vnc_ui(void *buf, int32_t w, int32_t h);

    //
    static int VncClientWorker(void* data);
    static void OnEvent(void* userData);

    void pushEvent(EventType event);
    EventType popEvent();

protected:
    //
    lv_display_t* m_disp;
    lv_indev_t* m_mouse;

    static lv_obj_t *s_canvas;
    static lv_obj_t *s_tab_bar ;
    static lv_obj_t *s_settings_panel;
    static lv_timer_t *s_hide_timer;

    // Tab bar fully visible / fully hidden target Y coordinates
    static const int32_t Y_VISIBLE = 0;
    static const int32_t Y_HIDDEN  = -TAB_BAR_HEIGHT;


    //
    std::shared_ptr<VncClient> m_clientPtr;
    std::atomic<bool> m_clientRunning;

    std::string m_hostAddr;
    int m_hostPort;
    std::string m_password;

    std::queue<EventType> m_eventQueue;
    SDL_mutex* m_eventMutex;


};
