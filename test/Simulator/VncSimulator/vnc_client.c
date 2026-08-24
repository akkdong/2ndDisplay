// vnc_client.c
//

#include "vnc_client.h"
#include "vnc_screen.h"
#include "app_main.h"
#include "d3des.h"
#include "tight_decoder.h"
#include <jpeglib.h>

static const char* TAG = "CLIENT";

/*
// USE Winsock Wrapper
#define USE_WINSOCK_WRAPPER 1
#if USE_WINSOCK_WRAPPER

#include "WinsockWorker.h"

#undef SOCKET
#undef socket
#undef connect
#undef closesocket
#undef send
#undef recv

#define SOCKET          WinsockWrapperSocket_t *
#define socket          Winsock_socket
#define connect         Winsock_connect
#define closesocket     Winsock_closesocket
#define send            Winsock_send
#define recv            Winsock_recv

#endif
*/


//
// local static functions
//

static void vnc_client_init(vnc_client_t* client);
static void vnc_client_deinit(vnc_client_t* client);

static void vnc_client_init_zstreams(vnc_client_t* client);
static void vnc_client_reset_zstream(vnc_client_t* client, int id);
static void vnc_client_destroy_zstreams(vnc_client_t* client);

static void vnc_client_compact_buf(vnc_client_t* client);
static bool vnc_client_fill_buf(vnc_client_t* client);
static bool vnc_client_wait_buf(vnc_client_t* client, size_t needed);

static int vnc_client_read_u8(vnc_client_t* client, uint8_t* v);
static int vnc_client_read_bytes(vnc_client_t* client, uint8_t* dst, size_t n);
static int vnc_client_read_clen(vnc_client_t* client, size_t* len);
static int vnc_client_rect_ok(vnc_client_t* client, int rx, int ry, int rw, int rh);
static int vnc_client_raw_decode(vnc_client_t* client, int rx, int ry, int rw, int rh, int bpp);
static int vnc_client_zrle_decode(vnc_client_t* client, int rx, int ry, int rw, int rh, int bpp);
static int vnc_client_tight_decode(vnc_client_t* client, int rx, int ry, int rw, int rh, int bpp);




//
// vnc client instance
//

static vnc_client_t vnc_client;



//
//
//

static bool read_exact(SOCKET fd, uint8_t* buf, size_t len)
{
    while (len > 0) 
    {
        int n = recv(fd, buf, len, 0);
        if (n > 0) 
        {
            buf += n; len -= n;
            continue;
        }

        if (n == 0) // disconnected
        {
            ESP_LOGI(TAG, "Socket Disconnected");
            return false;
        }
        n = 0 - n; // error
        if (n == pdFREERTOS_ERRNO_EINTR)
            continue;
        if (n == pdFREERTOS_ERRNO_EWOULDBLOCK)
            return false;

        return false;
    }

    return true;
}

static bool write_exact(SOCKET fd, const uint8_t* buf, size_t len)
{
    while (len > 0) 
    {
        int n = send(fd, buf, len, 0);
        if (n <= 0) 
        {
            if (n == EINTR) 
                continue;

            return false;
        }
        buf += n; len -= n;
    }
    return true;
}

// ============================================================
// VNC bit-reverse table for VNC Auth
// ============================================================
static const unsigned char bit_rev[256] =
{
    0x00,0x80,0x40,0xC0,0x20,0xA0,0x60,0xE0,0x10,0x90,0x50,0xD0,0x30,0xB0,0x70,0xF0,
    0x08,0x88,0x48,0xC8,0x28,0xA8,0x68,0xE8,0x18,0x98,0x58,0xD8,0x38,0xB8,0x78,0xF8,
    0x04,0x84,0x44,0xC4,0x24,0xA4,0x64,0xE4,0x14,0x94,0x54,0xD4,0x34,0xB4,0x74,0xF4,
    0x0C,0x8C,0x4C,0xCC,0x2C,0xAC,0x6C,0xEC,0x1C,0x9C,0x5C,0xDC,0x3C,0xBC,0x7C,0xFC,
    0x02,0x82,0x42,0xC2,0x22,0xA2,0x62,0xE2,0x12,0x92,0x52,0xD2,0x32,0xB2,0x72,0xF2,
    0x0A,0x8A,0x4A,0xCA,0x2A,0xAA,0x6A,0xEA,0x1A,0x9A,0x5A,0xDA,0x3A,0xBA,0x7A,0xFA,
    0x06,0x86,0x46,0xC6,0x26,0xA6,0x66,0xE6,0x16,0x96,0x56,0xD6,0x36,0xB6,0x76,0xF6,
    0x0E,0x8E,0x4E,0xCE,0x2E,0xAE,0x6E,0xEE,0x1E,0x9E,0x5E,0xDE,0x3E,0xBE,0x7E,0xFE,
    0x01,0x81,0x41,0xC1,0x21,0xA1,0x61,0xE1,0x11,0x91,0x51,0xD1,0x31,0xB1,0x71,0xF1,
    0x09,0x89,0x49,0xC9,0x29,0xA9,0x69,0xE9,0x19,0x99,0x59,0xD9,0x39,0xB9,0x79,0xF9,
    0x05,0x85,0x45,0xC5,0x25,0xA5,0x65,0xE5,0x15,0x95,0x55,0xD5,0x35,0xB5,0x75,0xF5,
    0x0D,0x8D,0x4D,0xCD,0x2D,0xAD,0x6D,0xED,0x1D,0x9D,0x5D,0xDD,0x3D,0xBD,0x7D,0xFD,
    0x03,0x83,0x43,0xC3,0x23,0xA3,0x63,0xE3,0x13,0x93,0x53,0xD3,0x33,0xB3,0x73,0xF3,
    0x0B,0x8B,0x4B,0xCB,0x2B,0xAB,0x6B,0xEB,0x1B,0x9B,0x5B,0xDB,0x3B,0xBB,0x7B,0xFB,
    0x07,0x87,0x47,0xC7,0x27,0xA7,0x67,0xE7,0x17,0x97,0x57,0xD7,0x37,0xB7,0x77,0xF7,
    0x0F,0x8F,0x4F,0xCF,0x2F,0xAF,0x6F,0xEF,0x1F,0x9F,0x5F,0xDF,0x3F,0xBF,0x7F,0xFF
};

