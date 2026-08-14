// app_main.c
//

#include "vnc_display.h"
#include "vnc_screen.h"



//
//
//

static void vnc_start_client(vnc_screen_t* scrn, const char* addr, uint16_t port)
{
    /*
    vnc_connect_info_t* cinfo = (vnc_connect_info_t *)arg;

    vnc_client_t* client = malloc(vcn_client_t);
    vnc_client_init(client);
    vnc_client_start(client, cinfo->addr, cinfo->port);
    */
}



//
//
//

void app_init()
{
	vnc_display_t* vnc_disp = vnc_display_start();
	//vTaskDelay(pdMS_TO_TICKS(100));

    //
    vnc_screen_config_t vnc_cfg = {
        .on_connect = vnc_start_client,

        //
        // ...
        //
        .disp = vnc_disp,
    };

    vnc_screen_t* vnc_scrn = vnc_screen_init(&vnc_cfg);
    //g_scrn = vnc_scrn;
    vnc_scrn->create(vnc_scrn);

}
