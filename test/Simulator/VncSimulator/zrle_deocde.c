// zrle_deocde.c
//

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "vnc_types.h"

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#define ZRLE_TILE_SIZE          64

#define ZRLE_SUB_PACKED_REUSE   127
#define ZRLE_SUB_PLAIN_RLE      128
#define ZRLE_SUB_PALRLE_REUSE   129

typedef struct {
    const uint8_t* buf;
    size_t size;
    size_t pos;

    const PixelFormat* fmt;
    int cplen;
    int cp_msb;
} zrle_stream;

static uint32_t zrle_palette[128];
static int zrle_palette_size;
static int zrle_palette_valid;

static int zrle_channel_bits(uint16_t max_value)
{
    int bits = 0;
    while (max_value)
    {
        ++bits;
        max_value >>= 1;
    }
    return bits;
}

static void zrle_cpixel_info(const PixelFormat* fmt, int* cplen, int* cp_msb)
{
    *cplen = fmt->bpp / 8;
    *cp_msb = 0;
    if (*cplen > 4)
        *cplen = 4;

    if (fmt->bpp != 32 || !fmt->true_color || fmt->depth > 24)
        return;

    int hi = fmt->red_shift + zrle_channel_bits(fmt->red_max);
    int lo = fmt->red_shift;

    if (fmt->green_shift + zrle_channel_bits(fmt->green_max) > hi)
        hi = fmt->green_shift + zrle_channel_bits(fmt->green_max);
    if (fmt->green_shift < lo)
        lo = fmt->green_shift;
    if (fmt->blue_shift + zrle_channel_bits(fmt->blue_max) > hi)
        hi = fmt->blue_shift + zrle_channel_bits(fmt->blue_max);
    if (fmt->blue_shift < lo)
        lo = fmt->blue_shift;

    if (hi <= 24)
    {
        *cplen = 3;
        *cp_msb = 0;
    }
    else if (lo >= 8)
    {
        *cplen = 3;
        *cp_msb = 1;
    }
}

static int zs_read_u8(zrle_stream* s, uint8_t* v)
{
    if (s->pos >= s->size)
        return 0;
    *v = s->buf[s->pos++];
    return 1;
}

