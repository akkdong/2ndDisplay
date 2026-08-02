// main.cpp
//


#include <iostream>
#include <string>
#include <queue>
#include <memory>
#include <csignal>

#include "VncViewer.h"
#include "Display.h"


//
//

volatile sig_atomic_t g_sigint = 0;

void sigint_handler(int) 
{ 
    std::cout << "User interrupt program" << std::endl;
    g_sigint = 1; 
}



//
//

int main(int argc, char **argv) 
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <host> [--port <port>] [--password <password>]" << std::endl;
        return 1;
    }

    std::string host = argv[1];
    int port = 5900;
    std::string password;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--password" && i + 1 < argc) {
            password = argv[++i];
        }
    }
    std::cout << "VncServer: " << host << ", port = " << port << ", password = " << password << std::endl;

    {
        struct sigaction sa;
        sa.sa_handler = sigint_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGINT, &sa, nullptr);
    }

    #if 0
    // LVGL + SDL2 (window is resized to the VNC framebuffer after handshake)
    lv_init();
    lv_display_t *disp = lv_sdl_window_create(800, 600);
    lv_indev_t *mouse = lv_sdl_mouse_create();
    lv_indev_set_display(mouse, disp);

    std::unique_ptr<VncClient> clientPtr = std::make_unique<VncClient>(disp);
    if (clientPtr) {
        if (!clientPtr->connect(host, port)) {
            std::cerr << "Connection failed" << std::endl;
            return 1;
        }
        std::cout << "Connected to " << host << ":" << port << std::endl;

        if (!clientPtr->handshake(password)) {
            std::cerr << "Handshake failed" << std::endl;
            return 1;
        }

        clientPtr->run();
    }
    #else
    Display& display = Display::Get();
    if (display.begin(host, port, password))
    {
        // LV_SDL_DIRECT_EXIT = 0: exit button --> clear display --> lv_display_get_default() == NULL
        while (g_sigint == 0 && lv_display_get_default() != nullptr)
        {
            //
            if (!display.loop())
                break;
        }
    }
    #endif

    // clean up
    SDL_Quit();

    return 0;
}
