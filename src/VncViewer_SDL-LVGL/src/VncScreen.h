// VncScreen.h
//

#pragma once

#include "SDL2/SDL.h"
#include "lvgl.h"


class VncApplication;
class VncScreen;
class VncDisplay;


typedef class VncScreen vnc_screen_t;


//
//
//

class VncScreen
{
    friend class VncDisplay;

public:
    VncScreen(VncApplication* app);
    ~VncScreen();

    enum EventType_e 
    {
        EVENT_WIFI_CONFIG,
        EVENT_CONNECT,
        EVENT_DISCONNECT,
    };

public:
    //
    void setConnectInfo(const char* addr, unsigned short port, const char* pass);

    const char* getServerAddr() const { return m_addr; }
    unsigned short getServerPort() const { return m_port; }
    const char* getPassword() const { return m_pass; }

    //
    void startPlay(int width, int height, int bpp);
    void startPlay();
    void stopPlay();

    void refreshFrame(uint32_t* fb, size_t size);

    //
    static void pointInputCallback(lv_event_t* evt);

protected:
    bool extractAndValidate();
    void shakeButton(lv_obj_t* btn);

private:
    //
    lv_obj_t* createMainLayer(vnc_screen_t* scrn, lv_obj_t* parent);
    lv_obj_t* createCanvasLayer(vnc_screen_t* scrn, lv_obj_t* parent);
    lv_obj_t* createKeyboard(vnc_screen_t* scrn);

    static void onTimer(lv_timer_t* timer);
    static void onEditEvent(lv_event_t * evt);
    static void onClickedWifi(lv_event_t* evt);
    static void onClickedConnect(lv_event_t* evt);
    static void onClickedDisconnect(lv_event_t* evt);
    static void onClickedClearLog(lv_event_t* evt);
    static void onClickedCanvas(lv_event_t* evt);

public:
    static Uint32 EventType;

private:
    //
    char m_addr[32];
    unsigned short m_port;
    char m_pass[64];

    //
    VncApplication* m_app;

    lv_obj_t* m_active;
    lv_obj_t* m_mainLayer;
    lv_obj_t* m_canvas;
    lv_obj_t* m_keyboard;

    uint8_t* m_canvasBuf;
};
