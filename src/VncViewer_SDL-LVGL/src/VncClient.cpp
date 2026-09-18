// VncClient.cpp
//

#include "VncClient.h"
#include "VncApplication.h"
#include "encrypt/d3des.h"
#include "jpeglib.h"
#include "zlib_inflate.h"
#include "zrle_decode.h"
#include "tight_decode.h"





//
//
//

VncClient::VncClient(VncApplication* app)
    : m_app(app)
    , m_addr("")
    , m_port(5900)
    , m_pass("")
    , m_sock(INVALID_SOCKET)
    , m_errno(ERROR_NONE)
    , m_thread(NULL)
    , fb(NULL)
    , rbuf_ptr(NULL)
    , temp_ptr(NULL)
{
    if (EventType == -1)
        EventType = SDL_RegisterEvents(1);

    m_mutex = SDL_CreateMutex();
}

VncClient::~VncClient()
{
    cleanup();
}


Uint32 VncClient::EventType = -1;




bool VncClient::connectToServer(const char* addr, unsigned short port, const char* pass)
{
    // save connection information
    strncpy(m_addr, addr, sizeof(m_addr));
    m_port = port;
    strncpy(m_pass, pass, sizeof(m_pass));

    //
    LV_LOG_INFO("VncClient::connectToServer(%s, %d)\n", addr, port);
    m_thread = SDL_CreateThread(ClientProc, "client", this);
    if (!m_thread)
        return false;

    return true;
}

void VncClient::disconnect()
{
    if (m_sock != INVALID_SOCKET)
    {
        LV_LOG_INFO("Close VNC socket handle\n");
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
    }
}

void VncClient::reset()
{
    LV_LOG_INFO("VncClient::reset()\n");
    if (m_sock != INVALID_SOCKET)
    {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
    }

    if (m_thread)
    {
        int status;
        SDL_WaitThread(m_thread, & status);
        m_thread = NULL;
    }

    cleanup();
}