static void vnc_encrypt_challenge(const uint8_t* challenge, const char* password, uint8_t* response)
{
    unsigned char key[8] = { 0 };
    size_t len = strlen(password);

    if (len > 8) 
        len = 8;
    for (size_t i = 0; i < len; ++i)
        key[i] = bit_rev[(unsigned char)password[i]];

    rfbDesKey(key, EN0);
    rfbDes((unsigned char*)challenge, response);
    rfbDes((unsigned char*)(challenge + 8), response + 8);
}






//
// vnc client main routine
//

static vnc_client_task(void* param)
{
    ESP_LOGI(TAG, "[vnc_client] enter task.");

    vnc_client_t* client = (vnc_client_t*)param;
    vnc_app_t* app = client->app;

    if (vnc_client_connect(client, app->server_addr, app->server_port, 5000))
    {
        vnc_app_send_event(app, VNC_SERVER_CONNECTED, 0, 0, 0);

        if (vnc_client_handshake(client, app->server_pass))
        {
            vnc_app_send_event(app, VNC_HANDSHAKE_FINISHED, client->fbw, client->fbh, sizeof(uint32_t));
            vnc_client_run(client);
            vnc_app_send_event(app, VNC_SERVER_DISCONNECTED, 0, 0, 0);
        }
        else
        {
            vnc_app_send_event(app, VNC_HANDSHAKE_FINISHED, -1, -1, -1);
        }

        vnc_client_close(client);
    }
    else
    {
        vnc_app_send_event(app, VNC_SERVER_DISCONNECTED, -1, -1, -1);
    }


    /*
    uint32_t count = 0;
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(100));

        if (++count > 10)
            break;
    }
    */

    ESP_LOGI(TAG, "[vnc_client] leave task.");
    vnc_client_deinit(client);
    vTaskDelete(NULL);
}




//
// start vnc client
//

vnc_client_t* vnc_client_start(vnc_app_t* app)
{
    //
    vnc_client_init(&vnc_client);
	vnc_client.app = app;
    vnc_client.scrn = app->scrn;

    //
    BaseType_t result = xTaskCreate(vnc_client_task, "vnc_client", 8 * 1024, &vnc_client, tskIDLE_PRIORITY + 4, NULL);
    if (result == pdTRUE)
        return &vnc_client;

    return NULL;
}



//
//
//

bool vnc_client_connect(vnc_client_t* client, const char* host, uint16_t port, int timeout)
{
    client->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client->fd == INVALID_SOCKET)
        return false;

#if USE_VIRTUALSOCKET
    struct VirtualSocket_sockaddr addr;
    addr.addr = inet_addr(host);
    addr.port = htons(port);
#else
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
#if defined(_SIMULATOR)
    addr.sin_address.ulIP_IPv4 = inet_addr(host);
#else
    addr.sin_addr.s_addr = inet_addr(host);
#endif
#endif

    if (connect(client->fd, &addr, sizeof(addr)) < 0 && errno != EINPROGRESS && errno != EINTR) 
    {
        closesocket(client->fd);
        client->fd = INVALID_SOCKET;
        return false;
    }

    return true;
}

