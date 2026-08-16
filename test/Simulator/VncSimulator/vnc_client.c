// vnc_client.c
//

#include "vnc_client.h"
#include "vnc_screen.h"
#include "app_main.h"
#include "d3des.h"
#include "tight_decoder.h"
#include <jpeglib.h>


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

        if (n == 0) 
            return false;
        if (n == EINTR) 
            continue;

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
    printf("[vnc_client] enter task.\n");

    vnc_client_t* client = (vnc_client_t*)param;
    vnc_app_t* app = client->app;
    vnc_app_set_state(app, app->state, APP_ACTION_VNC_CONNECT);

    if (vnc_client_connect(client, app->server_addr, app->server_port, 5000))
    {
        vnc_log_printf(app->scrn, "[Client] Connect to server: %s\n", app->server_addr);

        if (vnc_client_handshake(client, app->server_pass))
        {
            vnc_log_append(app->scrn, "[Client] Negotiation established\n");

            vnc_app_set_state(app, APP_STATE_PLAY, APP_ACTION_NONE);
            vnc_log_append(app->scrn, "[Client] Run\n");
            vnc_client_run(client);
            vnc_log_append(app->scrn, "[Client] Exit\n");
        }
        else
        {
            vnc_log_append(app->scrn, "[Client] Failed Negotiation\n");
        }
    }
    else
    {
        vnc_log_append(app->scrn, "[Client] Failed to connect to server\n");
    }

    vnc_app_set_state(app, APP_STATE_READY, APP_ACTION_NONE);

    /*
    uint32_t count = 0;
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(100));

        if (++count > 10)
            break;
    }
    */

    printf("[vnc_client] leave task.\n");
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
    if (client->fd < 0) 
        return false;

    BaseType_t size = 60 * 1024;
    BaseType_t ret = FreeRTOS_setsockopt(client->fd, 0, FREERTOS_SO_RCVBUF, &size, 0);

    sockaddr addr;   
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
#if defined(_SIMULATOR)
    addr.sin_address.ulIP_IPv4 = inet_addr(host);
#else
    addr.sin_addr.s_addr = inet_addr(host);