int SDLCALL VncClient::ClientProc(void* data)
{
    //
    VncClient* client = (VncClient *)data;

    client->m_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client->m_sock == INVALID_SOCKET)
    {
        LV_LOG_ERROR("socket failed: errno(%d)\n", errno);
        client->m_errno = ERROR_SOCKET_FAILED;
        client->m_app->postUserEvent(VncClient::EventType, VncClient::EVENT_CLIENT_FAILED, &client->m_errno, (void *)0);
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(client->m_port);
    addr.sin_addr.s_addr = inet_addr(client->m_addr);

    int ret = connect(client->m_sock, (const struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0 /*&& errno != EINPROGRESS && errno != EINTR*/) 
    {
        LV_LOG_ERROR("connect failed: ret(%d), errno(%d)\n", ret, errno);
        client->m_errno = ERROR_CONNECT_FAILED;
        client->m_app->postUserEvent(VncClient::EventType, VncClient::EVENT_CLIENT_FAILED, &client->m_errno, (void *)0);
        closesocket(client->m_sock);
        client->m_sock = INVALID_SOCKET;
        return 1;
    }
    LV_LOG_INFO("Connected to VNC Server: %s:#%d\n", client->m_addr, client->m_port);

    if (!client->handshake())
    {
        LV_LOG_ERROR("handshake failed\n");
        client->m_errno = ERROR_HANDSHAKE_FAILED;
        client->m_app->postUserEvent(VncClient::EventType, VncClient::EVENT_CLIENT_FAILED, &client->m_errno, (void *)0);
        return 1;
    }

    //
    LV_LOG_INFO("Completed handshaking. Now start frame updating\n");
    client->m_app->postUserEvent(VncClient::EventType, VncClient::EVENT_CLIENT_STARTED, (void *)0, (void *)0);
    client->run();

    //
    // cleanup ??
    //
    client->m_app->postUserEvent(VncClient::EventType, VncClient::EVENT_CLIENT_FINISHED, (void *)0, (void *)0);

    LV_LOG_INFO("Terminated client task\n");
    return 0;
}

bool VncClient::handshake()
{
    // 1. Protocol version
    uint8_t ver[14];
    memset(ver, 0, sizeof(ver));
    if (!readExact(this->m_sock, ver, 12)) 
        return false;
    LV_LOG("Protocol Version: %s\n", (char*)ver);

    const char* my_ver = "RFB 003.008\n";
    if (!writeExact(this->m_sock, (const uint8_t*)my_ver, 12)) 
        return false;

    // 2. Security
    uint8_t nsec;
    if (!readExact(this->m_sock, &nsec, 1) || nsec == 0)
        return false;

    uint8_t* types = (uint8_t *)lv_malloc(nsec);
    if (!types)
        return false;
    if (!readExact(this->m_sock, types, nsec))
    {
        lv_free(types);
        return false;
    }

    uint8_t chosen = 0;
    bool have_none = false, have_vncauth = false;
    for (int i = 0; i < nsec; ++i) 
    {
        if (types[i] == 1) 
            have_none = true;
        if (types[i] == 2) 
            have_vncauth = true;
    }
    lv_free(types);

    if (have_vncauth && (m_pass && m_pass[0]))
    {
        chosen = 2;
    }
    else if (have_none) 
    {
        chosen = 1;
    }
    else 
    {
        LV_LOG("No supported security type\n");
        return false;
    }

    if (!writeExact(this->m_sock, &chosen, 1)) 
        return false;

    if (chosen == 2) 
    {
        uint8_t challenge[16];
        if (!readExact(this->m_sock, challenge, 16))
            return false;

        uint8_t response[16];
        encryptChallenge(challenge, m_pass, response);
        if (!writeExact(this->m_sock, response, 16)) 
            return false;
    }

    // Security result (4 bytes for RFB 3.8+)
    //if (chosen == 2 || chosen == 1) 
    {
        uint32_t result;
        if (!readExact(this->m_sock, (uint8_t*)&result, 4)) 
            return false;

        if (ntohl(result) != 0) 
        {
            LV_LOG_ERROR("Security failed, result=%d\n", ntohl(result));
            return false;
        }

        LV_LOG_INFO("Security OK\n");
    }

    // 3. ClientInit
    uint8_t shared = 1;
    if (!writeExact(this->m_sock, &shared, 1)) 
        return false;

    // 4. ServerInit
    ServerInit si;
    if (!readExact(this->m_sock, (uint8_t*)&si, sizeof(si)))
        return false;

    this->fbw = ntohs(si.fb_width);
    this->fbh = ntohs(si.fb_height);
    this->fmt = si.fmt;
    this->fmt.red_max = ntohs(this->fmt.red_max);
    this->fmt.green_max = ntohs(this->fmt.green_max);
    this->fmt.blue_max = ntohs(this->fmt.blue_max);
    LV_LOG_INFO("Frame Resolution: %d x %d\n", this->fbw, this->fbh);
    LV_LOG_INFO("PixelFormat: BPP=%d, DEPTH=%d, ENDIAN=%s\n", this->fmt.bpp, this->fmt.depth, this->fmt.big_endian ? "Big" : "Little");
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
            LV_LOG_WARN("Desktop name too long: %u\n", nlen);
            return false;
        }

        char* name = (char *)lv_malloc(nlen + 1);
        if (!name)
            return false;
        memset(name, 0, nlen + 1);
        if (!readExact(this->m_sock, (uint8_t *)name, nlen))
        {
            lv_free(name);
            return false;
        }

        LV_LOG_INFO("Desktop: %s\n", name);
        free(name);
    }

#if SUPPORT_SETPIXELFORMAT
    // 5. SetPixelFormat
    uint8_t msg[4] = { 0, 0, 0, 0 };
    if (!writeExact(this->m_sock, &msg[0], sizeof(msg)))
    {
        LV_LOG_ERROR("SetPixelFormat phase1 failed.\n");
        return false;
    }

    this->fmt.bpp = 16;
    this->fmt.depth = 16;
    this->fmt.big_endian = 0;
    this->fmt.true_color = 1;
    this->fmt.red_max = 31; // 2^5 - 1
    this->fmt.green_max = 63; // 2^6 - 1
    this->fmt.blue_max = 31; // 2^5 - 1
    this->fmt.red_shift = 11;
    this->fmt.green_shift = 5;
    this->fmt.blue_shift = 0;

    if (!writeExact(this->m_sock, &this->fmt, sizeof(this->fmt)))
    {
        LV_LOG_ERROR("SetPixelFormat phase2 failed.\n");
        return false;
    }
