// tight_decoder.c
//

#include "tight_decoder.h"
#include "zlib.h"


// ============================================================
// Tight decoder
// ============================================================

static inline uint32_t read_pixel(const uint8_t* buf, int bpp)
{
    if (bpp == 1) 
        return buf[0];
    if (bpp == 2) 
        return ((uint32_t)(buf[1]) << 8) | buf[0];
    if (bpp == 3) 
        return ((uint32_t)(buf[2]) << 16) | ((uint32_t)(buf[1]) << 8) | buf[0];

    return ((uint32_t)(buf[3]) << 24) | ((uint32_t)(buf[2]) << 16) | ((uint32_t)(buf[1]) << 8) | buf[0];
}

static inline void write_pixel(uint8_t* buf, uint32_t pixel, int bpp)
{
    if (bpp == 1) { buf[0] = pixel & 0xFF; }
    else if (bpp == 2) { buf[0] = pixel & 0xFF; buf[1] = (pixel >> 8) & 0xFF; }
    else if (bpp == 3) { buf[0] = pixel & 0xFF; buf[1] = (pixel >> 8) & 0xFF; buf[2] = (pixel >> 16) & 0xFF; }
    else { buf[0] = pixel & 0xFF; buf[1] = (pixel >> 8) & 0xFF; buf[2] = (pixel >> 16) & 0xFF; buf[3] = (pixel >> 24) & 0xFF; }
}



//
//
//

uint32_t pixel_to_32bit(const uint8_t* p, const PixelFormat* fmt)
{
    uint32_t raw = read_pixel(p, fmt->bpp / 8);
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

                *p = (uint8_t)(*p + pred);
            }
        }
    }
}

bool zlib_decompress(z_stream* zs, const uint8_t* in, size_t inlen, uint8_t* out, size_t outlen)
{
    zs->next_in = (uint8_t*)in;
    zs->avail_in = inlen;
    zs->next_out = out;
    zs->avail_out = outlen;

    int ret = inflate(zs, Z_SYNC_FLUSH);
    if (ret != Z_OK && ret != Z_STREAM_END) 
    {
        //std::cerr << "inflate error: " << ret << std::endl;
        return false;
    }

    return true;
}

bool zlib_decompress2(z_stream* zs, const uint8_t* in, size_t inlen, uint8_t* out, size_t outlen, size_t* decompressed_size)
{
    zs->next_in = (uint8_t*)in;
    zs->avail_in = inlen;
    zs->next_out = out;
    zs->avail_out = outlen;

    size_t decomp_size = 0;
    while (zs->avail_in > 0)
    {
        int ret = inflate(zs, Z_NO_FLUSH);
        if (ret == Z_NEED_DICT || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR)
            return false;

        size_t decomp_bytes = outlen - zs->avail_out;
        decomp_size += decomp_bytes;

        zs->next_out = out + decomp_size;
        zs->avail_out = outlen - decomp_size;

        if (ret == Z_STREAM_END) 
        {
            break;
        }
    }

    if (decompressed_size != NULL) 
        *decompressed_size = decomp_size;

    return true;
    /*
    int ret = inflate(zs, Z_SYNC_FLUSH);
    if (ret != Z_OK && ret != Z_STREAM_END)
    {
        //std::cerr << "inflate error: " << ret << std::endl;
        return false;
    }

    if (decompressed_size != NULL) {
        *decompressed_size = outlen - zs->avail_out;
    }

    return true;
    */
}