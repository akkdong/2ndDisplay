// net_state.h
//

#pragma once

#include <stdint.h>
#include <stdbool.h>


#ifdef __cplusplus
extern "C"
{
#endif


///////////////////////////////////////////////////////////////////
//

typedef enum {
	EVT_NET_READY = 0,
	EVT_NET_CONNECTED,
	EVT_NET_DISCONNECTED
} NetEventId_t;

void NetEvent_Init();
bool NetEvent_Send(NetEventId_t id, uint32_t state);
bool NetEvent_Receive(NetEventId_t* id, uint32_t* state);


#ifdef __cplusplus
}
#endif
