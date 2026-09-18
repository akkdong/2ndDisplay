// VncDisplay.cpp
//

#include "VncDisplay.h"
#include "VncApplication.h"




//
//
//

VncDisplay::VncDisplay(VncApplication* app)
    : m_app(app)
    , m_disp(NULL)
    , m_input(NULL)
{
    //
    lv_init();

    //
    m_disp = lv_sdl_window_create(1024, 768);
    m_input = lv_sdl_mouse_create();

    lv_indev_set_display(m_input, m_disp);

    //
    lv_group_t * g = lv_group_create();
    lv_group_set_default(g);

    lv_indev_t * keyboard = lv_sdl_keyboard_create();
    lv_indev_set_display(keyboard, m_disp);

    lv_indev_set_group(keyboard, g);
}

VncDisplay::~VncDisplay()
{
    // ...
}




void VncDisplay::registerInputCallback(void (*event_cb)(lv_event_t*), void* userData)
{
    lv_indev_t * indev = lv_indev_get_next(NULL);
    LV_LOG("indev = %p\n", indev);
    while(indev) 
    {
        if(lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) 
        {
            LV_LOG("lv_indev_add_event_cb(LV_EVENT_PRESSED), userData=%p\n", userData);
            lv_indev_add_event_cb(indev, event_cb, LV_EVENT_PRESSED, userData);
            break;
        }

        indev = lv_indev_get_next(indev);
    }
}
