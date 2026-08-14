// sdl_init.c
//


#include "sdl_init.h"
#include <stdint.h>
#include <stdbool.h>

//
// Shared video memory (VRAM) bridge
//

typedef uint16_t lcd_color_t;

lcd_color_t shared_fbuffer[DISPLAY_WIDTH * DISPLAY_HEIGHT];
CRITICAL_SECTION vram_lock; // Native Windows lock accessible by both contexts

//
// Shared Mouse Device Matrix
//

typedef struct {
    int16_t x;
    int16_t y;
    bool is_pressed;
} sim_mouse_t;

sim_mouse_t shared_mouse = { 0, 0, false };
CRITICAL_SECTION mouse_lock; // Protect mouse data across threads





//
//
//

DWORD WINAPI SdlHardwareLcdThread(LPVOID lpParam)
{
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("MCU LCD Simulator", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, DISPLAY_WIDTH, DISPLAY_HEIGHT, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    // Create a streaming texture to quickly copy raw pixel grids
    SDL_Texture* lcd_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, DISPLAY_WIDTH, DISPLAY_HEIGHT);

    SDL_Event event;
    int running = 1;

    while (running) 
    {
        while (SDL_PollEvent(&event)) 
        {
            if (event.type == SDL_QUIT)
            {
                running = 0;
            }
            // Capture Mouse Coordinates and Action States
            else if (event.type == SDL_MOUSEMOTION) 
            {
                EnterCriticalSection(&mouse_lock);
                shared_mouse.x = event.motion.x;
                shared_mouse.y = event.motion.y;
                LeaveCriticalSection(&mouse_lock);
            }
            else if (event.type == SDL_MOUSEBUTTONDOWN) 
            {
                if (event.button.button == SDL_BUTTON_LEFT) 
                {
                    EnterCriticalSection(&mouse_lock);
                    shared_mouse.is_pressed = true;
                    printf("LButtonDown(%d, %d)\n", shared_mouse.x, shared_mouse.y);
                    LeaveCriticalSection(&mouse_lock);
                }
            }
            else if (event.type == SDL_MOUSEBUTTONUP) 
            {
                if (event.button.button == SDL_BUTTON_LEFT) 
                {
                    EnterCriticalSection(&mouse_lock);
                    shared_mouse.is_pressed = false;
                    printf("LButtonUp(%d, %d)\n", shared_mouse.x, shared_mouse.y);
                    LeaveCriticalSection(&mouse_lock);
                }
            }
        }

        // --- Read from Shared VRAM safely ---
        EnterCriticalSection(&vram_lock);
        SDL_UpdateTexture(lcd_texture, NULL, shared_fbuffer, DISPLAY_WIDTH * sizeof(lcd_color_t));
        LeaveCriticalSection(&vram_lock);

        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, lcd_texture, NULL, NULL);
        SDL_RenderPresent(renderer);

        Sleep(16); // Emulate hardware panel 60Hz refresh rate
    }

    SDL_DestroyTexture(lcd_texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    //
    exit(0);

    return 0;
}




void sdl_init()
{
    //
    InitializeCriticalSection(&vram_lock);
    InitializeCriticalSection(&mouse_lock);

    // Launch the hardware simulator thread natively on Windows
    CreateThread(NULL, 0, SdlHardwareLcdThread, NULL, 0, NULL);
}




// LVGL Input Processing Callback
void mouse_read_cb(lv_indev_t* indev, lv_indev_data_t* data) 
{
    EnterCriticalSection(&mouse_lock);

    // Copy coordinates into the LVGL pointer packet
    data->point.x = shared_mouse.x;
    data->point.y = shared_mouse.y;

    // Convert boolean flags to LVGL v9 State enums
    if (shared_mouse.is_pressed) 
    {
        data->state = LV_INDEV_STATE_PRESSED;
    }
    else 
    {
        data->state = LV_INDEV_STATE_RELEASED;
    }

    LeaveCriticalSection(&mouse_lock);
}


// LVGL Display Flush Callback (Treating SDL thread like a DMA block transfer)
void lcd_flush_cb(lv_display_t* display, const lv_area_t* area, uint8_t* px_map) 
{
    int32_t x, y;
    lcd_color_t* out_pixels = (lcd_color_t*)shared_fbuffer;
    lcd_color_t* in_pixels = (lcd_color_t*)px_map;

    EnterCriticalSection(&vram_lock); // Lock VRAM while flashing
    for (y = area->y1; y <= area->y2; y++) {
        for (x = area->x1; x <= area->x2; x++) {
            // Map the internal LVGL buffer coordinates directly to our shared display matrix
            out_pixels[y * DISPLAY_WIDTH + x] = *in_pixels;
            in_pixels++;
        }
    }
    LeaveCriticalSection(&vram_lock);

    // Tell LVGL that the MCU flushing handling is completed
    lv_display_flush_ready(display);
}