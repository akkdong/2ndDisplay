#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <format>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <csignal>

#include "VncViewer.h"
#include "Display.h"

#include <jpeglib.h>
#include "d3des.h"



// ============================================================
// Network helpers
// ============================================================

static int set_nonblock(int fd, bool enable) 
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    if (enable)
        flags |= O_NONBLOCK;
    else
        flags &= ~O_NONBLOCK;
    return fcntl(fd, F_SETFL, flags);
}

static bool read_exact(int fd, uint8_t *buf, size_t len, int timeout_ms = -1) 
{
    while (len > 0) {
        ssize_t n = recv(fd, buf, len, 0);
        if (n > 0) {
            buf += n; len -= n;
            continue;
        }
        if (n == 0) return false;
        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            struct pollfd pfd = { fd, POLLIN, 0 };
            int ret = poll(&pfd, 1, timeout_ms);
            if (ret <= 0) return false;
            continue;
        }
        return false;
    }
    return true;
}

static bool write_exact(int fd, const uint8_t *buf, size_t len) 
{
    while (len > 0) {
        ssize_t n = send(fd, buf, len, 0);
        if (n <= 0) {
            if (errno == EINTR) continue;
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

static void vnc_encrypt_challenge(const uint8_t *challenge, const std::string &password, uint8_t *response) 
{
    unsigned char key[8] = {};
    size_t len = password.size();
    if (len > 8) len = 8;
    for (size_t i = 0; i < len; ++i)
        key[i] = bit_rev[static_cast<unsigned char>(password[i])];

    rfbDesKey(key, EN0);
    rfbDes(const_cast<unsigned char *>(challenge), response);
    rfbDes(const_cast<unsigned char *>(challenge + 8), response + 8);
}



// ============================================================
// Tight decoder
// ============================================================

static inline uint32_t read_pixel(const uint8_t *buf, int bpp) 
{
    if (bpp == 1) return buf[0];
    if (bpp == 2) return (uint32_t(buf[1]) << 8) | buf[0];
    if (bpp == 3) return (uint32_t(buf[2]) << 16) | (uint32_t(buf[1]) << 8) | buf[0];
    return (uint32_t(buf[3]) << 24) | (uint32_t(buf[2]) << 16) | (uint32_t(buf[1]) << 8) | buf[0];
}

static inline void write_pixel(uint8_t *buf, uint32_t pixel, int bpp) 
{
    if (bpp == 1) { buf[0] = pixel & 0xFF; }
    else if (bpp == 2) { buf[0] = pixel & 0xFF; buf[1] = (pixel >> 8) & 0xFF; }
    else if (bpp == 3) { buf[0] = pixel & 0xFF; buf[1] = (pixel >> 8) & 0xFF; buf[2] = (pixel >> 16) & 0xFF; }
    else { buf[0] = pixel & 0xFF; buf[1] = (pixel >> 8) & 0xFF; buf[2] = (pixel >> 16) & 0xFF; buf[3] = (pixel >> 24) & 0xFF; }
}

static uint32_t pixel_to_32bit(const uint8_t *p, const PixelFormat &fmt) 
{
    uint32_t raw = read_pixel(p, fmt.bpp / 8);
    if (!fmt.true_color) return raw;
    uint8_t r = (raw >> fmt.red_shift) & fmt.red_max;
    uint8_t g = (raw >> fmt.green_shift) & fmt.green_max;
    uint8_t b = (raw >> fmt.blue_shift) & fmt.blue_max;
    if (fmt.red_max != 0xFF) r = (r * 255 + fmt.red_max / 2) / fmt.red_max;
    if (fmt.green_max != 0xFF) g = (g * 255 + fmt.green_max / 2) / fmt.green_max;
    if (fmt.blue_max != 0xFF) b = (b * 255 + fmt.blue_max / 2) / fmt.blue_max;
    return 0xFF000000 | (uint32_t(r) << 16) | (uint32_t(g) << 8) | b;
}

// Apply Tight Gradient filter (type 1) to a full buffer of pixel data
static void tight_filter_gradient(uint8_t *data, int w, int h, int bpp) 
{
    int stride = w * bpp;
    for (int y = 0; y < h; ++y) {
        uint8_t *row = data + y * stride;
        uint8_t *prev = (y > 0) ? data + (y - 1) * stride : nullptr;
        for (int x = 0; x < w; ++x) {
            for (int c = 0; c < bpp; ++c) {
                uint8_t *p = &row[x * bpp + c];
                int pred;
                if (x == 0 && prev == nullptr)
                    pred = 0;
                else if (x == 0)
                    pred = prev[c];
                else if (prev == nullptr)
                    pred = row[(x - 1) * bpp + c];
                else
                    pred = row[(x - 1) * bpp + c] + prev[x * bpp + c] - prev[(x - 1) * bpp + c];
                *p = static_cast<uint8_t>(*p + pred);
            }
        }
    }
}

static bool zlib_decompress(z_stream &zs, const uint8_t *in, size_t inlen, uint8_t *out, size_t outlen) 
{
    zs.next_in = const_cast<uint8_t *>(in);
    zs.avail_in = inlen;
    zs.next_out = out;
    zs.avail_out = outlen;

    int ret = inflate(&zs, Z_SYNC_FLUSH);
    if (ret != Z_OK && ret != Z_STREAM_END) {
        std::cerr << "inflate error: " << ret << std::endl;
        return false;
    }
    return true;
}



//
//
//

VncClient::VncClient(Display* disp)
{
    //
    disp_ = disp;
}

VncClient::~VncClient() 
{
    destroy_zstreams();

    if (fd_ != -1) 
        close(fd_);
}


bool VncClient::connect(const std::string &host, int port) 
{
    fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0) return false;

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
        struct hostent *he = gethostbyname(host.c_str());
        if (!he) return false;
        memcpy(&addr.sin_addr, he->h_addr, he->h_length);
    }
    if (::connect(fd_, (sockaddr *)&addr, sizeof(addr)) < 0) return false;
    return set_nonblock(fd_, true) == 0;
}

bool VncClient::handshake(const std::string &password) 
{
    // 1. Protocol version
    uint8_t ver[12];
    if (!read_exact(fd_, ver, 12)) return false;
    std::cout << "Server: " << std::string((char *)ver, 11) << std::endl;

    const char *my_ver = "RFB 003.008\n";
    if (!write_exact(fd_, (const uint8_t *)my_ver, 12)) return false;

    // 2. Security
    uint8_t nsec;
    if (!read_exact(fd_, &nsec, 1) || nsec == 0) return false;

    std::vector<uint8_t> types(nsec);
    if (!read_exact(fd_, types.data(), nsec)) return false;

    std::cout << "Security types:";
    for (auto t : types) std::cout << " " << int(t);
    std::cout << std::endl;

    uint8_t chosen = 0;
    bool have_none = false, have_vncauth = false;
    for (auto t : types) {
        if (t == 1) have_none = true;
        if (t == 2) have_vncauth = true;
    }

    if (have_vncauth && !password.empty()) {
        chosen = 2;
    } else if (have_none) {
        chosen = 1;
    } else {
        std::cerr << "No supported security type" << std::endl;
        return false;
    }

    if (!write_exact(fd_, &chosen, 1)) return false;

    if (chosen == 2) {
        uint8_t challenge[16];
        if (!read_exact(fd_, challenge, 16)) return false;
        uint8_t response[16];
        vnc_encrypt_challenge(challenge, password, response);
        if (!write_exact(fd_, response, 16)) return false;
    }

    // Security result (4 bytes for RFB 3.8+)
    if (chosen == 2 || chosen == 1) {
        uint32_t result;
        if (!read_exact(fd_, (uint8_t *)&result, 4)) return false;
        if (ntohl(result) != 0) {
            std::cerr << "Security failed, result=" << ntohl(result) << std::endl;
            return false;
        }
        std::cout << "Security OK" << std::endl;
    }

    // 3. ClientInit
    uint8_t shared = 1;
    if (!write_exact(fd_, &shared, 1)) return false;

    // 4. ServerInit
    ServerInit si;
    if (!read_exact(fd_, (uint8_t *)&si, sizeof(si))) return false;
    fbw_ = ntohs(si.fb_width);
    fbh_ = ntohs(si.fb_height);
    fmt_ = si.fmt;
    fmt_.red_max = ntohs(fmt_.red_max);
    fmt_.green_max = ntohs(fmt_.green_max);
    fmt_.blue_max = ntohs(fmt_.blue_max);
    uint32_t nlen = ntohl(si.name_len);

    std::cout << "Framebuffer: " << fbw_ << "x" << fbh_;
    std::cout << " fmt: bpp=" << int(fmt_.bpp) << " depth=" << int(fmt_.depth);
    std::cout << " big_endian=" << int(fmt_.big_endian) << " true_color=" << int(fmt_.true_color);
    std::cout << " shift=" << int(fmt_.red_shift) << "," << int(fmt_.green_shift) << "," << int(fmt_.blue_shift);
    std::cout << " max=" << fmt_.red_max << "," << fmt_.green_max << "," << fmt_.blue_max;
    std::cout << std::endl;

    if (nlen > 0) {
        if (nlen > (1 << 20)) {
            std::cerr << "Desktop name too long: " << nlen << std::endl;
            return false;
        }
        std::vector<char> name(nlen + 1, 0);
        if (!read_exact(fd_, (uint8_t *)name.data(), nlen)) return false;
        std::cout << "Desktop: " << name.data() << std::endl;
    }

    const size_t fb_pixels = size_t(fbw_) * fbh_;
    if (fb_pixels > 0x10000000) {
        std::cerr << "Framebuffer too large: " << fbw_ << "x" << fbh_ << std::endl;
        return false;
    }
    fb_.resize(fb_pixels, 0xFF000000);
    bpp_ = fmt_.bpp / 8;
    if (bpp_ < 1) bpp_ = 1;
    if (bpp_ > 4) bpp_ = 4;

    // Resize the LVGL display/window to the VNC framebuffer size and
    // (re)build the overlay UI on top of the VNC canvas
    //if (disp_) lv_display_set_resolution((lv_display_t *)(*disp_), fbw_, fbh_);
    //Display::create_vnc_ui(fb_.data(), fbw_, fbh_);

    init_zstreams();
    return true;
}

void VncClient::init_zstreams() 
{
    for (int i = 0; i < 4; ++i)
        inflateInit(&zstream_[i]);
}

void VncClient::reset_zstream(int id) 
{
    inflateReset(&zstream_[id]);
}

void VncClient::destroy_zstreams() 
{
    for (int i = 0; i < 4; ++i)
        inflateEnd(&zstream_[i]);
}

// Drop already-consumed bytes so the recv buffer cannot grow unboundedly
void VncClient::compact_buf() 
{
    if (rpos_ == 0) return;
    if (rpos_ == rbuf_.size() || rpos_ >= (1 << 16)) {
        rbuf_.erase(rbuf_.begin(), rbuf_.begin() + rpos_);
        rpos_ = 0;
    }
}

// Read more data from socket into buffer (non-blocking)
// Returns false on disconnect, true otherwise (including EAGAIN)
bool VncClient::fill_buf() 
{
    compact_buf();
    uint8_t tmp[8192];
    ssize_t n = recv(fd_, tmp, sizeof(tmp), 0);
    if (n > 0) {
        rbuf_.insert(rbuf_.end(), tmp, tmp + n);
        return true;
    }
    if (n == 0) return false;
    if (errno == EINTR) return true;
    if (errno == EAGAIN || errno == EWOULDBLOCK) return true;
    return false;
}

// Wait until buffer has at least 'needed' bytes
// Returns false only on disconnect/error
bool VncClient::wait_buf(size_t needed) 
{
    while (rbuf_.size() - rpos_ < needed) {
        if (breakLoop) return false;
        if (!fill_buf()) return false;
        if (rbuf_.size() - rpos_ >= needed) return true;
        struct pollfd pfd = { fd_, POLLIN, 0 };
        int ret = poll(&pfd, 1, 16);
        if (ret < 0) { if (breakLoop) return false; continue; }
        if (ret == 0) {
            // No data yet; re-check breakLoop while waiting
            continue;
        }
    }
    return true;
}

// Convenience: read one byte
bool VncClient::read_u8(uint8_t &v) 
{
    if (!wait_buf(1)) return false;
    v = rbuf_[rpos_++];
    return true;
}

// Convenience: read into buffer
bool VncClient::read_bytes(uint8_t *dst, size_t n) 
{
    if (!wait_buf(n)) return false;
    memcpy(dst, &rbuf_[rpos_], n);
    rpos_ += n;
    return true;
}

// Read compact length from buffer
bool VncClient::read_clen(size_t &len) 
{
    uint8_t b;
    if (!read_u8(b)) return false;
    len = b & 0x7F;
    if (b & 0x80) {
        if (!read_u8(b)) return false;
        len |= (size_t(b & 0x7F) << 7);
        if (b & 0x80) {
            if (!read_u8(b)) return false;
            len |= (size_t(b) << 14);
        }
    }
    return true;
}

// Validate that a rectangle fits within the framebuffer
bool VncClient::rect_ok(int rx, int ry, int rw, int rh) const 
{
    return rx >= 0 && ry >= 0 && rw > 0 && rh > 0 &&
            size_t(rx) + rw <= size_t(fbw_) && size_t(ry) + rh <= size_t(fbh_);
}

bool VncClient::tight_decode(int rx, int ry, int rw, int rh, int bpp) 
{
    auto &fb = fb_;
    int fbw = fbw_;

    if (!rect_ok(rx, ry, rw, rh)) {
        std::cerr << "Tight rect out of bounds" << std::endl;
        return false;
    }

    uint8_t ctrl;
    if (!read_u8(ctrl)) return false;

    for (int i = 0; i < 4; ++i) {
        if (ctrl & (0x10 << i))
            inflateReset(&zstream_[i]);
    }

    uint8_t comp_type = ctrl & 0x0F;

    if (comp_type == 0x08) {
        uint8_t fill_buf[4];
        if (!read_bytes(fill_buf, bpp)) return false;
        uint32_t color = pixel_to_32bit(fill_buf, fmt_);
        for (int y = 0; y < rh; ++y)
            for (int x = 0; x < rw; ++x)
                fb[(ry + y) * fbw + (rx + x)] = color;
        return true;
    }

    if (comp_type == 0x09) {
        size_t jpeg_len;
        if (!read_clen(jpeg_len)) return false;
        std::vector<uint8_t> jpeg_data(jpeg_len);
        if (!read_bytes(jpeg_data.data(), jpeg_len)) return false;

        struct jpeg_decompress_struct cinfo;
        struct jpeg_error_mgr jerr;
        cinfo.err = jpeg_std_error(&jerr);
        jpeg_create_decompress(&cinfo);
        jpeg_mem_src(&cinfo, jpeg_data.data(), jpeg_len);
        if (jpeg_read_header(&cinfo, TRUE) == JPEG_HEADER_OK) {
            cinfo.out_color_space = JCS_EXT_BGRA;
            jpeg_start_decompress(&cinfo);
            if (cinfo.output_width > (JDIMENSION)rw || cinfo.output_height > (JDIMENSION)rh) {
                std::cerr << "Tight JPEG size mismatch" << std::endl;
                jpeg_abort_decompress(&cinfo);
                jpeg_destroy_decompress(&cinfo);
                return false;
            }
            for (int y = 0; y < rh && y < (int)cinfo.output_height; ++y) {
                uint8_t *row = reinterpret_cast<uint8_t *>(&fb[(ry + y) * fbw + rx]);
                jpeg_read_scanlines(&cinfo, &row, 1);
                for (int x = cinfo.output_width; x < rw; ++x)
                    fb[(ry + y) * fbw + rx + x] = 0xFF000000;
            }
            jpeg_finish_decompress(&cinfo);
        }
        jpeg_destroy_decompress(&cinfo);
        return true;
    }

    bool has_palette = (comp_type & 0x04) != 0;
    int stream_idx = (comp_type & 0x03) - 1;
    bool is_compressed = (comp_type & 0x03) != 0;
    std::vector<uint32_t> palette;
    int palette_size = 0;

    if (has_palette) {
        uint8_t psize;
        if (!read_u8(psize)) return false;
        palette_size = psize + 1;
        palette.resize(palette_size);
        uint8_t pal_buf[4 * 256];
        if (!read_bytes(pal_buf, palette_size * bpp)) return false;
        for (int i = 0; i < palette_size; ++i)
            palette[i] = pixel_to_32bit(pal_buf + i * bpp, fmt_);
    }

    std::vector<uint8_t> raw_data;
    size_t pixel_count = size_t(rw) * rh;

    if (is_compressed) {
        size_t zlen;
        if (!read_clen(zlen)) return false;
        std::vector<uint8_t> compressed(zlen);
        if (!read_bytes(compressed.data(), zlen)) return false;

        size_t uncomp_size = has_palette ? pixel_count : (pixel_count * bpp + 1);
        raw_data.resize(uncomp_size);
        if (!zlib_decompress(zstream_[stream_idx], compressed.data(), zlen, raw_data.data(), uncomp_size))
            return false;
    }

    if (has_palette) {
        if (!is_compressed) {
            raw_data.resize(pixel_count);
            if (!read_bytes(raw_data.data(), pixel_count)) return false;
        }
        for (size_t i = 0; i < pixel_count; ++i) {
            uint8_t idx = raw_data[i];
            uint32_t color = (idx < palette.size()) ? palette[idx] : 0xFF000000;
            int x = rx + (i % rw);
            int y = ry + (i / rw);
            fb[y * fbw + x] = color;
        }
    } else {
        if (is_compressed) {
            uint8_t filter = raw_data[0];
            uint8_t *pixel_data = raw_data.data() + 1;
            if (filter == 1)
                tight_filter_gradient(pixel_data, rw, rh, bpp);
            else if (filter != 0) {
                std::cerr << "Unsupported Tight filter: " << int(filter) << std::endl;
                return false;
            }
            for (int y = 0; y < rh; ++y)
                for (int x = 0; x < rw; ++x) {
                    fb[(ry + y) * fbw + (rx + x)] = pixel_to_32bit(pixel_data + (y * rw + x) * bpp, fmt_);
                }
        } else {
            bool has_gradient = (ctrl & 0x20) != 0;
            if (has_gradient) {
                uint8_t filter;
                if (!read_u8(filter)) {
                    std::cerr << "TIGHT_GRADIENT: read filter failed" << std::endl;
                    return false;
                }
                if (filter == 0x01) {
                    size_t total = size_t(rw) * rh * bpp;
                    std::vector<uint8_t> raw(total);
                    if (!read_bytes(raw.data(), total)) {
                        std::cerr << "TIGHT_GRADIENT: read pixels failed" << std::endl;
                        return false;
                    }
                    tight_filter_gradient(raw.data(), rw, rh, bpp);
                    for (int y = 0; y < rh; ++y)
                        for (int x = 0; x < rw; ++x)
                            fb[(ry + y) * fbw + (rx + x)] = pixel_to_32bit(raw.data() + (y * rw + x) * bpp, fmt_);
                } else if (filter != 0x00) {
                    std::cerr << "TIGHT_GRADIENT: unknown filter " << int(filter) << std::endl;
                    return false;
                } else {
                    size_t total = size_t(rw) * rh * bpp;
                    std::vector<uint8_t> raw(total);
                    if (!read_bytes(raw.data(), total)) return false;
                    for (int y = 0; y < rh; ++y)
                        for (int x = 0; x < rw; ++x)
                            fb[(ry + y) * fbw + (rx + x)] = pixel_to_32bit(raw.data() + (y * rw + x) * bpp, fmt_);
                }
            } else {
                std::vector<uint8_t> row_buf(rw * bpp);
                for (int y = 0; y < rh; ++y) {
                    if (!read_bytes(row_buf.data(), rw * bpp)) return false;
                    const uint8_t *src = row_buf.data();
                    for (int x = 0; x < rw; ++x) {
                        fb[(ry + y) * fbw + (rx + x)] = pixel_to_32bit(src, fmt_);
                        src += bpp;
                    }
                }
            }
        }
    }

    return true;
}

// ============================================================
// Pointer input to the VNC server
// ============================================================
void VncClient::send_pointer(int x, int y, uint8_t mask) 
{
    if (fd_ == -1) return;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= fbw_) x = fbw_ - 1;
    if (y >= fbh_) y = fbh_ - 1;
    uint8_t ptr[6] = { 5, mask,
                       static_cast<uint8_t>((x >> 8) & 0xFF), static_cast<uint8_t>(x & 0xFF),
                       static_cast<uint8_t>((y >> 8) & 0xFF), static_cast<uint8_t>(y & 0xFF) };
    write_exact(fd_, ptr, 6);
}

