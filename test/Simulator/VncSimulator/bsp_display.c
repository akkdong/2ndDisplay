// bsp_display.c
//

#include <stdio.h>
#if defined(_SIMULATOR)
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "event_groups.h"
#else
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#endif

#include "bsp_display.h"
#include "lvgl/lvgl.h"
#include "SDL2/SDL.h"

#include "vnc_display.h"


//
//
//

typedef struct {
    int16_t x;
    int16_t y;
    bool is_pressed;
} sim_mouse_t;


static SemaphoreHandle_t vram_lock;
static SemaphoreHandle_t mouse_lock;

static int disp_width = 480;
static int disp_height = 800;

static lcd_color_t* shared_fbuffer = NULL;
static sim_mouse_t shared_mouse = { 0, 0, false };


//
//
//

static void bsp_display_task(void* param)
{
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("VNC Simulator", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, disp_width, disp_height, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    // Create a streaming texture to quickly copy raw pixel grids
    SDL_Texture* lcd_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, disp_width, disp_height);

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
                xSemaphoreTake(mouse_lock, portMAX_DELAY);
                shared_mouse.x = event.motion.x;
                shared_mouse.y = event.motion.y;
                xSemaphoreGive(mouse_lock);
            }
            else if (event.type == SDL_MOUSEBUTTONDOWN)
            {
                if (event.button.button == SDL_BUTTON_LEFT)
                {
                    xSemaphoreTake(mouse_lock, portMAX_DELAY);
                    shared_mouse.is_pressed = true;
                    xSemaphoreGive(mouse_lock);
                }
            }
            else if (event.type == SDL_MOUSEBUTTONUP)
            {
                if (event.button.button == SDL_BUTTON_LEFT)
                {
                    xSemaphoreTake(mouse_lock, portMAX_DELAY);
                    shared_mouse.is_pressed = false;
                    xSemaphoreGive(mouse_lock);
                }
            }
        }

        // --- Read from Shared VRAM safely ---
        xSemaphoreTake(vram_lock, portMAX_DELAY);
        SDL_UpdateTexture(lcd_texture, NULL, shared_fbuffer, disp_width * sizeof(lcd_color_t));
        xSemaphoreGive(vram_lock);

        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, lcd_texture, NULL, NULL);
        SDL_RenderPresent(renderer);

        vTaskDelay(pdMS_TO_TICKS(16)); // Emulate hardware panel 60Hz refresh rate
    }

    SDL_DestroyTexture(lcd_texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    //
    free(shared_fbuffer);
    shared_fbuffer = NULL;

    //
    exit(0);
}


// LVGL Input Processing Callback
void bsp_touch_input(lv_indev_t* indev, lv_indev_data_t* data)
{
    xSemaphoreTake(mouse_lock, portMAX_DELAY);

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

    xSemaphoreGive(mouse_lock);
}


// LVGL Display Flush Callback (Treating SDL thread like a DMA block transfer)
void bsp_display_flush(lv_display_t* display, const lv_area_t* area, uint8_t* px_map)
{
    int32_t x, y;
    lcd_color_t* out_pixels = (lcd_color_t*)shared_fbuffer;
    lcd_color_t* in_pixels = (lcd_color_t*)px_map;

    xSemaphoreTake(vram_lock, portMAX_DELAY); // Lock VRAM while flashing
    for (y = area->y1; y <= area->y2; y++) {
        for (x = area->x1; x <= area->x2; x++) {
            // Map the internal LVGL buffer coordinates directly to our shared display matrix
            out_pixels[y * disp_width + x] = *in_pixels;
            in_pixels++;
        }
    }
    xSemaphoreGive(vram_lock);

    // Tell LVGL that the MCU flushing handling is completed
    lv_display_flush_ready(display);
}


//
//
//

void bsp_display_init(vnc_display_t* disp)
{
    vram_lock = xSemaphoreCreateMutex();
    mouse_lock = xSemaphoreCreateMutex();

    disp_width = disp->disp_width;
    disp_height = disp->disp_height;
    shared_fbuffer = malloc(disp_width * disp_height * sizeof(lcd_color_t));

    disp->display_flush = bsp_display_flush;
    disp->touch_input = bsp_touch_input;

    xTaskCreate(bsp_display_task, "bsp_display_task", 2 * 1024, NULL, tskIDLE_PRIORITY + 2, NULL);
}