#endif

    // Non-blocking connect + poll() so an unreachable host fails quickly
    // instead of blocking for the kernel's default TCP timeout (~2 min).
    /*
    if (set_nonblock(client->fd, true) < 0)
    {
        close(client->fd);
        client->fd = -1;
        return false;
    }
    */

    if (connect(client->fd, &addr, sizeof(addr)) < 0 && errno != EINPROGRESS && errno != EINTR) 
    {
        closesocket(client->fd);
        client->fd = INVALID_SOCKET;
        return false;
    }

    /*
    struct pollfd pfd = { client->fd, POLLOUT, 0 };
    int ret = poll(&pfd, 1, timeout);
    if (ret <= 0)    // 0 = timeout, <0 = poll error
    {
        close(client->fd);
        client->fd = -1;
        return false;
    }

    int err = 0;
    socklen_t errlen = sizeof(err);
    if (getsockopt(client->fd, SOL_SOCKET, SO_ERROR, &err, &errlen) < 0 || err != 0) {
        close(client->fd);
        client->fd = -1;
        return false;
    }
    */

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

    uint8_t* types = _malloc(nsec);
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
    _free(types);

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

        char* name = _malloc(nlen + 1);
        if (!name)
            return false;
        if (!read_exact(client->fd, name, nlen))
        {
            _free(name);
            return false;
        }

        vnc_log_printf(client->scrn, "[Client] Desktop: %s\n", name);
        _free(name);
    }

    const size_t fb_pixels = client->fbw * client->fbh; // *(client->fmt.bpp / 8); // sizeof(uint16_t);
    if (fb_pixels > 0x10000000) 
    {
        vnc_log_printf(client->scrn, "[Client] Framebuffer too large: %d x %d\n", client->fbw, client->fbh);
        return false;
    }

    client->fb = _malloc(fb_pixels * sizeof(uint32_t));
    if (!client->fb)
        return false;
    client->fb_pixels = fb_pixels;

    client->bpp = client->fmt.bpp / 8;

    if (client->bpp < 1) 
        client->bpp = 1;
    if (client->bpp > 4) 
        client->bpp = 4;

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
        uint8_t fb_req_inc[] = { 3, 0, 0,0, 0,0, 0,0, 0,0 };
        fb_req_inc[6] = (self->fbw >> 8) & 0xFF;
        fb_req_inc[7] = self->fbw & 0xFF;
        fb_req_inc[8] = (self->fbh >> 8) & 0xFF;
        fb_req_inc[9] = self->fbh & 0xFF;
        if (!write_exact(self->fd, fb_req_inc, 10))
            self->break_loop = 1;
        self->need_update = 0;
    }

    // Read message type (blocking read)
    uint8_t msg_type;
    if (!vnc_client_read_u8(self, &msg_type)) 
    {
        if (!self->break_loop) 
            fprintf(stderr, "Server disconnected\n");
        self->break_loop = 1;
        return;
    }

    if (msg_type == 0) 
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
                    if (!vnc_client_rect_ok(self, rx, ry, rw, rh)) 
                    {
                        fprintf(stderr, "Rect out of bounds: %u,%u %ux%u (fb %u x %u)\n",
                            rx, ry, rw, rh, self->fbw, self->fbh);
                        msg_ok = 0;
                        break;
                    }
                    if (encoding == 0) 
                    {
                        size_t pix_bytes = (size_t)rw * rh * (size_t)self->bpp;
                        uint8_t* pixels = (uint8_t*)malloc(pix_bytes ? pix_bytes : 1);
                        if (!pixels) 
                        { 
                            msg_ok = 0; 
                            break; 
                        }
                        if (!vnc_client_read_bytes(self, pixels, pix_bytes)) 
                        { 
                            free(pixels); 
                            msg_ok = 0; 
                            break; 
                        }
                        printf("read on page\n");
                        const uint8_t* src = pixels;
                        for (int y = 0; y < rh; ++y)
                            for (int x = 0; x < rw; ++x) 
                            {
                                self->fb[(ry + y) * self->fbw + (rx + x)] = pixel_to_32bit(src, &self->fmt);
                                src += self->bpp;
                            }
                        free(pixels);

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
                            fprintf(stderr, "CopyRect source out of bounds\n");
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
                    else 
                    {
                        fprintf(stderr, "Unsupported encoding: %d\n", (int)encoding);
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
        /*
        if (self->disp) {
            display_publish_frame(self->disp, self->fb, self->fb_pixels);
            display_push_event(self->disp, EVT_UPDATE_FRAMEBUFFER);
        }
        */
        printf("update display\n");
        self->need_update = 1;

    }
    else if (msg_type == 1) {
        uint8_t pad;
        uint8_t fcbuf[2], ncbuf[2];
        if (!vnc_client_read_u8(self, &pad)) { self->break_loop = 1; return; }
        if (!vnc_client_read_bytes(self, fcbuf, 2)) { self->break_loop = 1; return; }
        if (!vnc_client_read_bytes(self, ncbuf, 2)) { self->break_loop = 1; return; }
        uint16_t ncolors = (uint16_t)(((uint16_t)ncbuf[0] << 8) | ncbuf[1]);
        if ((size_t)ncolors * 6 > (1 << 20)) {
            fprintf(stderr, "Oversized colour map\n");
            self->break_loop = 1;
            return;
        }
        uint8_t* cmap = (uint8_t*)_malloc((size_t)ncolors * 6);
        if (cmap) {
            if (!vnc_client_read_bytes(self, cmap, (size_t)ncolors * 6)) { _free(cmap); self->break_loop = 1; return; }
            _free(cmap);
        }

    }
    else if (msg_type == 2) {
        // Bell
    }
    else if (msg_type == 3) {
        uint8_t pad[3];
        if (!vnc_client_read_bytes(self, pad, 3)) { self->break_loop = 1; return; }
        uint8_t clbuf[4];
        if (!vnc_client_read_bytes(self, clbuf, 4)) { self->break_loop = 1; return; }
        uint32_t clen = ((uint32_t)clbuf[0] << 24) | ((uint32_t)clbuf[1] << 16) |
            ((uint32_t)clbuf[2] << 8) | clbuf[3];
        if (clen > (1 << 20)) {
            fprintf(stderr, "Oversized server cut-text: %u bytes\n", clen);
            self->break_loop = 1;
            return;
        }
        uint8_t* ctext = (uint8_t*)_malloc(clen ? clen : 1);
        if (ctext) {
            if (!vnc_client_read_bytes(self, ctext, clen)) { _free(ctext); self->break_loop = 1; return; }
            _free(ctext);
        }
    }
    else {
        fprintf(stderr, "Unknown message type: %d\n", (int)msg_type);
        self->break_loop = 1;
        return;
    }
}

