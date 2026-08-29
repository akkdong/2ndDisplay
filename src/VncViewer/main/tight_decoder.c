// tight_decoder.c
//

#include "tight_decoder.h"
#include <limits.h>
#include "miniz/miniz.h"


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

uint32_t pixel_to_32bit_b(const uint8_t* p, const PixelFormat* fmt, int bytes)
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
    if (inlen > (size_t)UINT_MAX || outlen > (size_t)UINT_MAX)
        return false;

    zs->next_in = (uint8_t*)in;
    zs->avail_in = (uInt)inlen;
    zs->next_out = out;
    zs->avail_out = (uInt)outlen;

    while (zs->avail_in > 0)
    {
        uInt prev_avail_out = zs->avail_out;
        int ret = inflate(zs, Z_NO_FLUSH);

        if (ret == Z_NEED_DICT || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR)
            return false;

        if (ret == Z_STREAM_END)
            break;

        if (zs->avail_out == prev_avail_out)
            return false;
    }

    if (decompressed_size != NULL)
        *decompressed_size = outlen - zs->avail_out;

    return true;
}

bool zlib_decompress_exact(z_stream* zs, const uint8_t* in, size_t inlen, uint8_t* out, size_t outlen)
{
    return zlib_inflate_exact(zs, in, inlen, out, outlen) == Z_OK;
}

int zlib_inflate_exact(z_stream* zs, const uint8_t* in, size_t inlen, uint8_t* out, size_t outlen)
{
    if (inlen > (size_t)UINT_MAX || outlen > (size_t)UINT_MAX)
        return Z_STREAM_ERROR;

    zs->next_in = (Bytef*)in;
    zs->avail_in = (uInt)inlen;
    zs->next_out = out;
    zs->avail_out = (uInt)outlen;

    /*
     * Fill the output, then keep inflating while input remains: the encoder
     * appends a Z_SYNC_FLUSH marker (and block-header bits) after each rect,
     * which inflate can only consume with output space exhausted. Draining
     * them keeps the shared zlib stream aligned for the next rect.
     */
    bool filled = (outlen == 0);
    while (!filled || zs->avail_in > 0)
    {
        uInt prev_in = zs->avail_in;
        uInt prev_out = zs->avail_out;
        int ret = inflate(zs, Z_NO_FLUSH);

        if (ret == Z_STREAM_END)
        {
            if (zs->avail_out == 0)
                filled = true;
            break;
        }

        if (ret != Z_OK && ret != Z_BUF_ERROR)
            return ret;

        if (prev_in == zs->avail_in && prev_out == zs->avail_out)
            break; /* no progress possible */

        if (zs->avail_out == 0)
            filled = true;
    }

    return (filled && zs->avail_out == 0) ? Z_OK : Z_BUF_ERROR;
}