static uint32_t zrle_unpack(const PixelFormat* fmt, uint32_t raw)
{
    uint32_t r = (raw >> fmt->red_shift) & fmt->red_max;
    uint32_t g = (raw >> fmt->green_shift) & fmt->green_max;
    uint32_t b = (raw >> fmt->blue_shift) & fmt->blue_max;

    if (fmt->red_max != 0xFF && fmt->red_max > 0)
        r = (r * 255 + fmt->red_max / 2) / fmt->red_max;
    if (fmt->green_max != 0xFF && fmt->green_max > 0)
        g = (g * 255 + fmt->green_max / 2) / fmt->green_max;
    if (fmt->blue_max != 0xFF && fmt->blue_max > 0)
        b = (b * 255 + fmt->blue_max / 2) / fmt->blue_max;

    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

static int zs_read_cpixel(zrle_stream* s, uint32_t* out)
{
    const uint8_t* p;
    uint32_t raw = 0;

    if (s->pos + (size_t)s->cplen > s->size)
        return 0;

    p = s->buf + s->pos;
    s->pos += (size_t)s->cplen;

    if (s->fmt->big_endian)
    {
        for (int i = 0; i < s->cplen; ++i)
            raw = (raw << 8) | p[i];
    }
    else
    {
        for (int i = s->cplen - 1; i >= 0; --i)
            raw = (raw << 8) | p[i];
    }

    if (s->cplen == 3 && s->cp_msb)
        raw <<= 8;

    if (!s->fmt->true_color)
    {
        *out = raw;
        return 1;
    }

    *out = zrle_unpack(s->fmt, raw);
    return 1;
}

static int zs_read_run_length(zrle_stream* s, size_t* run_length)
{
    size_t sum = 0;
    uint8_t b;

    do
    {
        if (!zs_read_u8(s, &b))
            return 0;
        sum += b;
    } while (b == 255);

    *run_length = sum + 1;
    return 1;
}

static uint32_t* zrle_pixel_at(uint32_t* fb, int stride, int tx, int ty, int lx, int ly)
{
    return fb + (size_t)(ty + ly) * (size_t)stride + (tx + lx);
}

static void zrle_fill_run(uint32_t* fb, int stride, int tx, int ty, int tw,
    uint32_t color, int* done, int total, size_t run_length)
{
    while (run_length-- > 0 && *done < total)
    {
        int lx = *done % tw;
        int ly = *done / tw;
        *zrle_pixel_at(fb, stride, tx, ty, lx, ly) = color;
        ++(*done);
    }
}

static int zrle_tile_raw(zrle_stream* s, uint32_t* fb, int stride, int tx, int ty, int tw, int th)
{
    for (int y = 0; y < th; ++y)
    {
        for (int x = 0; x < tw; ++x)
        {
            uint32_t color;
            if (!zs_read_cpixel(s, &color))
                return 0;
            *zrle_pixel_at(fb, stride, tx, ty, x, y) = color;
        }
    }
    return 1;
}

static int zrle_tile_solid(zrle_stream* s, uint32_t* fb, int stride, int tx, int ty, int tw, int th)
{
    uint32_t color;

    if (!zs_read_cpixel(s, &color))
        return 0;

    for (int y = 0; y < th; ++y)
        for (int x = 0; x < tw; ++x)
            *zrle_pixel_at(fb, stride, tx, ty, x, y) = color;

    return 1;
}

static int zrle_load_palette(zrle_stream* s, int palette_size)
{
    if (palette_size < 0 || palette_size > 128)
        return 0;

    if (palette_size > 0)
    {
        for (int i = 0; i < palette_size; ++i)
        {
            if (!zs_read_cpixel(s, &zrle_palette[i]))
                return 0;
        }
        zrle_palette_size = palette_size;
        zrle_palette_valid = 1;
    }

    return zrle_palette_valid && zrle_palette_size > 0;
}

static int zrle_tile_packed(zrle_stream* s, uint32_t* fb, int stride,
    int tx, int ty, int tw, int th, int palette_size)
{
    int ps;
    int bits;
    uint8_t mask;

    if (!zrle_load_palette(s, palette_size))
        return 0;
    ps = zrle_palette_size;

    bits = (ps == 2) ? 1 : ((ps <= 4) ? 2 : 4);
    mask = (uint8_t)((1u << bits) - 1);

    for (int y = 0; y < th; ++y)
    {
        int x = 0;
        while (x < tw)
        {
            uint8_t v;
            if (!zs_read_u8(s, &v))
                return 0;

            for (int shift = 8 - bits; shift >= 0 && x < tw; shift -= bits, ++x)
            {
                uint8_t idx = (uint8_t)((v >> shift) & mask);
                uint32_t color = (idx < ps) ? zrle_palette[idx] : 0xFF000000u;
                *zrle_pixel_at(fb, stride, tx, ty, x, y) = color;
            }
        }
    }
    return 1;
}

static int zrle_tile_plain_rle(zrle_stream* s, uint32_t* fb, int stride,
    int tx, int ty, int tw, int th)
{
    int total = tw * th;
    int done = 0;

    while (done < total)
    {
        uint32_t color;
        size_t run_length;

        if (!zs_read_cpixel(s, &color) || !zs_read_run_length(s, &run_length))
            return 0;

        zrle_fill_run(fb, stride, tx, ty, tw, color, &done, total, run_length);
    }
    return 1;
}

static int zrle_tile_palette_rle(zrle_stream* s, uint32_t* fb, int stride,
    int tx, int ty, int tw, int th, int palette_size)
{
    int ps;
    int total = tw * th;
    int done = 0;

    if (!zrle_load_palette(s, palette_size))
        return 0;
    ps = zrle_palette_size;

    while (done < total)
    {
        uint8_t v;
        if (!zs_read_u8(s, &v))
            return 0;

        {
            uint8_t idx = (uint8_t)(v & 0x7F);
            size_t run_length = 1;

            if (v & 0x80)
            {
                if (!zs_read_run_length(s, &run_length))
                    return 0;
            }

            {
                uint32_t color = (idx < ps) ? zrle_palette[idx] : 0xFF000000u;
                zrle_fill_run(fb, stride, tx, ty, tw, color, &done, total, run_length);
            }
        }
    }
    return 1;
}

static int zrle_decode_tile(zrle_stream* s, uint32_t* fb, int stride,
    int tx, int ty, int tw, int th)
{
    uint8_t subencoding;

    if (!zs_read_u8(s, &subencoding))
        return 0;

    if (subencoding == 0)
        return zrle_tile_raw(s, fb, stride, tx, ty, tw, th);

    if (subencoding == 1)
        return zrle_tile_solid(s, fb, stride, tx, ty, tw, th);

    if (subencoding >= 2 && subencoding <= 16)
        return zrle_tile_packed(s, fb, stride, tx, ty, tw, th, subencoding);

    if (subencoding == ZRLE_SUB_PACKED_REUSE)
        return zrle_tile_packed(s, fb, stride, tx, ty, tw, th, 0);

    if (subencoding == ZRLE_SUB_PLAIN_RLE)
        return zrle_tile_plain_rle(s, fb, stride, tx, ty, tw, th);

    if (subencoding == ZRLE_SUB_PALRLE_REUSE)
        return zrle_tile_palette_rle(s, fb, stride, tx, ty, tw, th, 0);

    if (subencoding >= 130)
        return zrle_tile_palette_rle(s, fb, stride, tx, ty, tw, th, subencoding - 128);

    fprintf(stderr, "ZRLE: unsupported subencoding %u\n", (unsigned)subencoding);
    return 0;
}

int parse_zrle_buffer(const uint8_t* decompressed_buf, size_t buf_size,
    int rect_x, int rect_y, int rect_width, int rect_height,
    uint32_t* screen_buffer, int screen_width, const PixelFormat* fmt)
{
    zrle_stream s;

    if (!decompressed_buf || !screen_buffer || !fmt)
        return 0;

    s.buf = decompressed_buf;
    s.size = buf_size;
    s.pos = 0;
    s.fmt = fmt;
    zrle_cpixel_info(fmt, &s.cplen, &s.cp_msb);

    zrle_palette_size = 0;
    zrle_palette_valid = 0;

    for (int ty = rect_y; ty < rect_y + rect_height; ty += ZRLE_TILE_SIZE)
    {
        int th = MIN(ZRLE_TILE_SIZE, rect_y + rect_height - ty);

        for (int tx = rect_x; tx < rect_x + rect_width; tx += ZRLE_TILE_SIZE)
        {
            int tw = MIN(ZRLE_TILE_SIZE, rect_x + rect_width - tx);

            if (!zrle_decode_tile(&s, screen_buffer, screen_width, tx, ty, tw, th))
            {
                fprintf(stderr, "ZRLE: corrupt/truncated stream at %zu/%zu (tile %d,%d)\n",
                    s.pos, s.size, tx, ty);
                return 0;
            }
        }
    }

    return 1;
}
