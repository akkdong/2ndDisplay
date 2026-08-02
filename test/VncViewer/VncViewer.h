// VncViewer.h
//

#pragma once

#include <vector>
#include <string>
#include <atomic>
#include <csignal>
#include <zlib.h>
#include "lvgl/lvgl.h"


//
//

class Display;



// ============================================================
// RFB structs
// ============================================================
struct PixelFormat {
    uint8_t bpp;
    uint8_t depth;
    uint8_t big_endian;
    uint8_t true_color;
    uint16_t red_max;
    uint16_t green_max;
    uint16_t blue_max;
    uint8_t red_shift;
    uint8_t green_shift;
    uint8_t blue_shift;
    uint8_t pad[3];
} __attribute__((packed));

struct ServerInit {
    uint16_t fb_width;
    uint16_t fb_height;
    PixelFormat fmt;
    uint32_t name_len;
} __attribute__((packed));



// ============================================================
// VncClient
// ============================================================
class VncClient 
{
    friend class Display;

public:
    VncClient(Display* disp);
public:
    virtual ~VncClient();

    bool connect(const std::string &host, int port);
    bool handshake(const std::string &password);
    void loop();
    void run();

    bool isOk();

    void send_pointer(int x, int y, uint8_t mask);

private:
    void init_zstreams();
    void reset_zstream(int id);
    void destroy_zstreams();

    // Drop already-consumed bytes so the recv buffer cannot grow unboundedly
    void compact_buf();

    // Read more data from socket into buffer (non-blocking)
    // Returns false on disconnect, true otherwise (including EAGAIN)
    bool fill_buf();

    // Wait until buffer has at least 'needed' bytes
    // Returns false only on disconnect/error
    bool wait_buf(size_t needed);

    // Convenience: read one byte
    bool read_u8(uint8_t &v);

    // Convenience: read into buffer
    bool read_bytes(uint8_t *dst, size_t n);

    // Read compact length from buffer
    bool read_clen(size_t &len);

    // Validate that a rectangle fits within the framebuffer
    bool rect_ok(int rx, int ry, int rw, int rh) const;

    bool tight_decode(int rx, int ry, int rw, int rh, int bpp);


private:
    int fd_ = -1;
    int bpp_ = 4;
    uint16_t fbw_ = 0, fbh_ = 0;
    PixelFormat fmt_ = {};
    std::vector<uint32_t> fb_;
    Display *disp_ = nullptr;

    bool need_update_ = false;

    z_stream zstream_[4] = {};

    // Recv buffer for non-blocking I/O
    std::vector<uint8_t> rbuf_;
    size_t rpos_ = 0;

    //
    std::atomic<bool> breakLoop = false;
};
