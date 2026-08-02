// Display.h
//

#pragma once

#include <string>
#include <queue>
#include <vector>
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

    // Terminal-state handler: destroys the LVGL display so that
    // lv_display_get_default() becomes NULL and main() exits its loop.
    static void closeDisplay(Display* display);

    void pushEvent(EventType event);
    EventType popEvent();
    bool hasEvents() const;

    // Copy the decoded frame from the VNC worker into the canvas buffer
    // under m_frameMutex (called from the worker thread).
    void publishFrame(const uint32_t *src, size_t count);

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

    // Framebuffer backing the LVGL canvas. Guarded by m_frameMutex:
    // the VNC worker copies into it (publishFrame), the main thread
    // renders it inside Display::loop().
    std::vector<uint32_t> m_frameBuf;
    SDL_mutex* m_frameMutex;


};