bool vnc_client_handshake(vnc_client_t* client, const char* pass)
{
    // 1. Protocol version
    uint8_t ver[14];
    memset(ver, 0, sizeof(ver));
    if (!read_exact(client->fd, ver, 12)) 
        return false;
    vnc_log_printf(client->scrn, "[Client] Server: %s", (char*)ver);

    const char* my_ver = "RFB 003.008\n";
    if (!write_exact(client->fd, (const uint8_t*)my_ver, 12)) 
        return false;

    // 2. Security
    uint8_t nsec;
    if (!read_exact(client->fd, &nsec, 1) || nsec == 0)
        return false;

    uint8_t* types = malloc(nsec);
    if (!types)
        return false;
    if (!read_exact(client->fd, types, nsec))
        return false;

    /*
    std::cout << "Security types:";
    for (auto t : types) std::cout << " " << int(t);
    std::cout << std::endl;
    */

    uint8_t chosen = 0;
    bool have_none = false, have_vncauth = false;
    for (int i = 0; i < nsec; ++i) 
    {
        if (types[i] == 1) 
            have_none = true;
        if (types[i] == 2) 
            have_vncauth = true;
    }
    free(types);

    if (have_vncauth && (pass && pass[0]))
    {
        chosen = 2;
    }
    else if (have_none) 
    {
        chosen = 1;
    }
    else 
    {
        vnc_log_append(client->scrn, "[Client] No supported security type\n");
        return false;
    }

    if (!write_exact(client->fd, &chosen, 1)) 
        return false;

    if (chosen == 2) 
    {
        uint8_t challenge[16];
        if (!read_exact(client->fd, challenge, 16))
            return false;

        uint8_t response[16];
        vnc_encrypt_challenge(challenge, pass, response);
        if (!write_exact(client->fd, response, 16)) 
            return false;
    }

    // Security result (4 bytes for RFB 3.8+)
    if (chosen == 2 || chosen == 1) 
    {
        uint32_t result;
        if (!read_exact(client->fd, (uint8_t*)&result, 4)) 
            return false;

        if (ntohl(result) != 0) 
        {
            vnc_log_printf(client->scrn, "[Client] Security failed, result=%d\n", ntohl(result));
            return false;
        }

        vnc_log_append(client->scrn, "[Client] Security OK\n");
    }

    // 3. ClientInit
    uint8_t shared = 1;
    if (!write_exact(client->fd, &shared, 1)) 
        return false;

    // 4. ServerInit
    ServerInit si;
    if (!read_exact(client->fd, (uint8_t*)&si, sizeof(si)))
        return false;

    client->fbw = ntohs(si.fb_width);
    client->fbh = ntohs(si.fb_height);
    client->fmt = si.fmt;
    client->fmt.red_max = ntohs(client->fmt.red_max);
    client->fmt.green_max = ntohs(client->fmt.green_max);
    client->fmt.blue_max = ntohs(client->fmt.blue_max);
    vnc_log_printf(client->scrn, "[Client] Frame Resolution: %d x %d\n", client->fbw, client->fbh);
    vnc_log_printf(client->scrn, "[Client] Frame BPP=%d, DEPTH=%d\n", client->fmt.bpp, client->fmt.depth);
    vnc_log_printf(client->scrn, "[Client] Frame bing_endian=%d\n", client->fmt.big_endian);
    /*
    std::cout << "Framebuffer: " << fbw << "x" << fbh;
    std::cout << " fmt: bpp=" << int(fmt.bpp) << " depth=" << int(fmt.depth);
    std::cout << " big_endian=" << int(fmt.big_endian) << " true_color=" << int(fmt.true_color);
    std::cout << " shift=" << int(fmt.red_shift) << "," << int(fmt.green_shift) << "," << int(fmt.blue_shift);
    std::cout << " max=" << fmt.red_max << "," << fmt.green_max << "," << fmt.blue_max;
    std::cout << std::endl;
    */

    uint32_t nlen = ntohl(si.name_len);
    if (nlen > 0)
    {
        if (nlen > (1 << 20)) 
        {
            vnc_log_printf(client->scrn, "[Client] Desktop name too long: %u\n", nlen);
            return false;
        }

        char* name = malloc(nlen + 1);
        if (!name)
            return false;
        if (!read_exact(client->fd, name, nlen))
        {
            free(name);
            return false;
        }

        vnc_log_printf(client->scrn, "[Client] Desktop: %s\n", name);
        free(name);
    }

#if SUPPORT_SETPIXELFORMAT
    // 5. SetPixelFormat
    uint8_t msg[4] = { 0, 0, 0, 0 };
    if (!write_exact(client->fd, &msg[0], sizeof(msg)))
    {
        vnc_log_printf(client->scrn, "[Client] SetPixelFormat phase1 failed.\n");
        return false;
    }

    client->fmt.bpp = 16;
    client->fmt.depth = 16;
    client->fmt.big_endian = 0;
    client->fmt.true_color = 1;
    client->fmt.red_max = 31; // 2^5 - 1
    client->fmt.green_max = 63; // 2^6 - 1
    client->fmt.blue_max = 31; // 2^5 - 1
    client->fmt.red_shift = 11;
    client->fmt.green_shift = 5;
    client->fmt.blue_shift = 0;

    if (!write_exact(client->fd, &client->fmt, sizeof(client->fmt)))
    {
        vnc_log_printf(client->scrn, "[Client] SetPixelFormat phase2 failed.\n");
        return false;
    }
#endif

    const size_t fb_pixels = client->fbw * client->fbh; // *(client->fmt.bpp / 8); // sizeof(uint16_t);
    if (fb_pixels > 0x10000000) 
    {
        vnc_log_printf(client->scrn, "[Client] Framebuffer too large: %d x %d\n", client->fbw, client->fbh);
        return false;
    }

    client->bpp = client->fmt.bpp / 8;

    if (client->bpp < 1) 
        client->bpp = 1;
    if (client->bpp > 4) 
        client->bpp = 4;

    client->fb = heap_caps_malloc(fb_pixels * sizeof(uint32_t), MALLOC_CAP_SPIRAM);
    if (!client->fb)
        return false;
    client->fb_pixels = fb_pixels;

    client->rbuf_ptr = heap_caps_malloc(fb_pixels * client->bpp, MALLOC_CAP_SPIRAM);
    if (!client->rbuf_ptr)
        return false;
    client->rbuf_len = (uint32_t)(fb_pixels * client->bpp);

    client->temp_ptr = heap_caps_malloc(fb_pixels * client->bpp, MALLOC_CAP_SPIRAM);
    if (!client->temp_ptr)
        return false;
    client->temp_len = (uint32_t)(fb_pixels * client->bpp);


    // Resize the LVGL display/window to the VNC framebuffer size and
    // (re)build the overlay UI on top of the VNC canvas
    //if (disp_) lv_display_set_resolution((lv_display_t *)(*disp_), fbw, fbh);
    //Display::create_vnc_ui(fb_.data(), fbw, fbh);

    vnc_client_init_zstreams(client);

    return true;
}

