// tight_decoder.h
//

#pragma once

#include <stdint.h>
#include <stdbool.h>
#if 0
#include "zlib.h
#else
#include "miniz_loc/miniz.h"
#endif


#include "vnc_client.h"



//////////////////////////////////////////////////////////////////////////////
//

uint32_t pixel_to_32bit(const uint8_t* p, const PixelFormat* fmt);
uint32_t pixel_to_32bit_b(const uint8_t* p, const PixelFormat* fmt, int bytes);

void tight_filter_gradient(uint8_t* data, int w, int h, int bpp);
bool zlib_decompress(z_stream* zs, const uint8_t* in, size_t inlen, uint8_t* out, size_t outlen);
bool zlib_decompress2(z_stream* zs, const uint8_t* in, size_t inlen, uint8_t* out, size_t outlen, size_t* decompressed_size);
bool zlib_decompress_exact(z_stream* zs, const uint8_t* in, size_t inlen, uint8_t* out, size_t outlen);
int zlib_inflate_exact(z_stream* zs, const uint8_t* in, size_t inlen, uint8_t* out, size_t outlen);

