// vncDefines.h
//

#pragma once

#include <stdint.h>



//
//
//

#pragma pack(push, 1)

typedef struct PixelFormat_t
{
    uint8_t bpp;
    uint8_t depth;
    uint8_t big_endian;
    uint8_t true_color;
    uint16_t red_max;
    uint16_t green_max;
    uint16_t blue_max;
    uint8_t red_shift;
    uint8_t green_shift;
    uint8_t blue_shift;
    uint8_t pad[3];
} PixelFormat;

typedef struct ServerInit_t
{
    uint16_t fb_width;
    uint16_t fb_height;
    PixelFormat fmt;
    uint32_t name_len;
} ServerInit;

#pragma pack(pop)


enum MessageType_e
{
    Message_Unknown = -1,
    Message_FrameBufferUpdate = 0,
    Message_SetColourMapEntries = 1,
    Message_Bell = 2,
    Message_ServerCutText = 3,
};

