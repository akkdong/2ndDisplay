// VncClient.h
//

#pragma once

#include "SDL2/SDL.h"
#include "lvgl.h"
#include "vncDefines.h"
#include "zlib_inflate.h"

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/socket.h>
    #include <netdb.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <netinet/in.h>
    #include <errno.h>
    
    #define INVALID_SOCKET      (-1)
    #define closesocket(s)      close(s)

    typedef int SOCKET;
#endif



//
//
//

class VncApplication;



//
//
//

class VncClient
{
public:
    VncClient(VncApplication* app);
    ~VncClient();

    enum EventType_e 
    {
        EVENT_CLIENT_STARTED,
        EVENT_CLIENT_UPDATE_FRAME,
        EVENT_CLIENT_FINISHED,
        EVENT_CLIENT_FAILED,
    };

    enum Error_e
    {
        ERROR_NONE,
        ERROR_SOCKET_FAILED,
        ERROR_CONNECT_FAILED,
        ERROR_HANDSHAKE_FAILED,
    };

    enum ClientState_e
    {
        State_Ready,
    };

public:
    bool connectToServer(const char* addr, unsigned short port, const char* pass);
    void disconnect();
    void reset();

protected:
    static int SDLCALL ClientProc(void* data);
    
    bool handshake();
    void run();
    void cleanup();

    bool cond();

private:
    bool readExact(SOCKET fd, uint8_t* buf, size_t len);
    bool read_u8(uint8_t* v) { return readExact(m_sock, v, 1); }
    bool read_bytes(uint8_t* dst, size_t n) { return readExact(m_sock, dst, n); }
    bool read_clen(size_t* len);

    bool writeExact(SOCKET fd, const uint8_t* buf, size_t len);

    void encryptChallenge(const uint8_t* challenge, const char* password, uint8_t* response);

    bool frameBufferUpdateRequest(bool full);
    bool setEncodings();

    void onFrameBufferUpdate();
    void onSetColourMapEntries();
    void onBell();
    void onServerCutText();

    bool checkRectangle(int rx, int ry, int rw, int rh);
    bool decodeRaw(int rx, int ry, int rw, int rh, int bpp);
    bool decodeCopyRect(int rx, int ry, int rw, int rh, int bpp);
    bool decodeTight(int rx, int ry, int rw, int rh, int bpp);
    bool decodeZrle(int rx, int ry, int rw, int rh, int bpp);
    bool decodeJpeg(int rx, int ry, int rw, int rh, int bpp);

    //
    bool zlib_decompress(z_stream* zs, const uint8_t* in, size_t inlen, uint8_t* out, size_t outlen, size_t* decompressed_size);

public:
    static Uint32 EventType;

private:
    //
    VncApplication* m_app;

    //
    char m_addr[32];
    unsigned short m_port;
    char m_pass[64];

    int m_sock;
    int bpp;
    uint16_t fbw, fbh;
    PixelFormat fmt;
    uint32_t* fb;
    size_t fb_pixels;

    // Recv buffer for non-blocking I/O
    uint8_t* rbuf_ptr;
    uint32_t rbuf_len;
    //size_t rpos;
    uint8_t* temp_ptr;
    uint32_t temp_len;    

    bool m_needUpdate;
    bool m_breakLoop;
    int m_errno;

    SDL_Thread* m_thread;
    SDL_mutex* m_mutex;
  
    z_stream m_zstrm[4];
};
