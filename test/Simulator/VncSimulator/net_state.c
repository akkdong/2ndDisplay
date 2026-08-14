// net_state.c
//

#include "net_state.h"
#include "FreeRTOS.h"
#include "queue.h"


///////////////////////////////////////////////////////////////////
//

typedef struct 
{
    NetEventId_t id;
    uint32_t state;
} NetEventMsg_t;


static QueueHandle_t s_net_event_queue = NULL;



///////////////////////////////////////////////////////////////////
//

void NetEvent_Init()
{
    if (!s_net_event_queue)
        s_net_event_queue = xQueueCreate(10, sizeof(NetEventMsg_t));
}

bool NetEvent_Send(NetEventId_t id, uint32_t state)
{
    if (!s_net_event_queue)
        return false;

    NetEventMsg_t msg = {
        .id = id,
        .state = state
    };

    if (xQueueSend(s_net_event_queue, &msg, 0) == pdTRUE)
        return true;

    return false;
}

bool NetEvent_Receive(NetEventId_t* id, uint32_t* state)
{
    if (!s_net_event_queue)
        return false;

    NetEventMsg_t msg;
    if (xQueueReceive(s_net_event_queue, &msg, 0) == pdTRUE)
    {
        if (id)
            *id = msg.id;
        if (state)
            *state = msg.state;

        return true;
    }

    return false;
}