// ============================================================
// Main event loop
// ============================================================

bool VncClient::isOk()
{
    if (fd_ == -1) return false;
    if (breakLoop) return false;
    if (disp_ && !disp_->m_clientRunning) return false;
    return true;
}

void VncClient::loop()
{
    // NOTE: LVGL is driven solely by the main thread (Display::loop()).
    // This worker must not call lv_timer_handler().

    // Send pending FB request
    if (need_update_) {
        uint8_t fb_req_inc[] = { 3, 1, 0,0, 0,0, 0,0, 0,0 };
        fb_req_inc[6] = (fbw_ >> 8) & 0xFF;
        fb_req_inc[7] = fbw_ & 0xFF;
        fb_req_inc[8] = (fbh_ >> 8) & 0xFF;
        fb_req_inc[9] = fbh_ & 0xFF;
        if (!write_exact(fd_, fb_req_inc, 10))
            breakLoop = true;
        need_update_ = false;
    }

    // Read message type (wait_buf handles polling + LVGL processing internally)
    uint8_t msg_type;
    if (!read_u8(msg_type)) {
        if (!breakLoop) std::cerr << "Server disconnected" << std::endl;
        breakLoop = true;
        return;
    }

    if (msg_type == 0) {
        uint8_t pad;
        uint16_t nrects;
        bool msg_ok = true;
        if (!read_u8(pad)) { msg_ok = false; }
        if (msg_ok) {
            uint8_t nrbuf[2];
            if (!read_bytes(nrbuf, 2)) { msg_ok = false; }
            if (msg_ok) {
                nrects = (uint16_t(nrbuf[0]) << 8) | nrbuf[1];
                for (int i = 0; i < nrects && msg_ok; ++i) {
                    uint8_t rect_hdr[12];
                    if (!read_bytes(rect_hdr, 12)) { msg_ok = false; break; }

                    uint16_t rx = (uint16_t(rect_hdr[0]) << 8) | rect_hdr[1];
                    uint16_t ry = (uint16_t(rect_hdr[2]) << 8) | rect_hdr[3];
                    uint16_t rw = (uint16_t(rect_hdr[4]) << 8) | rect_hdr[5];
                    uint16_t rh = (uint16_t(rect_hdr[6]) << 8) | rect_hdr[7];
                    int32_t encoding = (int32_t(rect_hdr[8]) << 24) | (int32_t(rect_hdr[9]) << 16) |
                                       (int32_t(rect_hdr[10]) << 8) | rect_hdr[11];
                    if (!rect_ok(rx, ry, rw, rh)) {
                        std::cerr << "Rect out of bounds: " << rx << "," << ry << " "
                                  << rw << "x" << rh << " (fb " << fbw_ << "x" << fbh_ << ")" << std::endl;
                        msg_ok = false;
                        break;
                    }
                    if (encoding == 0) {
                        size_t pix_bytes = size_t(rw) * rh * bpp_;
                        std::vector<uint8_t> pixels(pix_bytes);
                        if (!read_bytes(pixels.data(), pix_bytes)) { msg_ok = false; break; }
                        const uint8_t *src = pixels.data();
                        for (int y = 0; y < rh; ++y)
                            for (int x = 0; x < rw; ++x) {
                                fb_[(ry + y) * fbw_ + (rx + x)] = pixel_to_32bit(src, fmt_);
                                src += bpp_;
                            }
                    } else if (encoding == 1) {
                        uint8_t copy_hdr[4];
                        if (!read_bytes(copy_hdr, 4)) { msg_ok = false; break; }
                        int src_x = (int(copy_hdr[0]) << 8) | copy_hdr[1];
                        int src_y = (int(copy_hdr[2]) << 8) | copy_hdr[3];
                        if (!rect_ok(src_x, src_y, rw, rh)) {
                            std::cerr << "CopyRect source out of bounds" << std::endl;
                            msg_ok = false;
                            break;
                        }
                        for (int y = 0; y < rh; ++y)
                            for (int x = 0; x < rw; ++x)
                                fb_[(ry + y) * fbw_ + (rx + x)] = fb_[(src_y + y) * fbw_ + (src_x + x)];
                    } else if (encoding == 7) {
                        if (!tight_decode(rx, ry, rw, rh, bpp_)) {
                            msg_ok = false;
                            break;
                        }
                    } else {
                        std::cerr << "Unsupported encoding: " << encoding << std::endl;
                        msg_ok = false;
                        break;
                    }
                }
            }
        }
        if (!msg_ok) {
            breakLoop = true;
            return;
        }

        // Publish the decoded frame to the Display-owned canvas buffer and
        // signal the main thread (Display::loop drains the event queue)
        if (disp_) {
            disp_->publishFrame(fb_.data(), fb_.size());
            disp_->pushEvent(Display::EVT_UPDATE_FRAMEBUFFER);
        }
        need_update_ = true;

    } else if (msg_type == 1) {
        uint8_t pad;
        uint8_t fcbuf[2], ncbuf[2];
        if (!read_u8(pad)) { breakLoop = true; return; }
        if (!read_bytes(fcbuf, 2)) { breakLoop = true; return; }
        if (!read_bytes(ncbuf, 2)) { breakLoop = true; return; }
        uint16_t ncolors = (uint16_t(ncbuf[0]) << 8) | ncbuf[1];
        std::vector<uint8_t> cmap(ncolors * 6);
        if (!read_bytes(cmap.data(), ncolors * 6)) { breakLoop = true; return; }

    } else if (msg_type == 2) {
        // Bell
    } else if (msg_type == 3) {
        uint8_t pad[3];
        if (!read_bytes(pad, 3)) { breakLoop = true; return; }
        uint8_t clbuf[4];
        if (!read_bytes(clbuf, 4)) { breakLoop = true; return; }
        uint32_t clen = (uint32_t(clbuf[0]) << 24) | (uint32_t(clbuf[1]) << 16) |
                        (uint32_t(clbuf[2]) << 8) | clbuf[3];
        if (clen > (1 << 20)) {
            std::cerr << "Oversized server cut-text: " << clen << " bytes" << std::endl;
            breakLoop = true;
            return;
        }
        std::vector<uint8_t> ctext(clen);
        if (!read_bytes(ctext.data(), clen)) { breakLoop = true; return; }
    } else {
        std::cerr << "Unknown message type: " << int(msg_type) << std::endl;
        breakLoop = true;
        return;
    }
}

void VncClient::run()
{
    // Send SetEncodings
    uint8_t setenc[] = {
        2,       // message type
        0,       // padding
        0, 3,    // number of encodings
        0, 0, 0, 0,  // Raw (desktop fallback)
        0, 0, 0, 1,  // CopyRect
        0, 0, 0, 7   // Tight (h/w decode on embedded target)
    };
    if (!write_exact(fd_, setenc, sizeof(setenc))) {
        std::cerr << "Failed to send SetEncodings" << std::endl;
        breakLoop = true;
        return;
    }

    // Send initial full FB update request
    need_update_ = true;
    uint8_t fb_req_full[] = { 3, 0, 0,0, 0,0, 0,0, 0,0 };
    fb_req_full[6] = (fbw_ >> 8) & 0xFF;
    fb_req_full[7] = fbw_ & 0xFF;
    fb_req_full[8] = (fbh_ >> 8) & 0xFF;
    fb_req_full[9] = fbh_ & 0xFF;
    if (!write_exact(fd_, fb_req_full, 10)) {
        std::cerr << "Failed FB request" << std::endl;
        breakLoop = true;
        return;
    }

    while (isOk())
        loop();
}

