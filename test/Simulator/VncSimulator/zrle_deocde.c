#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define TILE_SZ 64

// 픽셀 스트림에서 3바이트(24depth)를 읽어 32bpp uint32_t 값으로 변환 (엔디안 주의)
static inline uint32_t read_pixel_3bytes(const uint8_t** ptr) {
    uint32_t pixel = ((*ptr)[0] << 16) | ((*ptr)[1] << 8) | (*ptr)[2]; // RGB format 예시
    *ptr += 3;
    return pixel;
}

// 64x64 타일 단위 디코딩 함수
static void decode_zrle_tile(const uint8_t** src, uint32_t* fb, int tile_w, int tile_h, int fb_stride) {
    uint8_t subencoding = **src;
    (*src)++;

    // 1. Solid Color Tile
    if (subencoding == 1) {
        uint32_t color = read_pixel_3bytes(src);
        for (int y = 0; y < tile_h; y++) {
            for (int x = 0; x < tile_w; x++) {
                fb[y * fb_stride + x] = color;
            }
        }
        return;
    }

    // 2. Packed Palette Types (Palette 크기: 2 ~ 16)
    if (subencoding >= 2 && subencoding <= 16) {
        int palette_size = subencoding;
        uint32_t palette[16];
        for (int i = 0; i < palette_size; i++) {
            palette[i] = read_pixel_3bytes(src);
        }

        // 비트 수 결정 (2개면 1비트, 3~4개면 2비트, 5~16개면 4비트)
        int bits_per_pixel = (palette_size <= 2) ? 1 : ((palette_size <= 4) ? 2 : 4);
        int pixels_per_byte = 8 / bits_per_pixel;
        int mask = (1 << bits_per_pixel) - 1;

        for (int y = 0; y < tile_h; y++) {
            for (int x = 0; x < tile_w; ) {
                uint8_t byte = **src;
                (*src)++;

                for (int p = 0; p < pixels_per_byte && x < tile_w; p++) {
                    int shift = 8 - bits_per_pixel * (p + 1);
                    int index = (byte >> shift) & mask;
                    fb[y * fb_stride + x] = palette[index];
                    x++;
                }
            }
        }
        return;
    }

    // 3. Plain RLE (런렝스)
    if (subencoding == 127) {
        int total_pixels = tile_w * tile_h;
        int count = 0;

        while (count < total_pixels) {
            uint32_t color = read_pixel_3bytes(src);

            // 길이(Run Length) 필드 파싱 (멀티바이트 처리 명세 준수)
            int run_len = 1;
            uint8_t b;
            do {
                b = **src;
                (*src)++;
                run_len += b;
            } while (b == 255);

            for (int i = 0; i < run_len && count < total_pixels; i++) {
                int target_x = count % tile_w;
                int target_y = count / tile_w;
                fb[target_y * fb_stride + target_x] = color;
                count++;
            }
        }
        return;
    }

    // 4. Palette RLE
    if (subencoding >= 130 && subencoding <= 144) {
        int palette_size = subencoding - 128;
        uint32_t palette[16];
        for (int i = 0; i < palette_size; i++) {
            palette[i] = read_pixel_3bytes(src);
        }

        int total_pixels = tile_w * tile_h;
        int count = 0;

        while (count < total_pixels) {
            uint8_t index_byte = **src;
            (*src)++;

            int run_len = 1;
            if (index_byte & 128) { // 최상위 비트가 1이면 뒤에 run length가 붙음
                index_byte &= 127;  // 마스킹하여 실제 팔레트 인덱스 추출
                uint8_t b;
                do {
                    b = **src;
                    (*src)++;
                    run_len += b;
                } while (b == 255);
            }

            uint32_t color = palette[index_byte];
            for (int i = 0; i < run_len && count < total_pixels; i++) {
                int target_x = count % tile_w;
                int target_y = count / tile_w;
                fb[target_y * fb_stride + target_x] = color;
                count++;
            }
        }
        return;
    }

    // 5. Raw Pixel Data (subencoding == 0)
    if (subencoding == 0) {
        for (int y = 0; y < tile_h; y++) {
            for (int x = 0; x < tile_w; x++) {
                fb[y * fb_stride + x] = read_pixel_3bytes(src);
            }
        }
        return;
    }
}

// 사각형 영역(Rectangle) 전체를 디코딩하여 Framebuffer에 쓰는 메인 진입점
void decode_zrle_rectangle(const uint8_t* decompressed_data,
    uint32_t* framebuffer,
    int rect_x, int rect_y,
    int rect_w, int rect_h,
    int screen_w)
{
    const uint8_t* src_ptr = decompressed_data;

    // 타일 그리드를 루프 돌며 처리
    for (int ty = rect_y; ty < rect_y + rect_h; ty += TILE_SZ) {
        int th = (rect_y + rect_h - ty < TILE_SZ) ? (rect_y + rect_h - ty) : TILE_SZ;

        for (int tx = rect_x; tx < rect_x + rect_w; tx += TILE_SZ) {
            int tw = (rect_x + rect_w - tx < TILE_SZ) ? (rect_x + rect_w - tx) : TILE_SZ;

            // 현재 타일의 좌측 상단 메모리 주소 계산
            uint32_t* fb_tile_ptr = framebuffer + (ty * screen_w) + tx;

            // 타일 하나 디코딩
            decode_zrle_tile(&src_ptr, fb_tile_ptr, tw, th, screen_w);
        }
    }
}