#endif

    const size_t fb_pixels = this->fbw * this->fbh; // *(this->fmt.bpp / 8); // sizeof(uint16_t);
    if (fb_pixels > 0x10000000) 
    {
        LV_LOG_ERROR("Framebuffer too large: %d x %d\n", this->fbw, this->fbh);
        return false;
    }

    this->bpp = this->fmt.bpp / 8;

    if (this->bpp < 1) 
        this->bpp = 1;
    if (this->bpp > 4) 
        this->bpp = 4;

    this->fb = (uint32_t *)malloc(fb_pixels * sizeof(uint32_t));
    if (!this->fb)
        return false;
    this->fb_pixels = fb_pixels;

    this->rbuf_ptr = (uint8_t *)malloc(fb_pixels * this->bpp);
    if (!this->rbuf_ptr)
        return false;
    this->rbuf_len = (uint32_t)(fb_pixels * this->bpp);

    this->temp_ptr = (uint8_t *)malloc(fb_pixels * this->bpp);
    if (!this->temp_ptr)
        return false;
    this->temp_len = (uint32_t)(fb_pixels * this->bpp);

    return true;
}

#if !USE_ZLIB
void* custom_zalloc(void* opaque, size_t items, size_t size) {
    #if BUILD_ON_ESP32
    void* ptr = heap_caps_malloc(items * size, MALLOC_CAP_SPIRAM);
    #else
    void* ptr = malloc(items * size);
    #endif
    if (!ptr) {
        LV_LOG_ERROR("custom_zalloc(%lu) failed: \n", size * items);
    }
    return ptr;
}

void custom_zfree(void* opaque, void* ptr) {
    #if BUILD_ON_ESP32
    heap_caps_free(ptr);
    #else
    free(ptr);
    #endif
}
#endif

void VncClient::run()
{
    // vnc_client_init_zstreams(client);
    for (int i = 0; i < sizeof(m_zstrm) / sizeof(m_zstrm[0]); ++i)
    {
        memset(&m_zstrm[i], 0, sizeof(z_stream));
        
        m_zstrm[i].zalloc = Z_NULL;
        m_zstrm[i].zfree = Z_NULL;
        m_zstrm[i].opaque = Z_NULL;
        #if !USE_ZLIB
        m_zstrm[i].zalloc = custom_zalloc;
        m_zstrm[i].zfree = custom_zfree;
        #endif

        inflateInit(&m_zstrm[i]);
    }


    //
    uint32_t updateCount = 0;
    m_needUpdate = true;
    m_breakLoop = false;

    //
    setEncodings();

    //
    while (cond())
    {
        // 
        if (m_needUpdate)
        {
            if (!frameBufferUpdateRequest(updateCount++ == 0))
                break;

            m_needUpdate = false;
        }

        // read message-type
        unsigned char msgType;
        if (!read_u8(&msgType))
            break;

        switch (msgType)
        {
        case Message_FrameBufferUpdate:
            onFrameBufferUpdate();
            break;
        case Message_SetColourMapEntries:
            onSetColourMapEntries();
            break;
        case Message_Bell:
            onBell();
            break;
        case Message_ServerCutText:
            onServerCutText();
            break;
        default:
            LV_LOG_ERROR("Unknown message type: %d\n", (int)msgType);
            m_breakLoop = 1;
            break;
        }
    }

    // vnc_client_destroy_zstreams
    for (int i = 0; i < sizeof(m_zstrm) / sizeof(m_zstrm[0]); ++i)
        inflateEnd(&m_zstrm[i]);
}

void VncClient::cleanup()
{
    LV_LOG("VncClient::cleanup()\n");
    if (this->m_sock != INVALID_SOCKET)
    {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
    }

    if (m_thread)
    {
        #if 1
        SDL_DetachThread(m_thread);
        #else
        int status;
        SDL_WaitThread(m_thread, &status);
        #endif
        m_thread = NULL;
    }


    //
    if (this->fb)
    {
        free(this->fb);
        this->fb = NULL;
    }

    if (this->rbuf_ptr)
    {
        free(this->rbuf_ptr);
        this->rbuf_ptr = NULL;
    }

    if (this->temp_ptr)
    {
        free(this->temp_ptr);
        this->temp_ptr = NULL;
    }

    if (m_mutex)
    {
        SDL_DestroyMutex(m_mutex);
        m_mutex = NULL;
    }
}