void vnc_client_loop(vnc_client_t* self)
{
    // NOTE: LVGL is driven solely by the main thread (display_loop()).
    // This worker must not call lv_timer_handler().

    // Send pending FB request
    if (self->need_update) 
    {
        uint8_t fb_req_inc[] = { 3, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
        fb_req_inc[6] = (self->fbw >> 8) & 0xFF;
        fb_req_inc[7] = self->fbw & 0xFF;
        fb_req_inc[8] = (self->fbh >> 8) & 0xFF;
        fb_req_inc[9] = self->fbh & 0xFF;
        //ESP_LOGI(TAG, "FramebufferUpdateRequest: 1");
        if (!write_exact(self->fd, fb_req_inc, 10))
        {
            //ESP_LOGI(TAG, "  --> failed");
            self->break_loop = 1;
        }
        self->need_update = 0;
    }

    // Read message type (blocking read)
    uint8_t msg_type;
    if (!vnc_client_read_u8(self, &msg_type)) 
    {
        if (!self->break_loop) 
            ESP_LOGE(TAG, "Server disconnected");
        self->break_loop = 1;
        return;
    }

    if (msg_type == 0) // FramebufferUpdate
    {
        uint8_t pad;
        uint16_t nrects;
        int msg_ok = 1;
        if (!vnc_client_read_u8(self, &pad)) 
        { 
            msg_ok = 0; 
        }
        if (msg_ok) 
        {
            uint8_t nrbuf[2];
            if (!vnc_client_read_bytes(self, nrbuf, 2)) 
            { 
                msg_ok = 0; 
            }
            if (msg_ok) 
            {
                nrects = (uint16_t)(((uint16_t)nrbuf[0] << 8) | nrbuf[1]);
                //ESP_LOGI(TAG, "nrets = %d", nrects);
                for (int i = 0; i < nrects && msg_ok; ++i) 
                {
                    uint8_t rect_hdr[12];
                    if (!vnc_client_read_bytes(self, rect_hdr, 12)) 
                    { 
                        msg_ok = 0; 
                        break; 
                    }

                    uint16_t rx = (uint16_t)(((uint16_t)rect_hdr[0] << 8) | rect_hdr[1]);
                    uint16_t ry = (uint16_t)(((uint16_t)rect_hdr[2] << 8) | rect_hdr[3]);
                    uint16_t rw = (uint16_t)(((uint16_t)rect_hdr[4] << 8) | rect_hdr[5]);
                    uint16_t rh = (uint16_t)(((uint16_t)rect_hdr[6] << 8) | rect_hdr[7]);
                    int32_t encoding = ((int32_t)rect_hdr[8] << 24) | ((int32_t)rect_hdr[9] << 16) |
                        ((int32_t)rect_hdr[10] << 8) | rect_hdr[11];
                    //ESP_LOGI(TAG, "rect #%d: (%d, %d, %d, %d), %d", i, rx, ry, rw, rh, encoding);
                    if (!vnc_client_rect_ok(self, rx, ry, rw, rh)) 
                    {
                        ESP_LOGE(TAG, "Rect out of bounds: %u,%u %ux%u (fb %u x %u)",
                            rx, ry, rw, rh, self->fbw, self->fbh);
                        msg_ok = 0;
                        break;
                    }

                    if (encoding == 0) 
                    {
                        if (!vnc_client_raw_decode(self, rx, ry, rw, rh, self->bpp))
                        {
                            msg_ok = 0;
                            break;
                        }
                    }
                    else if (encoding == 1) 
                    {
                        uint8_t copy_hdr[4];
                        if (!vnc_client_read_bytes(self, copy_hdr, 4)) 
                        { 
                            msg_ok = 0; 
                            break; 
                        }
                        int src_x = ((int)copy_hdr[0] << 8) | copy_hdr[1];
                        int src_y = ((int)copy_hdr[2] << 8) | copy_hdr[3];
                        if (!vnc_client_rect_ok(self, src_x, src_y, rw, rh)) 
                        {
                            ESP_LOGE(TAG, "CopyRect source out of bounds");
                            msg_ok = 0;
                            break;
                        }
                        for (int y = 0; y < rh; ++y)
                            for (int x = 0; x < rw; ++x)
                                self->fb[(ry + y) * self->fbw + (rx + x)] =
                                self->fb[(src_y + y) * self->fbw + (src_x + x)];
                    }
                    else if (encoding == 7) 
                    {
                        if (!vnc_client_tight_decode(self, rx, ry, rw, rh, self->bpp)) 
                        {
                            msg_ok = 0;
                            break;
                        }
                    }
                    else if (encoding == 16)
                    {
                        if (!vnc_client_zrle_decode(self, rx, ry, rw, rh, self->bpp))
                        {
                            msg_ok = 0;
                            break;
                        }
                    }
                    else
                    {
                        ESP_LOGE(TAG, "Unsupported encoding: %d", (int)encoding);
                        msg_ok = 0;
                        break;
                    }
                }
            }
        }
        if (!msg_ok) 
        {
            self->break_loop = 1;
            return;
        }

        // Publish the decoded frame to the Display-owned canvas buffer and
        // signal the main thread (display_loop drains the event queue)
        if (self->scrn) 
        {
            vnc_screen_publish_frame(self->scrn, (uint8_t *)self->fb, (uint32_t)(self->fb_pixels * sizeof(uint32_t)));
            //vnc_screen_push_event(self->scrn, EVT_UPDATE_FRAMEBUFFER);
        }
        //ESP_LOGI(TAG, "update display");
        self->need_update = 1;

    }
    else if (msg_type == 1) // SetColourMapEntries
    {
        uint8_t pad;
        uint8_t fcbuf[2], ncbuf[2];
        if (!vnc_client_read_u8(self, &pad)) { self->break_loop = 1; return; }
        if (!vnc_client_read_bytes(self, fcbuf, 2)) { self->break_loop = 1; return; }
        if (!vnc_client_read_bytes(self, ncbuf, 2)) { self->break_loop = 1; return; }
        uint16_t ncolors = (uint16_t)(((uint16_t)ncbuf[0] << 8) | ncbuf[1]);
        if ((size_t)ncolors * 6 > (1 << 20)) {
            ESP_LOGE(TAG, "Oversized colour map");
            self->break_loop = 1;
            return;
        }
        uint8_t* cmap = (uint8_t*)malloc((size_t)ncolors * 6);
        if (cmap) {
            if (!vnc_client_read_bytes(self, cmap, (size_t)ncolors * 6)) { free(cmap); self->break_loop = 1; return; }
            free(cmap);
        }

    }
    else if (msg_type == 2) 
    {
        // Bell
    }
    else if (msg_type == 3) // ServerCutText
    {
        uint8_t pad[3];
        if (!vnc_client_read_bytes(self, pad, 3)) { self->break_loop = 1; return; }
        uint8_t clbuf[4];
        if (!vnc_client_read_bytes(self, clbuf, 4)) { self->break_loop = 1; return; }
        uint32_t clen = ((uint32_t)clbuf[0] << 24) | ((uint32_t)clbuf[1] << 16) |
            ((uint32_t)clbuf[2] << 8) | clbuf[3];
        if (clen > (1 << 20)) {
            ESP_LOGE(TAG, "Oversized server cut-text: %u bytes", clen);
            self->break_loop = 1;
            return;
        }
        uint8_t* ctext = (uint8_t*)malloc(clen ? clen : 1);
        if (ctext) {
            if (!vnc_client_read_bytes(self, ctext, clen)) { free(ctext); self->break_loop = 1; return; }
            free(ctext);
        }
    }
    else {
        ESP_LOGE(TAG, "Unknown message type: %d", (int)msg_type);
        self->break_loop = 1;
        return;
    }
}

#define USE_TIGHT_ENCODING  1
#define USE_ZRLE_ENCODING   2
#define VNC_ENCODING        USE_TIGHT_ENCODING

void vnc_client_run(vnc_client_t* client)
{
    // Send SetEncodings
    //  0   Raw Encoding
    //  1   CopyRect Encoding
    //  2   RRE Encoding
    //  4   CoRRE Encoding
    //  5   Hextile Encoding
    //  6   zlib Encoding
    //  7   Tight Encoding
    //  8   zlibhex Encoding
    //  16  ZRLE Encoding
    //  21  JEPG Encoding
    uint8_t setenc[] =
    {
        2,          // message type
        0,          // padding
        0, 2,       // number of encodings
#if VNC_ENCODING == USE_TIGHT_ENCODING
        0, 0, 0, 7, // Tight
#elif VNC_ENCODING == USE_ZRLE_ENCODING
        0, 0, 0, 16, // ZRLE
#else // RAW_ENCODING
        0, 0, 0, 0, // Raw (desktop fallback)
#endif
#if ENABLE_JPEG_COMPRESSION || 0
        0xFF, 0xFF, 0xFF, 0xE6, // Tight + JPEG (JPEG Quality Level Pseudo-encoding)
#else
        0, 0, 0, 1, // CopyRect
#endif
    };

    if (!write_exact(client->fd, setenc, sizeof(setenc))) 
    {
        vnc_log_append(client->scrn, "[Client] Failed to send SetEncodings\n");
        client->break_loop = true;
        return;
    }

    // Send initial full FB update request
    client->need_update = true;
#if 1
    uint8_t fb_req_full[] = 
    { 
        3, 0, 0, 0, 0, 0, 0, 0, 0, 0 
    };

    fb_req_full[6] = (client->fbw >> 8) & 0xFF;
    fb_req_full[7] = client->fbw & 0xFF;
    fb_req_full[8] = (client->fbh >> 8) & 0xFF;
    fb_req_full[9] = client->fbh & 0xFF;

    //ESP_LOGI(TAG, "FramebufferUpdateRequest: 0");
    if (!write_exact(client->fd, fb_req_full, 10)) 
    {
        vnc_log_append(client->scrn, "[Client] Failed FB request\n");
        client->break_loop = true;
        return;
    }

    client->need_update = false;
#endif

    while (vnc_client_isOk(client))
        vnc_client_loop(client);
}

void vnc_client_close(vnc_client_t* client)
{
    if (client->fd != INVALID_SOCKET)
    {
        closesocket(client->fd);
        client->fd = INVALID_SOCKET;
    }
}


bool vnc_client_isOk(vnc_client_t* client)
{
    if (client->fd == INVALID_SOCKET)
        return false;
    if (client->break_loop)
        return false;
    /*
    if (client->disp_ && !client->disp_->m_clientRunning)
        return false;
    */

    return true;
}

/*
void vnc_client_send_pointer(vnc_client_t* client, int x, int y, uint8_t mask)
{
}
*/




//
//
//

static void vnc_client_init(vnc_client_t* client)
{
    //
    memset(client, 0, sizeof(vnc_client_t));

    client->fd = INVALID_SOCKET;
    //
    // ...
    //
}

static void vnc_client_deinit(vnc_client_t* client)
{
    vnc_client_destroy_zstreams(client);

    if (client->fb)
    {
        vnc_log_append(client->scrn, "[Client] free Frame Buffer\n");
        free(client->fb);
    }

    if (client->rbuf_ptr)
    {
        vnc_log_append(client->scrn, "[Client] free Receive Buffer\n");
        free(client->rbuf_ptr);
    }

    if (client->temp_ptr)
    {
        vnc_log_append(client->scrn, "[Client] free Temporary Buffer\n");
        free(client->temp_ptr);
    }

    if (client->fd != INVALID_SOCKET)
    {
        vnc_log_append(client->scrn, "[Client] Close Socket\n");
        closesocket(client->fd);
    }
}



static void vnc_client_init_zstreams(vnc_client_t* client)
{
    for (int i = 0; i < sizeof(client->zstream) / sizeof(client->zstream[0]); ++i)
    {
        memset(&client->zstream[i], 0, sizeof(z_stream));
        client->zstream[i].zalloc = Z_NULL;
        client->zstream[i].zfree = Z_NULL;
        client->zstream[i].opaque = Z_NULL;

        inflateInit(&client->zstream[i]);
    }
}

static void vnc_client_reset_zstream(vnc_client_t* client, int id)
{
    inflateReset(&client->zstream[id]);
}

static void vnc_client_destroy_zstreams(vnc_client_t* client)
{
    for (int i = 0; i < sizeof(client->zstream) / sizeof(client->zstream[0]); ++i)
        inflateEnd(&client->zstream[i]);
}


// Convenience: read one byte (blocking)
static int vnc_client_read_u8(vnc_client_t* self, uint8_t* v)
{
    return read_exact(self->fd, v, 1);
}

// Convenience: read into buffer (blocking)
static int vnc_client_read_bytes(vnc_client_t* self, uint8_t* dst, size_t n)
{
    return read_exact(self->fd, dst, n);
}

// Read compact length from buffer
static int vnc_client_read_clen(vnc_client_t* self, size_t* len)
{
    uint8_t b;
    if (!vnc_client_read_u8(self, &b)) return 0;
    *len = b & 0x7F;
    if (b & 0x80) {
        if (!vnc_client_read_u8(self, &b)) return 0;
        *len |= (size_t)(b & 0x7F) << 7;
        if (b & 0x80) {
            if (!vnc_client_read_u8(self, &b)) return 0;
            *len |= (size_t)(b & 0x7F) << 14;
        }
    }
    return 1;
}

// Validate that a rectangle fits within the framebuffer
static int vnc_client_rect_ok(vnc_client_t* self, int rx, int ry, int rw, int rh)
{
    return rx >= 0 && ry >= 0 && rw > 0 && rh > 0 &&
        (size_t)rx + (size_t)rw <= (size_t)self->fbw &&
        (size_t)ry + (size_t)rh <= (size_t)self->fbh;
}

static int vnc_client_raw_decode(vnc_client_t* self, int rx, int ry, int rw, int rh, int bpp)
{
    size_t pix_bytes = (size_t)rw * rh * (size_t)self->bpp;
    uint8_t* pixels = self->rbuf_ptr;
    if (!vnc_client_read_bytes(self, pixels, pix_bytes))
        return 0;
    //ESP_LOGI(TAG, "receive done");

    const uint8_t* src = pixels;
    for (int y = 0; y < rh; ++y)
    {
        //int offset = y * self->fbw * bpp + rx * bpp;
        int offset = (ry + y) * self->fbw + rx;
        for (int x = 0; x < rw; ++x)
        {
            self->fb[offset + x] = pixel_to_32bit(src, &self->fmt);
            src += self->bpp;
        }
    }

    return 1;
}



int parse_zrle_buffer(const uint8_t* decompressed_buf, size_t buf_size,
    int rect_x, int rect_y, int rect_width, int rect_height,
    uint32_t* screen_buffer, int screen_width, const PixelFormat* fmt
);

static int vnc_client_zrle_decode(vnc_client_t* self, int rx, int ry, int rw, int rh, int bpp)
{
    (void)bpp;

    uint32_t zlen;
    if (!vnc_client_read_bytes(self, (uint8_t*)&zlen, 4))
        return 0;
    zlen = ntohl(zlen);

    if ((size_t)zlen > (size_t)self->rbuf_len)
    {
        ESP_LOGE(TAG, "ZRLE rect data too large: %u", zlen);
        return 0;
    }

    //ESP_LOGI(TAG, "ZRLE receiving %u bytes", zlen);
    if (!vnc_client_read_bytes(self, self->rbuf_ptr, zlen))
        return 0;

    size_t uncomp_size = 0;
    if (!zlib_decompress2(&self->zstream[0], self->rbuf_ptr, zlen, self->temp_ptr, self->temp_len, &uncomp_size))
    {
        ESP_LOGE(TAG, "ZRLE inflate failed");
        return 0;
    }

    if (!parse_zrle_buffer(self->temp_ptr, uncomp_size, rx, ry, rw, rh, self->fb, self->fbw, &self->fmt))
    {
        ESP_LOGE(TAG, "ZRLE tile decode failed");
        return 0;
    }

    //ESP_LOGI(TAG, "decode title done.");

    return 1;
}

/* Tight depth-24 wire pixels are R,G,B component bytes (not a native LE pixel). */
static uint32_t tight_pixel_to_32bit(const uint8_t* p, const PixelFormat* fmt, size_t tpb)
{
    if (tpb == 3)
        return 0xFF000000u | ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
    return pixel_to_32bit_b(p, fmt, (int)tpb);
}

static void tight_render_pixels(const uint8_t* pix, const PixelFormat* fmt, size_t tpb,
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

static void tight_render_palette(const uint8_t* idx_data, int bits,
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

static int vnc_client_tight_jpeg(vnc_client_t* self, int rx, int ry, int rw, int rh)
{
    uint32_t* fb = self->fb;
    int fbw = self->fbw;

    size_t jpeg_len;
    if (!vnc_client_read_clen(self, &jpeg_len)) return 0;
    uint8_t* jpeg_data = (uint8_t*)malloc(jpeg_len ? jpeg_len : 1);
    if (!jpeg_data) return 0;
    if (!vnc_client_read_bytes(self, jpeg_data, jpeg_len)) { free(jpeg_data); return 0; }

    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    memset(&cinfo, 0, sizeof(cinfo));
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, jpeg_data, jpeg_len);
    if (jpeg_read_header(&cinfo, TRUE) == JPEG_HEADER_OK) {
        cinfo.out_color_space = JCS_EXT_BGRA;
        jpeg_start_decompress(&cinfo);
        if (cinfo.output_width > (JDIMENSION)rw || cinfo.output_height > (JDIMENSION)rh) {
            ESP_LOGE(TAG, "Tight JPEG size mismatch");
            jpeg_abort_decompress(&cinfo);
            jpeg_destroy_decompress(&cinfo);
            free(jpeg_data);
            return 0;
        }
        for (int y = 0; y < rh && y < (int)cinfo.output_height; ++y) {
            uint8_t* row = (uint8_t*)&fb[(ry + y) * fbw + rx];
            jpeg_read_scanlines(&cinfo, &row, 1);
            for (int x = cinfo.output_width; x < rw; ++x)
                fb[(ry + y) * fbw + rx + x] = 0xFF000000;
        }
        jpeg_finish_decompress(&cinfo);
    }
    jpeg_destroy_decompress(&cinfo);
    free(jpeg_data);
    return 1;
}

/*
 * Tight encoding, TightVNC wire layout:
 *   ctrl bits 0-3: reset zlib streams 0..3
 *   ctrl >> 4:     0x08 fill, 0x09 jpeg,
 *                  0x0A basic uncompressed, 0x0E basic uncompressed + filter id,
 *                  0x00-0x07 basic compressed (bits 5-4 = stream index,
 *                  bit 6 = explicit filter id follows)
 */
static int vnc_client_tight_decode(vnc_client_t* self, int rx, int ry, int rw, int rh, int bpp)
{
    (void)bpp;

    uint32_t* fb = self->fb;
    int fbw = self->fbw;
    const PixelFormat* fmt = &self->fmt;

    if (!vnc_client_rect_ok(self, rx, ry, rw, rh)) {
        ESP_LOGE(TAG, "Tight rect out of bounds");
        return 0;
    }

    uint8_t ctrl;
    if (!vnc_client_read_u8(self, &ctrl)) return 0;

    for (int i = 0; i < (int)(sizeof(self->zstream) / sizeof(self->zstream[0])); ++i) {
        if (ctrl & (1u << i))
            inflateReset(&self->zstream[i]);
    }

    uint8_t ctype = ctrl >> 4;

    size_t tpb = (fmt->bpp == 32 && fmt->depth == 24 &&
        fmt->red_max == 0xFF && fmt->green_max == 0xFF && fmt->blue_max == 0xFF)
        ? 3 : (size_t)(fmt->bpp / 8);

    if (ctype == 0x08) {
        uint8_t fill_buf[4];
        if (tpb > sizeof(fill_buf)) return 0;
        if (!vnc_client_read_bytes(self, fill_buf, tpb)) return 0;
        uint32_t color = tight_pixel_to_32bit(fill_buf, fmt, tpb);
        for (int y = 0; y < rh; ++y)
            for (int x = 0; x < rw; ++x)
                fb[(ry + y) * fbw + (rx + x)] = color;
        return 1;
    }

    if (ctype == 0x09)
        return vnc_client_tight_jpeg(self, rx, ry, rw, rh);

    int uncompressed = (ctype == 0x0A || ctype == 0x0E);
    if (!uncompressed && ctype > 0x07) {
        ESP_LOGE(TAG, "Invalid Tight compression type %u", (unsigned)ctype);
        return 0;
    }
    int stream_idx = (int)(ctype & 0x03);

    int filter_id = 0;
    if (ctrl & 0x40) {
        uint8_t f;
        if (!vnc_client_read_u8(self, &f)) return 0;
        if (f > 2) {
            ESP_LOGE(TAG, "Unknown Tight filter id %u", (unsigned)f);
            return 0;
        }
        filter_id = f;
    }

    int bits_pixel;
    uint32_t palette[256];
    int pal_size = 0;

    if (filter_id == 1) {
        uint8_t n;
        if (!vnc_client_read_u8(self, &n)) return 0;
        pal_size = (int)n + 1;
        if (pal_size < 2 || tpb > 4) {
            ESP_LOGE(TAG, "Invalid Tight palette size %d", pal_size);
            return 0;
        }
        {
            uint8_t pal_buf[256 * 4];
            if (!vnc_client_read_bytes(self, pal_buf, (size_t)pal_size * tpb)) return 0;
            for (int i = 0; i < pal_size; ++i)
                palette[i] = tight_pixel_to_32bit(pal_buf + i * tpb, fmt, tpb);
        }
        bits_pixel = (pal_size == 2) ? 1 : 8;
    }
    else {
        if (filter_id == 2 && tpb != 3) {
            ESP_LOGE(TAG, "Tight gradient unsupported for %u bytes/pixel", (unsigned)tpb);
            return 0;
        }
        bits_pixel = (tpb == 3) ? 24 : (int)(tpb * 8);
    }

    size_t row_size = ((size_t)rw * bits_pixel + 7) / 8;
    size_t data_len = row_size * (size_t)rh;
    uint8_t* data = (uint8_t*)malloc(data_len ? data_len : 1);
    if (!data) return 0;

    if (!uncompressed && data_len < 12) {
        /* Small rect: filtered pixel data follows verbatim, no compact length. */
        if (!vnc_client_read_bytes(self, data, data_len)) goto fail;
    }
    else {
        size_t zlen;
        if (!vnc_client_read_clen(self, &zlen)) goto fail;
        if (zlen == 0 || zlen > (size_t)self->fb_pixels * 4 + 65536) {
            ESP_LOGE(TAG, "Tight bad data length %u", (unsigned)zlen);
            goto fail;
        }
        uint8_t* buf = (uint8_t*)malloc(zlen);
        if (!buf) goto fail;
        if (!vnc_client_read_bytes(self, buf, zlen)) { free(buf); goto fail; }

        bool ok;
        if (uncompressed) {
            ok = (zlen == data_len);
            if (ok)
                memcpy(data, buf, data_len);
            else
                ESP_LOGE(TAG, "Tight length mismatch: %u != %u", (unsigned)zlen, (unsigned)data_len);
        }
        else {
            ok = zlib_decompress_exact(&self->zstream[stream_idx], buf, zlen, data, data_len);
            if (!ok)
                ESP_LOGE(TAG, "Tight inflate failed (stream %d)", stream_idx);
        }
        free(buf);
        if (!ok) goto fail;
    }

    switch (filter_id) {
    case 1:
        tight_render_palette(data, bits_pixel, rw, rh, palette, fb, fbw, rx, ry);
        break;
    case 2:
        tight_filter_gradient(data, rw, rh, (int)tpb);
        tight_render_pixels(data, fmt, tpb, rw, rh, fb, fbw, rx, ry);
        break;
    default:
        tight_render_pixels(data, fmt, tpb, rw, rh, fb, fbw, rx, ry);
        break;
    }

    free(data);
    return 1;

fail:
    free(data);
    return 0;
}
