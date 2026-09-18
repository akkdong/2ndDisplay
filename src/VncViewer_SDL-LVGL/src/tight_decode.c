// tight_decode.c
//

#include "tight_decode.h"


//
//
//

static uint32_t pixel_to_32bit_b(const uint8_t* p, const PixelFormat* fmt, int bytes)
{
    uint32_t raw;
    if (bytes == 1)
        raw = p[0];
    else if (bytes == 2)
        raw = ((uint32_t)(p[1]) << 8) | p[0];
    else if (bytes == 3)
        raw = ((uint32_t)(p[2]) << 16) | ((uint32_t)(p[1]) << 8) | p[0];
    else
        raw = ((uint32_t)(p[3]) << 24) | ((uint32_t)(p[2]) << 16) | ((uint32_t)(p[1]) << 8) | p[0];

    if (!fmt->true_color)
        return raw;

    uint8_t r = (raw >> fmt->red_shift) & fmt->red_max;
    uint8_t g = (raw >> fmt->green_shift) & fmt->green_max;
    uint8_t b = (raw >> fmt->blue_shift) & fmt->blue_max;

    if (fmt->red_max != 0xFF)
        r = (r * 255 + fmt->red_max / 2) / fmt->red_max;
    if (fmt->green_max != 0xFF)
        g = (g * 255 + fmt->green_max / 2) / fmt->green_max;
    if (fmt->blue_max != 0xFF)
        b = (b * 255 + fmt->blue_max / 2) / fmt->blue_max;

    return 0xFF000000 | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | b;
}

uint32_t pixel_to_32bit(const uint8_t* p, const PixelFormat* fmt)
{
    return pixel_to_32bit_b(p, fmt, fmt->bpp / 8);
}




//
//
//

/* Tight depth-24 wire pixels are R,G,B component bytes (not a native LE pixel). */
uint32_t tight_pixel_to_32bit(const uint8_t* p, const PixelFormat* fmt, size_t tpb)
{
    if (tpb == 3)
        return 0xFF000000u | ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
    return pixel_to_32bit_b(p, fmt, (int)tpb);
}

void tight_render_pixels(const uint8_t* pix, const PixelFormat* fmt, size_t tpb,
    int rw, int rh, uint32_t* fb, int fbw, int rx, int ry)
{
    for (int y = 0; y < rh; ++y) {
        const uint8_t* src = pix + (size_t)y * (size_t)rw * tpb;
        uint32_t* dst = fb + (size_t)(ry + y) * fbw + rx;
        for (int x = 0; x < rw; ++x) {
            dst[x] = tight_pixel_to_32bit(src, fmt, tpb);
            src += tpb;
        }
    }
}

void tight_render_palette(const uint8_t* idx_data, int bits,
    int rw, int rh, const uint32_t* palette, uint32_t* fb, int fbw, int rx, int ry)
{
    size_t row_bytes = ((size_t)rw * bits + 7) / 8;
    unsigned mask = (1u << bits) - 1;
    for (int y = 0; y < rh; ++y) {
        const uint8_t* rp = idx_data + (size_t)y * row_bytes;
        uint32_t* dst = fb + (size_t)(ry + y) * fbw + rx;
        for (int x = 0; x < rw; ++x) {
            size_t bitpos = (size_t)x * bits;
            dst[x] = palette[(rp[bitpos >> 3] >> (8 - bits - (bitpos & 7))) & mask];
        }
    }
}


// Apply Tight Gradient filter (type 1) to a full buffer of pixel data
void tight_filter_gradient(uint8_t* data, int w, int h, int bpp)
{
    int stride = w * bpp;
    for (int y = 0; y < h; ++y) 
    {
        uint8_t* row = data + y * stride;
        uint8_t* prev = (y > 0) ? data + (y - 1) * stride : NULL;
        for (int x = 0; x < w; ++x) 
        {
            for (int c = 0; c < bpp; ++c) 
            {
                uint8_t* p = &row[x * bpp + c];
                int pred;

                if (x == 0 && prev == NULL)
                    pred = 0;
                else if (x == 0)
                    pred = prev[c];
                else if (prev == NULL)
                    pred = row[(x - 1) * bpp + c];
                else
                    pred = row[(x - 1) * bpp + c] + prev[x * bpp + c] - prev[(x - 1) * bpp + c];

                if (pred < 0)
                    pred = 0;
                if (pred > 255)
                    pred = 255;

                *p = (uint8_t)(*p + pred);
            }
        }
    }
}
