// WinsockWorker.h
//

#pragma once

#include <stdint.h>

typedef struct WinsockWrapperSocket WinsockWrapperSocket_t;


WinsockWrapperSocket_t* Winsock_connect_server(const char* addr, uint16_t port, int timeout);

WinsockWrapperSocket_t* Winsock_socket(int af, int type, int protocol);
int Winsock_connect(WinsockWrapperSocket_t* s, const char* name, int namelen);
void Winsock_closesocket(WinsockWrapperSocket_t* s);

int32_t Winsock_setsockopt(WinsockWrapperSocket_t* pxSocket, int32_t lLevel, int32_t lOptionName, const void* pvOptionValue, size_t xOptionLength);

int32_t Winsock_send(WinsockWrapperSocket_t* pxSocket, const void* pvBuffer, size_t xTotalLength, uint32_t ulFlags);
int32_t Winsock_recv(WinsockWrapperSocket_t* pxSocket, void* pvBuffer, size_t xBufferLength, uint32_t ulFlags);

