// VncDisplay.h
//

#pragma once

#include "lvgl.h"


class VncApplication;
class VncScreen;


//
//
//

class VncDisplay
{
    friend class VncScreen;

public:
    VncDisplay(VncApplication* app);
    ~VncDisplay();

public:
    void registerInputCallback(void (*event_cb)(lv_event_t*), void* userData);

private:
    VncApplication* m_app;
    lv_disp_t* m_disp;
    lv_indev_t* m_input;
};
