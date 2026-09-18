// VncEventQueue.h
//

#include <stdint.h>
#include <stdbool.h>
#include "SDL2/SDL.h"


#define USE_SDL_CONDITION   0
#define VNC_QUEUE_MAX       6


//
//
//

typedef struct 
{
    uint32_t type;
    int32_t code;
    void *data1;
    void *data2;
} VncEvent;

typedef struct 
{
    VncEvent data[VNC_QUEUE_MAX];
    int head;
    int tail;
    int count;
    
    SDL_mutex *mutex;
    #if USE_SDL_CONDITION
    SDL_cond *cond;
    #endif
} VncEventQueue;



//
//
//

#if defined(__cplusplus)
extern "C"
{
#endif

//
VncEventQueue* VNC_CreateQueue();
void VNC_DestroyQueue(VncEventQueue *q);

void VNC_InitQueue(VncEventQueue* q);
void VNC_DeinitQueue(VncEventQueue* q);


bool VNC_PushEvent(VncEventQueue *q, VncEvent *ev);
bool VNC_PollEvent(VncEventQueue *q, VncEvent *out_ev);


#if defined(__cplusplus)
}
#endif
