// tight_decoder.h
//

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <zlib.h>

#include "vnc_client.h"



//////////////////////////////////////////////////////////////////////////////
//

uint32_t pixel_to_32bit(const uint8_t* p, const PixelFormat* fmt);

void tight_filter_gradient(uint8_t* data, int w, int h, int bpp);
bool zlib_decompress(z_stream* zs, const uint8_t* in, size_t inlen, uint8_t* out, size_t outlen);
