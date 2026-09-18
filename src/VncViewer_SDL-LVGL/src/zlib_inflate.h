// zlib_inflate.h
//

#pragma once

#include <stdint.h>
#include <stdbool.h>
#if USE_ZLIB
#include "zlib.h"
#else
#include "miniz/miniz.h"
#endif



//
//
//

#if defined (__cplusplus)
extern "C"
{
#endif


/*
bool zlib_decompress(z_stream* zs, const uint8_t* in, size_t inlen, uint8_t* out, size_t outlen, size_t* decompressed_size);
*/

bool zlib_decompress_exact(z_stream* zs, const uint8_t* in, size_t inlen, uint8_t* out, size_t outlen);
int zlib_inflate_exact(z_stream* zs, const uint8_t* in, size_t inlen, uint8_t* out, size_t outlen);


#if defined (__cplusplus)
}
#endif
