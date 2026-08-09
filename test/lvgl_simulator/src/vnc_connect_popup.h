// vnc_connect_popup.h
//

#pragma once

#include <stdint.h>



#ifdef __cplusplus
extern "C"
{
#endif


typedef void (*on_connect_cb)(const char* addr, uint16_t port, const char* pass);

void show_vnc_connect_popup(const char* addr, uint16_t port, on_connect_cb callback);


#ifdef __cplusplus
}
#endif
