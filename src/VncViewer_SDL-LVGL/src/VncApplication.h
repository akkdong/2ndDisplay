// VncApplication.h
//

#pragma once

#include "SDL2/SDL.h"
#include <signal.h>
#include "VncEventQueue.h"


//
//
//

class VncDisplay;
class VncScreen;
class VncClient;


//
//
//

class VncApplication
{
public:
    VncApplication();
    ~VncApplication();

public:    
    //
    bool begin();
    void run();
    void loop();

    int parseCmdLine(int argc, char* argv[]);
    bool aborted();

    void updateFrame(uint32_t* fb, size_t size);

    //
    static void postUserEvent(Uint32 type, Uint32 code, void* data1, void* data2);

private:
    //
    void processClientEvent(Uint32 code, void* data1, void* data2);
    void processScreenEvent(Uint32 code, void* data1, void* data2);

    //
    void setupSignal();
    static void signal_handler(int no);

private:
    static volatile sig_atomic_t m_interrupted;
    static VncEventQueue m_evtQueue;

    VncDisplay* m_disp;
    VncScreen* m_scrn;
    VncClient* m_client;

    SDL_mutex* m_mutex;
};
