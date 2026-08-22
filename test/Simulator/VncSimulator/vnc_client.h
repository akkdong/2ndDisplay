// vnc_client.h
//

#pragma once

#include <stdbool.h>
#include <zlib.h>

#include "extern.h"
#include "vnc_types.h"

#if defined(_SIMULATOR)
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"

#define socket          FreeRTOS_socket
#define bind            FreeRTOS_bind
#define setscokopt      FreeRTOS_setsockopt
#define closesocket     FreeRTOS_closesocket
#define sendto          FreeRTOS_sendto
#define recvfrom        FreeRTOS_recvfrom

#define connect         FreeRTOS_connect
#define listen          FreeRTOS_listen
#define accept          FreeRTOS_accept
#define send            FreeRTOS_send
#define recv            FreeRTOS_recv
#define shutdown        FreeRTOS_shutdown

#define inet_ntoa       FreeRTOS_inet_ntoa
#define inet_addr       FreeRTOS_inet_addr
#define inet_pton       FreeRTOS_inet_pton
#define inet_ntop       FreeRTOS_inet_ntop 
#define gethostbyname   FreeRTOS_gethostbyname

#define htons           FreeRTOS_htons
#define htonl           FreeRTOS_htonl
#define ntohs           FreeRTOS_ntohs
#define ntohl           FreeRTOS_ntohl

#define FD_SET          FreeRTOS_FD_SET
#define FD_CLR          FreeRTOS_FD_CLR
#define FD_ISSET        FreeRTOS_FD_ISSET

#define AF_INET         FREERTOS_AF_INET
#define SOCK_STREAM     FREERTOS_SOCK_STREAM
#define IPPROTO_TCP     FREERTOS_IPPROTO_TCP

#define sockaddr        struct freertos_sockaddr

#define SOCKET          Socket_t
#define INVALID_SOCKET  FREERTOS_INVALID_SOCKET

#if defined(EINTR)
#undef EINTR
#endif

#define EINTR           (-pdFREERTOS_ERRNO_EINTR)

//
//

#define _malloc          pvPortMalloc
#define _free            vPortFree

#else

#define sockaddr        struct sockaddr_in

#define SOCKET          int
#define INVALID_SOCKET  (-1)

#endif


BEGIN_EXTERN_C();


//
//
//

#if defined(_WIN32)
#pragma pack( push, 1 )
#endif

struct PixelFormat_s {
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
} 
#if defined(_WIN32)
;
#else
__attribute__((packed));
#endif

struct ServerInit_s {
    uint16_t fb_width;
    uint16_t fb_height;
    struct PixelFormat_s fmt;
    uint32_t name_len;
}
#if defined(_WIN32)
;
#else
__attribute__((packed));
#endif

#if defined(_WIN32)
#pragma pack( pop )
#endif

typedef struct PixelFormat_s PixelFormat;
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

bool vnc_client_isOk(vnc_client_t* client);
/*
void vnc_client_send_pointer(vnc_client_t* client, int x, int y, uint8_t mask);
*/


END_EXTERN_C();