bool VncClient::cond()
{
    if (m_sock == INVALID_SOCKET)
        return false;
    
    if (m_breakLoop)        
        return false;

    return true;
}

bool VncClient::readExact(SOCKET fd, uint8_t* buf, size_t len)
{
    while (len > 0) 
    {
        int n = recv(fd, (char *)buf, len, 0);
        if (n > 0) 
        {
            buf += n; len -= n;
            continue;
        }

        if (n == 0) // disconnected
        {
            LV_LOG_WARN("Socket Disconnected");
            return false;
        }

        n = 0 - n; // error
        if (n == EINTR)
            continue;
        if (n == EWOULDBLOCK)
            return false;

        return false;
    }

    return true;
}

bool VncClient::read_clen(size_t* len)
{
    uint8_t b;
    if (!read_u8(&b)) 
        return false;

    *len = b & 0x7F;
    if (b & 0x80) 
    {
        if (!read_u8(&b)) 
            return false;

        *len |= (size_t)(b & 0x7F) << 7;
        if (b & 0x80) 
        {
            if (!read_u8(&b)) 
                return false;

            *len |= (size_t)(b & 0x7F) << 14;
        }
    }

    return true;
}

bool VncClient::writeExact(SOCKET fd, const uint8_t* buf, size_t len)
{
    while (len > 0) 
    {
        int n = send(fd, (char *)buf, len, 0);
        if (n <= 0) 
        {
            if (n == -EINTR) 
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

void VncClient::encryptChallenge(const uint8_t* challenge, const char* password, uint8_t* response)
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

bool VncClient::frameBufferUpdateRequest(bool full)
{
    uint8_t fb_req_full[] = 
    { 
        3, 1, 0, 0, 0, 0, 0, 0, 0, 0 
    };

    if (full)
        fb_req_full[1] = 0;

    fb_req_full[6] = (this->fbw >> 8) & 0xFF;
    fb_req_full[7] = this->fbw & 0xFF;
    fb_req_full[8] = (this->fbh >> 8) & 0xFF;
    fb_req_full[9] = this->fbh & 0xFF;

    //
    if (writeExact(m_sock, fb_req_full, 10)) 
        return true;

    LV_LOG_ERROR("Failed FramebufferUpdate request\n");
    m_breakLoop = true;

    return false;
}

#define USE_RAW_ENCODING    0
#define USE_TIGHT_ENCODING  1
#define USE_ZRLE_ENCODING   2
#define VNC_ENCODING        USE_TIGHT_ENCODING

bool VncClient::setEncodings()
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
                                // 0xFFFFFFE0 - QualityLevel(0 ~ 9)
                                //       0xE0 : Quality 0 (-32), Low quality, Low bandwidth
                                //       0xE3 : Quality 3 (-29)
                                //       0xE6 : Quality 6 (-26)
                                //       0xE9 : Quality 8 (-23), High quality, High bandwidth
#else
        0, 0, 0, 1, // CopyRect
#endif
    };

    if (writeExact(m_sock, setenc, sizeof(setenc))) 
        return true;

    //
    LV_LOG_ERROR("Failed to send SetEncodings\n");
    m_breakLoop = true;

    return false;
}



void VncClient::onFrameBufferUpdate()
{
    int msg_ok = 1;
    uint8_t pad;
    if (!read_u8(&pad)) 
        msg_ok = 0; 

    if (msg_ok) 
    {
        uint8_t nrbuf[2];
        if (!read_bytes(nrbuf, 2)) 
            msg_ok = 0; 

        if (msg_ok) 
        {
            uint16_t nrects = (uint16_t)(((uint16_t)nrbuf[0] << 8) | nrbuf[1]); // ntohs()
            //LV_LOG_INFO("nrets = %d\n", nrects);
            for (int i = 0; i < nrects && msg_ok; ++i) 
            {
                uint8_t rect_hdr[12];
                if (!read_bytes(rect_hdr, 12)) 
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
                //LV_LOG_INFO("rect #%d: (%d, %d, %d, %d), %d\n", i, rx, ry, rw, rh, encoding);
                if (!checkRectangle(rx, ry, rw, rh)) 
                {
                    LV_LOG_ERROR("Rect out of bounds: %u,%u %ux%u (fb %u x %u)\n",
                        rx, ry, rw, rh, this->fbw, this->fbh);
                    msg_ok = 0;
                    break;
                }

                if (encoding == 0)
                    msg_ok = decodeRaw(rx, ry, rw, rh, this->bpp);
                else if (encoding == 1) 
                    msg_ok = decodeCopyRect(rx, ry, rw, rh, this->bpp);
                else if (encoding == 7) 
                    msg_ok = decodeTight(rx, ry, rw, rh, this->bpp);
                else if (encoding == 16)
                    msg_ok = decodeZrle(rx, ry, rw, rh, this->bpp);
                else
                {
                    LV_LOG_ERROR("Unsupported encoding: %d\n", (int)encoding);
                    msg_ok = 0;
                    break;
                }
            }
        }
    }

    if (!msg_ok) 
    {
        m_breakLoop = 1;
        return;
    }

    // Publish the decoded frame to the Display-owned canvas buffer and
    // signal the main thread (display_loop drains the event queue)
    /*
    this->m_app->postUserEvent(VncClient::EventType, VncClient::EVENT_CLIENT_UPDATE_FRAME, (void *)this->fb, (void *)(this->fb_pixels * sizeof(uint32_t)));
    */
    //
    // copy fb to lvgl canvas
    //
    m_app->updateFrame(this->fb, this->fb_pixels * sizeof(uint32_t));

    //LV_LOG_INFO("Update display\n");
    this->m_needUpdate = true;
}

void VncClient::onSetColourMapEntries()
{
    uint8_t pad;
    uint8_t fcbuf[2], ncbuf[2];
    if (!read_u8(&pad)) { m_breakLoop = 1; return; }
    if (!read_bytes(fcbuf, 2)) { m_breakLoop = 1; return; }
    if (!read_bytes(ncbuf, 2)) { m_breakLoop = 1; return; }
    uint16_t ncolors = (uint16_t)(((uint16_t)ncbuf[0] << 8) | ncbuf[1]);
    if ((size_t)ncolors * 6 > (1 << 20)) 
    {
        LV_LOG_ERROR("Oversized colour map\n");
        m_breakLoop = 1;
        return;
    }

    uint8_t* cmap = (uint8_t*)lv_malloc((size_t)ncolors * 6);
    if (cmap) 
    {
        if (!read_bytes(cmap, (size_t)ncolors * 6))
        {
            lv_free(cmap);
            m_breakLoop = 1;
            return;
        }

        lv_free(cmap);
    }
}

void VncClient::onBell()
{
    // nop
}

void VncClient::onServerCutText()
{
    uint8_t pad[3];
    if (!read_bytes(pad, 3)) {
        m_breakLoop = 1; 
        return; 
    }

    uint8_t clbuf[4];
    if (!read_bytes(clbuf, 4))
    { 
        m_breakLoop = 1; 
        return; 
    }

    uint32_t clen = ((uint32_t)clbuf[0] << 24) | 
        ((uint32_t)clbuf[1] << 16) |
        ((uint32_t)clbuf[2] << 8) | clbuf[3];
    if (clen > (1 << 20)) 
    {
        LV_LOG_ERROR("Oversized server cut-text: %u bytes\n", clen);
        m_breakLoop = 1;
        return;
    }

    uint8_t* ctext = (uint8_t*)lv_malloc(clen ? clen : 1);
    if (ctext) 
    {
        if (!read_bytes(ctext, clen))
        {
            lv_free(ctext);
            m_breakLoop = 1;
            return;
        }

        lv_free(ctext);
    }    
}


bool VncClient::checkRectangle(int rx, int ry, int rw, int rh)
{
    return rx >= 0 && ry >= 0 && rw > 0 && rh > 0 &&
        (size_t)rx + (size_t)rw <= (size_t)this->fbw &&
        (size_t)ry + (size_t)rh <= (size_t)this->fbh;
}

bool VncClient::decodeRaw(int rx, int ry, int rw, int rh, int bpp)
{
    size_t pix_bytes = (size_t)rw * rh * (size_t)this->bpp;
    uint8_t* pixels = this->rbuf_ptr;
    if (!read_bytes(pixels, pix_bytes))
        return false;
    //LV_LOG_VERBOSE("receive done\n");

    const uint8_t* src = pixels;
    for (int y = 0; y < rh; ++y)
    {
        //int offset = y * this->fbw * bpp + rx * bpp;
        int offset = (ry + y) * this->fbw + rx;
        for (int x = 0; x < rw; ++x)
        {
            this->fb[offset + x] = pixel_to_32bit(src, &this->fmt);
            src += this->bpp;
        }
    }

    return true;
}

bool VncClient::decodeCopyRect(int rx, int ry, int rw, int rh, int bpp)
{
    uint8_t copy_hdr[4];
    if (!read_bytes(copy_hdr, 4)) 
        return false;

    int src_x = ((int)copy_hdr[0] << 8) | copy_hdr[1];
    int src_y = ((int)copy_hdr[2] << 8) | copy_hdr[3];
    if (!checkRectangle(src_x, src_y, rw, rh)) 
    {
        LV_LOG_ERROR("CopyRect source out of bounds\n");
        return false;
    }

    for (int y = 0; y < rh; ++y)
        for (int x = 0; x < rw; ++x)
            this->fb[(ry + y) * this->fbw + (rx + x)] =
            this->fb[(src_y + y) * this->fbw + (src_x + x)];

    return true;
}

bool VncClient::decodeTight(int rx, int ry, int rw, int rh, int bpp)
{
    (void)bpp;

    uint32_t* fb = this->fb;
    int fbw = this->fbw;
    const PixelFormat* fmt = &this->fmt;

    if (!checkRectangle(rx, ry, rw, rh)) {
        LV_LOG_ERROR("Tight rect out of bounds\n");
        return false;
    }

    uint8_t ctrl;
    if (!read_u8(&ctrl)) 
        return false;

    for (int i = 0; i < (int)(sizeof(this->m_zstrm) / sizeof(this->m_zstrm[0])); ++i) 
    {
        if (ctrl & (1u << i))
            inflateReset(&this->m_zstrm[i]);
    }

    uint8_t ctype = ctrl >> 4;

    size_t tpb = (fmt->bpp == 32 && fmt->depth == 24 &&
        fmt->red_max == 0xFF && fmt->green_max == 0xFF && fmt->blue_max == 0xFF)
        ? 3 : (size_t)(fmt->bpp / 8);

    if (ctype == 0x08) {
        uint8_t fill_buf[4];
        if (tpb > sizeof(fill_buf)) 
            return false;
        if (!read_bytes(fill_buf, tpb)) 
            return false;
        uint32_t color = tight_pixel_to_32bit(fill_buf, fmt, tpb);
        for (int y = 0; y < rh; ++y)
            for (int x = 0; x < rw; ++x)
                fb[(ry + y) * fbw + (rx + x)] = color;
        return true;
    }

    if (ctype == 0x09)
        return decodeJpeg(rx, ry, rw, rh, bpp);

    int uncompressed = (ctype == 0x0A || ctype == 0x0E);
    if (!uncompressed && ctype > 0x07) {
        LV_LOG_ERROR("Invalid Tight compression type %u\n", (unsigned)ctype);
        return false;
    }
    int stream_idx = (int)(ctype & 0x03);

    int filter_id = 0;
    if (ctrl & 0x40) {
        uint8_t f;
        if (!read_u8(&f)) 
            return false;
        if (f > 2) 
        {
            LV_LOG_ERROR("Unknown Tight filter id %u\n", (unsigned)f);
            return false;
        }
        filter_id = f;
    }

    int bits_pixel;
    uint32_t palette[256];
    int pal_size = 0;

    if (filter_id == 1) {
        uint8_t n;
        if (!read_u8(&n)) 
            return false;
        pal_size = (int)n + 1;
        if (pal_size < 2 || tpb > 4) 
        {
            LV_LOG_ERROR("Invalid Tight palette size %d\n", pal_size);
            return false;
        }
        {
            uint8_t pal_buf[256 * 4];
            if (!read_bytes(pal_buf, (size_t)pal_size * tpb)) 
                return false;
            for (int i = 0; i < pal_size; ++i)
                palette[i] = tight_pixel_to_32bit(pal_buf + i * tpb, fmt, tpb);
        }
        bits_pixel = (pal_size == 2) ? 1 : 8;
    }
    else {
        if (filter_id == 2 && tpb != 3) {
            LV_LOG_ERROR("Tight gradient unsupported for %u bytes/pixel\n", (unsigned)tpb);
            return false;
        }
        bits_pixel = (tpb == 3) ? 24 : (int)(tpb * 8);
    }

    size_t row_size = ((size_t)rw * bits_pixel + 7) / 8;
    size_t data_len = row_size * (size_t)rh;
    uint8_t* data = (uint8_t*)malloc(data_len ? data_len : 1);
    if (!data) 
        return false;

    if (!uncompressed && data_len < 12) 
    {
        /* Small rect: filtered pixel data follows verbatim, no compact length. */
        if (!read_bytes(data, data_len)) 
            goto fail;
    }
    else 
    {
        size_t zlen;
        if (!read_clen(&zlen)) goto fail;
        if (zlen == 0 || zlen > (size_t)this->fb_pixels * 4 + 65536) 
        {
            LV_LOG_ERROR("Tight bad data length %u\n", (unsigned)zlen);
            goto fail;
        }
        uint8_t* buf = (uint8_t*)malloc(zlen);
        if (!buf) goto fail;
        if (!read_bytes(buf, zlen)) { free(buf); goto fail; }

        bool ok;
        if (uncompressed) 
        {
            ok = (zlen == data_len);
            if (ok)
                memcpy(data, buf, data_len);
            else
                LV_LOG_ERROR("Tight length mismatch: %u != %u\n", (unsigned)zlen, (unsigned)data_len);
        }
        else {
            z_stream* zs = &this->m_zstrm[stream_idx];
            int zrc = zlib_inflate_exact(zs, buf, zlen, data, data_len);
            ok = (zrc == Z_OK);
            if (!ok) 
            {
                LV_LOG_ERROR(
                    "Tight inflate failed (stream %d): zlib=%d msg=%s "
                    "ctype=0x%02X filter=%d pal=%d rw=%d rh=%d tpb=%u "
                    "zlen=%u data_len=%u left_in=%u left_out=%u\n",
                    stream_idx, zrc, zs->msg ? zs->msg : "-", (unsigned)ctype,
                    filter_id, pal_size, rw, rh, (unsigned)tpb,
                    (unsigned)zlen, (unsigned)data_len,
                    (unsigned)zs->avail_in, (unsigned)zs->avail_out);
            }
        }
        free(buf);
        if (!ok) 
            goto fail;
    }

    switch (filter_id) 
    {
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
    return true;

fail:
    free(data);
    return false;
}

bool VncClient::decodeZrle(int rx, int ry, int rw, int rh, int bpp)
{
    (void)bpp;

    uint32_t zlen;
    if (!read_bytes((uint8_t*)&zlen, 4))
        return false;
    zlen = ntohl(zlen);

    if ((size_t)zlen > (size_t)this->rbuf_len)
    {
        LV_LOG_ERROR("ZRLE rect data too large: %u\n", zlen);
        return false;
    }

    //LV_LOG_INFO("ZRLE receiving %u bytes\n", zlen);
    if (!read_bytes(this->rbuf_ptr, zlen))
        return false;

    size_t uncomp_size = 0;
    if (!zlib_decompress(&this->m_zstrm[0], this->rbuf_ptr, zlen, this->temp_ptr, this->temp_len, &uncomp_size))
    {
        LV_LOG_ERROR("ZRLE inflate failed\n");
        return false;
    }

    if (!parse_zrle_buffer(this->temp_ptr, uncomp_size, rx, ry, rw, rh, this->fb, this->fbw, &this->fmt))
    {
        LV_LOG_ERROR("ZRLE tile decode failed\n");
        return false;
    }

    return true;
}

bool VncClient::decodeJpeg(int rx, int ry, int rw, int rh, int bpp)
{
#if USE_JPEGLIB
    uint32_t* fb = this->fb;
    int fbw = this->fbw;

    size_t jpeg_len;
    if (!read_clen(&jpeg_len)) return 0;
    uint8_t* jpeg_data = (uint8_t*)malloc(jpeg_len ? jpeg_len : 1);
    if (!jpeg_data) return 0;
    if (!read_bytes(jpeg_data, jpeg_len)) { free(jpeg_data); return 0; }

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
            LV_LOG_ERROR("Tight JPEG size mismatch\n");
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
#else
    return 0;
#endif  
}

bool VncClient::zlib_decompress(z_stream* zs, const uint8_t* in, size_t inlen, uint8_t* out, size_t outlen, size_t* decompressed_size)
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
