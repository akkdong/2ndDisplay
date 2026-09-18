// VncEventQueue.c
//

#include "VncEventQueue.h"


//
//
//

VncEventQueue* VNC_CreateQueue() 
{
    VncEventQueue *q = (VncEventQueue*)malloc(sizeof(VncEventQueue));
    if (!q) 
        return NULL;

    VNC_InitQueue(q);

    return q;
}

void VNC_DestroyQueue(VncEventQueue *q) 
{
    if (!q) 
        return;

    VNC_DeinitQueue(q);

    free(q);
}

void VNC_InitQueue(VncEventQueue* q)
{
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    q->mutex = SDL_CreateMutex();
    #if USE_SDL_CONDITION
    q->cond = SDL_CreateCond();
    #endif
}

void VNC_DeinitQueue(VncEventQueue* q)
{
    SDL_DestroyMutex(q->mutex);
    #if USE_SDL_CONDITION
    SDL_DestroyCond(q->cond);
    #endif
}


bool VNC_PushEvent(VncEventQueue *q, VncEvent *ev) 
{
    if (!q) 
        return false;

    SDL_LockMutex(q->mutex);

    // Check if the queue is full.
    if (q->count >= VNC_QUEUE_MAX) 
    {
        SDL_UnlockMutex(q->mutex);
        return false; // Queue Full
    }

    // Data Insertion (Circular Queue Structure)
    q->data[q->tail] = *ev;
    q->tail = (q->tail + 1) % VNC_QUEUE_MAX;
    q->count++;

    #if USE_SDL_CONDITION
    // Wake up any waiting Pop threads
    SDL_CondSignal(q->cond);
    #endif

    SDL_UnlockMutex(q->mutex);
    return true;
}

bool VNC_PollEvent(VncEventQueue *q, VncEvent *out_ev)
{
    if (!q || !out_ev) 
        return false;

    SDL_LockMutex(q->mutex);

    // Return immediately if there are no events to retrieve.
    if (q->count == 0) 
    {
        SDL_UnlockMutex(q->mutex);
        return false;
    }

    // Data Extraction
    *out_ev = q->data[q->head];
    q->head = (q->head + 1) % VNC_QUEUE_MAX;
    q->count--;

    SDL_UnlockMutex(q->mutex);
    return true;
}
