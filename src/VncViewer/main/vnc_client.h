// vnc_client.h
//

#pragma once

#include <stdbool.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "miniz/miniz.h"

#include "extern.h"
#include "vnc_types.h"




BEGIN_EXTERN_C();


//
//
//

#define INVALID_SOCKET      (int)(-1)

typedef int SOCKET;



//
//
//

#if defined(_WIN32)
#pragma pack( push, 1 )
#endif

struct ServerInit_s {
    uint16_t fb_width;
    uint16_t fb_height;
    PixelFormat fmt;
    uint32_t name_len;
}
#if defined(_WIN32)
;
#pragma pack( pop )
#else
__attribute__((packed));
#endif

typedef struct ServerInit_s ServerInit;


//
//
//

struct vnc_client_s
{
    //
	vnc_app_t* app;
    vnc_screen_t* scrn;


    //
    SOCKET fd;
    int bpp;
    uint16_t fbw, fbh;
    PixelFormat fmt;
    uint32_t* fb;
    size_t fb_pixels;

    bool need_update;

    z_stream zstream[4];

    // Recv buffer for non-blocking I/O
    uint8_t* rbuf_ptr;
    uint32_t rbuf_len;
    //size_t rpos;
    uint8_t* temp_ptr;
    uint32_t temp_len;

    //
    volatile bool break_loop;
};



//
//
//

vnc_client_t* vnc_client_start(vnc_app_t* app);


//
//
//

bool vnc_client_connect(vnc_client_t* client, const char* addr, uint16_t port, int timeout);
bool vnc_client_handshake(vnc_client_t* client, const char* pass);
void vnc_client_loop(vnc_client_t* client);
void vnc_client_run(vnc_client_t* client);
void vnc_client_stop(vnc_client_t* client);
void vnc_client_close(vnc_client_t* client);

bool vnc_client_isOk(vnc_client_t* client);
/*
void vnc_client_send_pointer(vnc_client_t* client, int x, int y, uint8_t mask);
*/


END_EXTERN_C();