void vnc_client_run(vnc_client_t* client)
{
    // Send SetEncodings
    uint8_t setenc[] = 
    {
        2,          // message type
        0,          // padding
        0, 2,       // number of encodings
        0, 0, 0, 7,  // Tight (h/w decode on embedded target)
        0, 0, 0, 1, // CopyRect
#if 0
        0, 0, 0, 0, // Raw (desktop fallback)
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
#if 0
    uint8_t fb_req_full[] = 
    { 
        3, 0, 0, 0, 0, 0, 0, 0, 0, 0 
    };

    fb_req_full[6] = (client->fbw >> 8) & 0xFF;
    fb_req_full[7] = client->fbw & 0xFF;
    fb_req_full[8] = (client->fbh >> 8) & 0xFF;
    fb_req_full[9] = client->fbh & 0xFF;

    if (!write_exact(client->fd, fb_req_full, 10)) 
    {
        vnc_log_append(client->scrn, "[Client] Failed FB request\n");
        client->break_loop = true;
        return;
    }
#endif

    while (vnc_client_isOk(client))
        vnc_client_loop(client);
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
        vnc_log_append(client->scrn, "[Client] _free Frame Buffer\n");
        _free(client->fb);
    }
    /*
    if (client->rbuf_ptr_)
    {
        vnc_log_append(client->scrn, "[Client] _free Receive Buffer\n");
        _free(client->rbuf_ptr_);
    }
    */

    if (client->fd != INVALID_SOCKET)
    {
        vnc_log_append(client->scrn, "[Client] Close Socket\n");
        closesocket(client->fd);
    }
}



