// vnc_client.h
//

#pragma once


#include "extern.h"
#include "vnc_types.h"

BEGIN_EXTERN_C();


struct vnc_client_s
{
	vnc_app_t* app;

};



//
//
//

vnc_client_t* vnc_client_start(vnc_app_t* app);



END_EXTERN_C();
