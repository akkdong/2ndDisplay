// main.c
//

#include <iostream>
#include <string>
#include <queue>
#include <memory>
#include <csignal>

#include "SDL2/SDL.h"
#include "lvgl.h"

#include "VncApplication.h"
#include "VncDisplay.h"
#include "VncScreen.h"
#include "VncClient.h"


#ifdef _WIN32
    #pragma comment(lib, "ws2_32.lib") 
#endif    





//
//
//

volatile sig_atomic_t VncApplication::m_interrupted = 0;
VncEventQueue VncApplication::m_evtQueue;


VncApplication::VncApplication()
    : m_disp(nullptr)
    , m_scrn(nullptr)
    , m_client(nullptr)
    , m_mutex(nullptr)
{
    //
    setupSignal();

    m_mutex = SDL_CreateMutex();
}

VncApplication::~VncApplication()
{
    if (m_client)
        delete m_client;
    if (m_scrn)
        delete m_scrn;
    if (m_disp)
        delete m_disp;

    if (m_mutex)
        SDL_DestroyMutex(m_mutex);
}


bool VncApplication::begin()
{
    m_disp = new VncDisplay(this);
    m_scrn = new VncScreen(this);
    /*
    m_client = new VncClient(this);
    */

    VNC_InitQueue(&m_evtQueue);

    m_disp->registerInputCallback(VncScreen::pointInputCallback, m_scrn);

    return true;
}

void VncApplication::run()
{
    #if USE_SDL_EVENT
    SDL_Event event[128];
    #else
    VncEvent event;
    #endif
    
    while (!aborted())
    {
        //
        #if USE_SDL_EVENT
        SDL_PumpEvents();

        int count = SDL_PeepEvents(event, sizeof(event) / sizeof(event[0]), SDL_GETEVENT, VncClient::EventType, VncClient::EventType);
        for (int i = 0; i < count; i++)
            processClientEvent(event[i].user.code, event[i].user.data1, event[i].user.data2);

        count = SDL_PeepEvents(event, sizeof(event) / sizeof(event[0]), SDL_GETEVENT, VncScreen::EventType, VncScreen::EventType);
        for (int i = 0; i < count; i++)
            processScreenEvent(event[i].user.code, event[i].user.data1, event[i].user.data2);
        #else
        while (VNC_PollEvent(&m_evtQueue, &event))
        {
            if (event.type == VncClient::EventType)
                processClientEvent(event.code, event.data1, event.data2);
            else if (event.type == VncScreen::EventType)    
                processScreenEvent(event.code, event.data1, event.data2);            
        }
        #endif


        //
        SDL_LockMutex(m_mutex);
        uint32_t ms = lv_timer_handler();
        SDL_UnlockMutex(m_mutex);

        SDL_Delay(ms);
    }
}

void VncApplication::loop()
{
    //
    SDL_LockMutex(m_mutex);
    uint32_t ms = lv_timer_handler();
    SDL_UnlockMutex(m_mutex);

    lv_sleep_ms(ms);
}

bool VncApplication::aborted()
{
    return m_interrupted != 0 || lv_display_get_default() == nullptr;
}



void VncApplication::updateFrame(uint32_t* fb, size_t size)
{
    SDL_LockMutex(m_mutex);
    m_scrn->refreshFrame(fb, size);
    SDL_UnlockMutex(m_mutex);
}



int VncApplication::parseCmdLine(int argc, char* argv[])
{
    if (argc < 2) 
    {
        std::cerr << "Usage: " << argv[0] << " <host> [--port <port>] [--password <password>]" << std::endl;
        return 0;
    }

    std::string host = argv[1];
    int port = 5900;
    std::string password;

    for (int i = 2; i < argc; ++i) 
    {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) 
        {
            port = std::stoi(argv[++i]);
        } else if (arg == "--password" && i + 1 < argc) 
        {
            password = argv[++i];
        }
    }
    std::cout << "VncServer: addr = " << host << ", port = " << port << ", password = " << password << std::endl;

    //
    m_scrn->setConnectInfo(host.c_str(), port, password.c_str());

    return 0;    
}