static void vnc_client_init_zstreams(vnc_client_t* client)
{
    for (int i = 0; i < sizeof(client->zstream) / sizeof(client->zstream[0]); ++i)
        inflateInit(&client->zstream[i]);
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
            *len |= (size_t)b << 14;
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

static int vnc_client_tight_decode(vnc_client_t* self, int rx, int ry, int rw, int rh, int bpp)
{
    uint32_t* fb = self->fb;
    int fbw = self->fbw;

    if (!vnc_client_rect_ok(self, rx, ry, rw, rh)) {
        fprintf(stderr, "Tight rect out of bounds\n");
        return 0;
    }

    uint8_t ctrl;
    if (!vnc_client_read_u8(self, &ctrl)) return 0;

    for (int i = 0; i < sizeof(self->zstream) / sizeof(self->zstream[0]); ++i) {
        if (ctrl & (0x10 << i))
            inflateReset(&self->zstream[i]);
    }

    uint8_t comp_type = ctrl & 0x0F;

    if (comp_type == 0x08) {
        uint8_t fill_buf[4];
        if (!vnc_client_read_bytes(self, fill_buf, (size_t)bpp)) return 0;
        uint32_t color = pixel_to_32bit(fill_buf, &self->fmt);
        for (int y = 0; y < rh; ++y)
            for (int x = 0; x < rw; ++x)
                fb[(ry + y) * fbw + (rx + x)] = color;
        return 1;
    }

    if (comp_type == 0x09) {
        size_t jpeg_len;
        if (!vnc_client_read_clen(self, &jpeg_len)) return 0;
        uint8_t* jpeg_data = (uint8_t*)_malloc(jpeg_len ? jpeg_len : 1);
        if (!jpeg_data) return 0;
        if (!vnc_client_read_bytes(self, jpeg_data, jpeg_len)) { _free(jpeg_data); return 0; }

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
                fprintf(stderr, "Tight JPEG size mismatch\n");
                jpeg_abort_decompress(&cinfo);
                jpeg_destroy_decompress(&cinfo);
                _free(jpeg_data);
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
        _free(jpeg_data);
        return 1;
    }

    bool has_palette = (comp_type & 0x04) != 0;
    int stream_idx = (comp_type & 0x03) - 1;
    bool is_compressed = (comp_type & 0x03) != 0;
    uint32_t* palette = NULL;
    int palette_size = 0;

    if (has_palette) {
        uint8_t psize;
        if (!vnc_client_read_u8(self, &psize)) return 0;
        palette_size = psize + 1;
        palette = (uint32_t*)_malloc((size_t)palette_size * sizeof(uint32_t));
        if (!palette) return 0;
        uint8_t pal_buf[4 * 256];
        if (!vnc_client_read_bytes(self, pal_buf, (size_t)palette_size * bpp)) { _free(palette); return 0; }
        for (int i = 0; i < palette_size; ++i)
            palette[i] = pixel_to_32bit(pal_buf + i * bpp, &self->fmt);
    }

    uint8_t* raw_data = NULL;
    size_t pixel_count = (size_t)rw * rh;

    if (is_compressed) {
        size_t zlen;
        if (!vnc_client_read_clen(self, &zlen)) { _free(palette); return 0; }
        uint8_t* compressed = (uint8_t*)_malloc(zlen ? zlen : 1);
        if (!compressed) { _free(palette); return 0; }
        if (!vnc_client_read_bytes(self, compressed, zlen)) { _free(compressed); _free(palette); return 0; }

        size_t uncomp_size = has_palette ? pixel_count : (pixel_count * bpp + 1);
        raw_data = (uint8_t*)_malloc(uncomp_size ? uncomp_size : 1);
        if (!raw_data) { _free(compressed); _free(palette); return 0; }
        if (!zlib_decompress(&self->zstream[stream_idx], compressed, zlen, raw_data, uncomp_size)) {
            _free(compressed); _free(raw_data); _free(palette);
            return 0;
        }
        _free(compressed);
    }

    if (has_palette) {
        if (!is_compressed) {
            raw_data = (uint8_t*)_malloc(pixel_count ? pixel_count : 1);
            if (!raw_data) { _free(palette); return 0; }
            if (!vnc_client_read_bytes(self, raw_data, pixel_count)) { _free(raw_data); _free(palette); return 0; }
        }
        for (size_t i = 0; i < pixel_count; ++i) {
            uint8_t idx = raw_data[i];
            uint32_t color = (idx < (uint8_t)palette_size) ? palette[idx] : 0xFF000000;
            int x = rx + (int)(i % (size_t)rw);
            int y = ry + (int)(i / (size_t)rw);
            fb[y * fbw + x] = color;
        }
        _free(raw_data);
        _free(palette);
        return 1;
    }

    if (is_compressed) {
        uint8_t filter = raw_data[0];
        uint8_t* pixel_data = raw_data + 1;
        if (filter == 1)
            tight_filter_gradient(pixel_data, rw, rh, bpp);
        else if (filter != 0) {
            fprintf(stderr, "Unsupported Tight filter: %d\n", (int)filter);
            _free(raw_data);
            return 0;
        }
        for (int y = 0; y < rh; ++y)
            for (int x = 0; x < rw; ++x) {
                fb[(ry + y) * fbw + (rx + x)] = pixel_to_32bit(pixel_data + (y * rw + x) * bpp, &self->fmt);
            }
        _free(raw_data);
        return 1;
    }

    bool has_gradient = (ctrl & 0x20) != 0;
    if (has_gradient) {
        uint8_t filter;
        if (!vnc_client_read_u8(self, &filter)) {
            fprintf(stderr, "TIGHT_GRADIENT: read filter failed\n");
            return 0;
        }
        if (filter == 0x01) {
            size_t total = (size_t)rw * rh * bpp;
            uint8_t* raw = (uint8_t*)_malloc(total ? total : 1);
            if (!raw) return 0;
            if (!vnc_client_read_bytes(self, raw, total)) {
                fprintf(stderr, "TIGHT_GRADIENT: read pixels failed\n");
                _free(raw);
                return 0;
            }
            tight_filter_gradient(raw, rw, rh, bpp);
            for (int y = 0; y < rh; ++y)
                for (int x = 0; x < rw; ++x)
                    fb[(ry + y) * fbw + (rx + x)] = pixel_to_32bit(raw + (y * rw + x) * bpp, &self->fmt);
            _free(raw);
        }
        else if (filter != 0x00) {
            fprintf(stderr, "TIGHT_GRADIENT: unknown filter %d\n", (int)filter);
            return 0;
        }
        else {
            size_t total = (size_t)rw * rh * bpp;
            uint8_t* raw = (uint8_t*)_malloc(total ? total : 1);
            if (!raw) return 0;
            if (!vnc_client_read_bytes(self, raw, total)) return 0;
            for (int y = 0; y < rh; ++y)
                for (int x = 0; x < rw; ++x)
                    fb[(ry + y) * fbw + (rx + x)] = pixel_to_32bit(raw + (y * rw + x) * bpp, &self->fmt);
            _free(raw);
        }
    }
    else {
        uint8_t* row_buf = (uint8_t*)_malloc((size_t)rw * bpp);
        if (!row_buf) return 0;
        for (int y = 0; y < rh; ++y) {
            if (!vnc_client_read_bytes(self, row_buf, (size_t)rw * bpp)) { _free(row_buf); return 0; }
            const uint8_t* src = row_buf;
            for (int x = 0; x < rw; ++x) {
                fb[(ry + y) * fbw + (rx + x)] = pixel_to_32bit(src, &self->fmt);
                src += bpp;
            }
        }
        _free(row_buf);
    }

    return 1;
}
