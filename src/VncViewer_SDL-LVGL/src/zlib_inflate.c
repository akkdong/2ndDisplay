// zlib_inflate.c
//

#include "zlib_inflate.h"




//
//
//

/*
bool zlib_decompress(z_stream* zs, const uint8_t* in, size_t inlen, uint8_t* out, size_t outlen, size_t* decompressed_size)
{
    if (inlen > (size_t)UINT_MAX || outlen > (size_t)UINT_MAX)
    {
        LV_LOG_ERROR("zlib_decompress error: out of range\n");
        return false;
    }

    zs->next_in = (uint8_t*)in;
    zs->avail_in = (uInt)inlen;
    zs->next_out = out;
    zs->avail_out = (uInt)outlen;

    while (zs->avail_in > 0)
    {
        uInt prev_avail_out = zs->avail_out;
        int ret = inflate(zs, Z_NO_FLUSH);

        if (ret == Z_NEED_DICT || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR)
        {
            LV_LOG_ERROR("zlib_decompress error: %d\n", ret);
            return false;
        }

        if (ret == Z_STREAM_END)
            break;

        if (zs->avail_out == prev_avail_out)
        {
            LV_LOG_ERROR("zlib_decompress error: zs->avail_out != prev_avail_out\n");
            return false;
        }
    }

    if (decompressed_size != NULL)
        *decompressed_size = outlen - zs->avail_out;

    return true;
}
*/

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