void VncApplication::postUserEvent(Uint32 type, Uint32 code, void* data1, void* data2)
{
    #if USE_SDL_EVENT
    SDL_Event event;
    SDL_zero(event);

    event.type = type;
    event.user.code = code;
    event.user.data1 = data1;
    event.user.data2 = data2;

    if (SDL_PushEvent(&event) == 0)
    {
        LV_LOG_ERROR("User event filtered: type(%d), code(%d)\n",type, code);
    }
    #else
    VncEvent event;

    event.type = type;
    event.code = code;
    event.data1 = data1;
    event.data2 = data2;

    VNC_PushEvent(&m_evtQueue, &event);
    #endif
}


void VncApplication::processClientEvent(Uint32 code, void* data1, void* data2)
{
    if (code == VncClient::EVENT_CLIENT_STARTED)
    {
        LV_LOG_INFO("VncClient::EVENT_CLIENT_STARTED\n");
        m_scrn->startPlay(1024, 768, 4);
    }    
    else if (code == VncClient::EVENT_CLIENT_UPDATE_FRAME)
    {
        uint32_t* fb = (uint32_t *)data1;
        size_t fb_size = (size_t)data2;

        //LV_LOG_INFO("UPDATE_FRAME: \n");
    }
    else if (code == VncClient::EVENT_CLIENT_FINISHED)
    {
        LV_LOG_INFO("VncClient::EVENT_CLIENT_FINISHED\n");
        if (m_client)
        {
            m_client->close();
            delete m_client;
            m_client = nullptr;
        }

        m_scrn->stopPlay();
    }
    else if (code == VncClient::EVENT_CLIENT_FAILED)
    {
        // EVENT_CLIENT_FAILED
        //      ERROR_SOCKET_FAILED
        //      ERROR_CONNECT_FAILED
        //      ERROR_HANDSHAKE_FAILED
        LV_LOG_INFO("client error: %d\n", (int)(uint64_t)data1); // errno = (int)data1
        if (m_client)
        {
            m_client->close();
            delete m_client;
            m_client = nullptr;
        }

        m_scrn->stopPlay();
    }
}

void VncApplication::processScreenEvent(Uint32 code, void* data1, void* data2)
{
    switch (code)
    {
    case VncScreen::EVENT_CONNECT:
        LV_LOG_INFO("Connect to: %s:#%d\n", m_scrn->getServerAddr(), m_scrn->getServerPort());
        if (m_client)
            return;

        m_client = new VncClient(this);
        if (m_client)
        {
            if( m_client->connectToServer(
                    m_scrn->getServerAddr(), 
                    m_scrn->getServerPort(),
                    m_scrn->getPassword()))
            {
                //
                //
            }
            else
            {
                //
                // failed client-task starting
                //
            }
        }
        break;    

    case VncScreen::EVENT_DISCONNECT:
        LV_LOG_INFO("Close VNC connection.\n");
        if (m_client)
            m_client->disconnect();
        break;
    }
}



void VncApplication::setupSignal()
{
#if defined(_WIN32) || defined(_WIN64)
    // Windows Environment
    signal(SIGINT, signal_handler);
#else
    // POSIX(Linux/macOS) Environment
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
#endif
}

void VncApplication::signal_handler(int no)
{
    printf("\nUser interrupt program!\n\n");

    m_interrupted = 1;
}



//
//
//

int main(int argc, char* argv[])
{
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        return -1;
#endif

    //
    VncApplication app;

    if (app.begin())
    {
        if (app.parseCmdLine(argc, argv) != 0)
            return 1;

        /*
        while (!app.aborted())
        {
            //
            app.loop();
        }
        */
        app.run();
    }

#ifdef _WIN32
    WSACleanup();
#endif    

    return 0;
}
