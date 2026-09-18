// zrle_decode.h
//

#pragma once

#include <stdint.h>
#include "vncDefines.h"


//
//
//

#if defined(__cplusplus)
extern "C"
{
#endif


int parse_zrle_buffer(const uint8_t* decompressed_buf, size_t buf_size,
    int rect_x, int rect_y, int rect_width, int rect_height,
    uint32_t* screen_buffer, int screen_width, const PixelFormat* fmt);

    

#if defined(__cplusplus)
}
#endif
