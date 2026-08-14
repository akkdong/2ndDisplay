// sdl_init.h
//

#pragma once

#define SDL_MAIN_HANDLED 
#include <windows.h>
#include <SDL2/SDL.h>

#include "lvgl.h"

#define ENABLE_SDL_FEATURE	1

#define DISPLAY_WIDTH		800
#define DISPLAY_HEIGHT		480



#ifdef __cplusplus
extern "C"
{
#endif

//
//
//

void sdl_init();


void mouse_read_cb(lv_indev_t* indev, lv_indev_data_t* data);
void lcd_flush_cb(lv_display_t* display, const lv_area_t* area, uint8_t* px_map);


#ifdef __cplusplus
}
#endif
