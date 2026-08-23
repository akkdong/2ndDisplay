// VirtualSocket.h
//

#pragma once

#include <stdint.h>


#ifndef AF_INET
#define AF_INET			2
#endif

#ifndef SOCK_STREAM
#define SOCK_STREAM     1
#endif

#ifndef IPPROTO_TCP
#define IPPROTO_TCP		6
#endif


typedef struct VirtualSocket VirtualSocket_t;

struct VirtualSocket_sockaddr
{
	uint32_t addr;
	uint16_t port;
};

enum VirtualSocket_State_e
{
	E_SUSPENDED = 0,
	E_CONNECTED = 1,
	E_DISCONNECTED = 2,
	E_ERROR = 3,
};

typedef enum VirtualSocket_State_e VirtualSocket_State;


//
//
//

VirtualSocket_t* VirtualSocket_socket(int af, int type, int protocol);

int VirtualSocket_connect(VirtualSocket_t* s, struct VirtualSocket_sockaddr* addr, int namelen);
void VirtualSocket_closesocket(VirtualSocket_t* s);

int VirtualSocket_setsockopt(VirtualSocket_t* pxSocket, int lLevel, int lOptionName, const char* pvOptionValue, int xOptionLength);

int VirtualSocket_send(VirtualSocket_t* pxSocket, const void* pvBuffer, int xTotalLength, int ulFlags);
int VirtualSocket_recv(VirtualSocket_t* pxSocket, void* pvBuffer, int xBufferLength, int ulFlags);

unsigned long VirtualSocket_inet_addr(const char* cp);
unsigned long VirtualSocket_htonl(unsigned long hostlong);
unsigned short VirtualSocket_htons(unsigned short hostshort);
unsigned long VirtualSocket_ntohl(unsigned long netlong);
unsigned short VirtualSocket_ntohs(unsigned short netshort);
