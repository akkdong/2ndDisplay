#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// 32비트 픽셀 구조체 (24 depth 유효)
typedef struct {
    uint8_t b;
    uint8_t g;
    uint8_t r;
    uint8_t a; // 24 depth일 경우 더미 또는 알파 채널
} Pixel32;

// 최소값 계산 매크로
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

// 가변 길이 Run-Length 파싱 함수
// 마지바이트(0xFF)가 아닐 때까지 누적합을 구함
size_t read_run_length(const uint8_t* buffer, size_t* ptr) {
    size_t run_count = 0;
    uint8_t b = 0, i = 0;
    do
    {
        uint8_t b = buffer[(*ptr)++];
        run_count += b;
        ++i;
    } while (b == 255);

    if (i > 3)
    {
        printf("i = %d\n", i);
    }
    return run_count + 1; // 명세상 실제 반복 횟수는 누적합 + 1
}

// 4바이트 단위로 픽셀을 읽는 함수
Pixel32 read_pixel32(const uint8_t* buffer, size_t* ptr) {
    Pixel32 p;
    p.b = buffer[(*ptr)++]; // 32비트 포맷이므로 반드시 4바이트 파싱
    p.g = buffer[(*ptr)++];
    p.r = buffer[(*ptr)++];
    p.a = buffer[(*ptr)++];
    return p;
}

// 화면 버퍼(1차원 배열)의 특정 (x, y) 좌표에 픽셀을 쓰는 함수
void write_pixel_to_screen(Pixel32* screen, int screen_width, int x, int y, Pixel32 color) {
    screen[y * screen_width + x] = color;
}

static Pixel32 palette[127]; // 최대 팔레트 크기인 127개로 정적 배열 할당 (속도 최적화)

// ZRLE 압축 해제된(Inflated) 메모리 스트림 파싱 핵심 함수
void parse_zrle_buffer(const uint8_t* decompressed_buf, size_t buf_size,
    int rect_x, int rect_y, int rect_width, int rect_height,
    Pixel32* screen_buffer, int screen_width) {
    size_t ptr = 0;

    // 64x64 타일 단위 루프
    for (int ty = rect_y; ty < rect_y + rect_height; ty += 64) {
        for (int tx = rect_x; tx < rect_x + rect_width; tx += 64) {

            // 이미지 경계면에 걸친 타일의 실제 가로/세로 크기 계산
            int tw = MIN(64, rect_x + rect_width - tx);
            int th = MIN(64, rect_y + rect_height - ty);
            int total_pixels = tw * th;

            if (ptr >= buf_size) 
                return;

            // 1바이트 서브인코딩 타입 읽기
            uint8_t subencoding = decompressed_buf[ptr++];

            //Pixel32 c;
            //c.r = rand() % 256;
            //c.g = rand() % 256;
            //c.b = rand() % 256;
            //c.a = 0xFF;

            // --- Case 1: Raw 데이터 (Type = 0) ---
            if (subencoding == 0) {
                for (int y = 0; y < th; ++y) {
                    for (int x = 0; x < tw; ++x) {
                        Pixel32 color = read_pixel32(decompressed_buf, &ptr);
                        write_pixel_to_screen(screen_buffer, screen_width, tx + x, ty + y, color);
                    }
                }
            }
            // --- Case 2: 단색 타일 (Type = 1) ---
            else if (subencoding == 1) {
                Pixel32 color = read_pixel32(decompressed_buf, &ptr);
                for (int y = 0; y < th; ++y) {
                    for (int x = 0; x < tw; ++x) {
                        write_pixel_to_screen(screen_buffer, screen_width, tx + x, ty + y, color);
                    }
                }
            }
            // --- Case 3: 팔레트 비트 패킹 (Type = 2 ~ 16) ---
            else if (subencoding >= 2 && subencoding <= 16) {
                int palette_size = subencoding;
                for (int i = 0; i < palette_size; ++i) {
                    palette[i] = read_pixel32(decompressed_buf, &ptr);
                }

                // 픽셀당 필요 비트 수 계산 (2개: 1비트, 3~4개: 2비트, 5~16개: 4비트)
                int bits_per_pixel = (palette_size == 2) ? 1 : ((palette_size <= 4) ? 2 : 4);
                uint8_t mask = (1 << bits_per_pixel) - 1;

                for (int y = 0; y < th; ++y) {
                    uint8_t byte_val = 0;
                    int bit_shift = -1; // 바이트를 새로 읽어야 함을 표시

                    for (int x = 0; x < tw; ++x) {
                        if (bit_shift < 0) {
                            byte_val = decompressed_buf[ptr++];
                            bit_shift = 8 - bits_per_pixel; // 상위 비트부터 채워짐
                        }

                        uint8_t idx = (byte_val >> bit_shift) & mask;
                        bit_shift -= bits_per_pixel;

                        if (idx < palette_size) {
                            write_pixel_to_screen(screen_buffer, screen_width, tx + x, ty + y, palette[idx]);
                        }
                    }
                    // 행(Row)이 끝날 때 바이트 경계 패딩 처리 (비트가 남았어도 다음 바이트로 강제 이동)
                    bit_shift = -1;
                }
            }
            // --- Case 4: 순수 RLE (Type = 128) ---
            else if (subencoding == 128) {
                int pixels_done = 0;
                while (pixels_done < total_pixels) {
                    Pixel32 color = read_pixel32(decompressed_buf, &ptr);
                    size_t run_len = read_run_length(decompressed_buf, &ptr);

                    for (size_t i = 0; i < run_len && pixels_done < total_pixels; ++i) {
                        int lx = pixels_done % tw;
                        int ly = pixels_done / tw;
                        write_pixel_to_screen(screen_buffer, screen_width, tx + lx, ty + ly, color);
                        pixels_done++;
                    }
                }
            }
            // --- Case 5: 팔레트 RLE (Type = 130 ~ 255) ---
            else if (subencoding >= 130) {
                int palette_size = subencoding - 128; // 최대 127개 팔레트
                for (int i = 0; i < palette_size; ++i) {
                    palette[i] = read_pixel32(decompressed_buf, &ptr);
                }

                int pixels_done = 0;
                while (pixels_done < total_pixels) {
                    uint8_t b = decompressed_buf[ptr++];
                    size_t run_len = read_run_length(decompressed_buf, &ptr);
                    for (size_t i = 0; i < run_len && pixels_done < total_pixels; ++i) {
                        int lx = pixels_done % tw;
                        int ly = pixels_done / tw;
                        //if (idx < palette_size) {
                            write_pixel_to_screen(screen_buffer, screen_width, tx + lx, ty + ly, palette[b]);
                        //}
                        pixels_done++;
                    }
                }
            }

            // 안전장치: 스트림을 초과해서 파싱하는 것을 방지
            if (ptr >= buf_size && (tx + 64 < rect_x + rect_width || ty + 64 < rect_y + rect_height)) {
                fprintf(stderr, "ZRLE 스트림 데이터가 비정상적으로 부족합니다.\n");
                return;
            }
        }
    }
}
