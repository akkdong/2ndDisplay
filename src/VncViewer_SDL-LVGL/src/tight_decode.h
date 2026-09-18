// tight_decode.h
//

#pragma once

#include <stdint.h>
#include "vncDefines.h"



//
//
//

#if defined (__cplusplus)
extern "C"
{
#endif

//
uint32_t pixel_to_32bit(const uint8_t* p, const PixelFormat* fmt);

//
uint32_t tight_pixel_to_32bit(const uint8_t* p, const PixelFormat* fmt, size_t tpb);
void tight_render_pixels(const uint8_t* pix, const PixelFormat* fmt, size_t tpb,
    int rw, int rh, uint32_t* fb, int fbw, int rx, int ry);
void tight_render_palette(const uint8_t* idx_data, int bits,
    int rw, int rh, const uint32_t* palette, uint32_t* fb, int fbw, int rx, int ry);    
void tight_filter_gradient(uint8_t* data, int w, int h, int bpp);

#if defined (__cplusplus)
}
#endif
