// main.c
//

/*
 * FreeRTOS V202212.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

 /******************************************************************************
  * This project provides two demo applications.  A simple blinky style project,
  * and a more comprehensive test and demo application.  The
  * mainCREATE_SIMPLE_BLINKY_DEMO_ONLY setting is used to select between the two.
  * The simply blinky demo is implemented and described in main_blinky.c.  The
  * more comprehensive test and demo application is implemented and described in
  * main_full.c.
  *
  * This file implements the code that is not demo specific, including the
  * hardware setup and FreeRTOS hook functions.
  *
  *******************************************************************************
  * NOTE: Windows will not be running the FreeRTOS demo threads continuously, so
  * do not expect to get real time behaviour from the FreeRTOS Windows port, or
  * this demo application.  Also, the timing information in the FreeRTOS+Trace
  * logs have no meaningful units.  See the documentation page for the Windows
  * port for further information:
  * https://www.FreeRTOS.org/FreeRTOS-Windows-Simulator-Emulator-for-Visual-Studio-and-Eclipse-MingW.html
  *
  *
  *******************************************************************************
  */

  /* Standard includes. */
#include <stdio.h>
#include <time.h>

/* Windows includes. */
#include <windows.h>

/* FreeRTOS includes. */
#include <FreeRTOS.h>
#include "task.h"

#include "FreeRTOSIPConfig.h"

/* Demo application includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
/*
#include "SimpleUDPClientAndServer.h"
#include "SimpleTCPEchoServer.h"
#include "TCPEchoClient_SingleTasks.h"
*/
#include "logging.h"

#include "app_main.h"


/*
 * Miscellaneous initialisation including preparing the logging and seeding the
 * random number generator.
 */
static void prvMiscInitialisation(void);

#if SELF_IMPLEMENT
/* The default IP and MAC address used by the demo.  The address configuration
 * defined here will be used if ipconfigUSE_DHCP is 0, or if ipconfigUSE_DHCP is
 * 1 but a DHCP server could not be contacted.  See the online documentation for
 * more information. */
static const uint8_t ucIPAddress[4] = { configIP_ADDR0, configIP_ADDR1, configIP_ADDR2, configIP_ADDR3 };
static const uint8_t ucNetMask[4] = { configNET_MASK0, configNET_MASK1, configNET_MASK2, configNET_MASK3 };
static const uint8_t ucGatewayAddress[4] = { configGATEWAY_ADDR0, configGATEWAY_ADDR1, configGATEWAY_ADDR2, configGATEWAY_ADDR3 };
static const uint8_t ucDNSServerAddress[4] = { configDNS_SERVER_ADDR0, configDNS_SERVER_ADDR1, configDNS_SERVER_ADDR2, configDNS_SERVER_ADDR3 };

/* Default MAC address configuration.  The demo creates a virtual network
 * connection that uses this MAC address by accessing the raw Ethernet data
 * to and from a real network connection on the host PC.  See the
 * configNETWORK_INTERFACE_TO_USE definition for information on how to configure
 * the real network connection to use. */
const uint8_t ucMACAddress[6] = { configMAC_ADDR0, configMAC_ADDR1, configMAC_ADDR2, configMAC_ADDR3, configMAC_ADDR4, configMAC_ADDR5 };


#if defined( ipconfigIPv4_BACKWARD_COMPATIBLE ) && ( ipconfigIPv4_BACKWARD_COMPATIBLE == 0 )
/* In case multiple interfaces are used, define them statically. */

/* With WinPCap there is only 1 physical interface. */
static NetworkInterface_t xInterfaces[1];

/* It will have several end-points. */
static NetworkEndPoint_t xEndPoints[4];

#endif /* defined( ipconfigIPv4_BACKWARD_COMPATIBLE ) && ( ipconfigIPv4_BACKWARD_COMPATIBLE == 0 ) */
#endif // SELF_IMPLEMENT

int main()
{
    /*
     * Instructions for using this project are provided on:
     * https://www.FreeRTOS.org/FreeRTOS-Plus/FreeRTOS_Plus_TCP/examples_FreeRTOS_simulator.html
     */

    /* Miscellaneous initialisation including preparing the logging and seeding
     * the random number generator. */
    prvMiscInitialisation();

#if 0
    /* Initialise the network interface.
     *
     ***NOTE*** Tasks that use the network are created in the network event hook
     * when the network is connected and ready for use (see the definition of
     * vApplicationIPNetworkEventHook() below).  The address values passed in here
     * are used if ipconfigUSE_DHCP is set to 0, or if ipconfigUSE_DHCP is set to 1
     * but a DHCP server cannot be	contacted. */

     /* Initialise the network interface.*/
#if SELF_IMPLEMENT
    FreeRTOS_debug_printf(("FreeRTOS_IPInit\r\n"));

#if defined( ipconfigIPv4_BACKWARD_COMPATIBLE ) && ( ipconfigIPv4_BACKWARD_COMPATIBLE == 0 )
    extern NetworkInterface_t* pxWinPcap_FillInterfaceDescriptor(BaseType_t xEMACIndex,
        NetworkInterface_t * pxInterface);

    //
    pxWinPcap_FillInterfaceDescriptor(0, &(xInterfaces[0]));

    /* === End-point 0 === */
    FreeRTOS_FillEndPoint(&(xInterfaces[0]), &(xEndPoints[0]), ucIPAddress, ucNetMask, ucGatewayAddress, ucDNSServerAddress, ucMACAddress);
#if ( ipconfigUSE_DHCP != 0 )
    {
        /* End-point 0 wants to use DHCPv4. */
        xEndPoints[0].bits.bWantDHCP = pdTRUE;
    }
#endif /* ( ipconfigUSE_DHCP != 0 ) */

    BaseType_t xResult = FreeRTOS_IPInit_Multi();
#else /* if defined( ipconfigIPv4_BACKWARD_COMPATIBLE ) && ( ipconfigIPv4_BACKWARD_COMPATIBLE == 0 ) */
    /* Using the old /single /IPv4 library, or using backward compatible mode of the new /multi library. */
    xResult = FreeRTOS_IPInit(ucIPAddress, ucNetMask, ucGatewayAddress, ucDNSServerAddress, ucMACAddress);
#endif /* defined( ipconfigIPv4_BACKWARD_COMPATIBLE ) && ( ipconfigIPv4_BACKWARD_COMPATIBLE == 0 ) */

    configASSERT(xResult == pdTRUE);
#else // NOT SELF_IMPLEMENT
    extern void vPlatformInitIpStack(void);

    vPlatformInitIpStack();
#endif // SELF_IMPLEMENT
#endif

    //
    //
    //
    vnc_app_init();


    /* Start the RTOS scheduler. */
    FreeRTOS_debug_printf(("vTaskStartScheduler\r\n"));
    vTaskStartScheduler();

    /* If all is well, the scheduler will now be running, and the following
     * line will never be reached.  If the following line does execute, then
     * there was insufficient FreeRTOS heap memory available for the idle and/or
     * timer tasks	to be created.  See the memory management section on the
     * FreeRTOS web site for more details (this is standard text that is not not
     * really applicable to the Win32 simulator port). 
     */
    const uint32_t ulLongTime_ms = pdMS_TO_TICKS(1000UL);
    for (; ; )
        Sleep(ulLongTime_ms);

    return 0;
}




/* Set the following constant to pdTRUE to log using the method indicated by the
 * name of the constant, or pdFALSE to not log using the method indicated by the
 * name of the constant.  Options include to standard out (xLogToStdout) and to a
 * file on disk (xLogToFile). */
const BaseType_t xLogToStdout = pdTRUE, xLogToFile = pdFALSE;

/* Use by the pseudo random number generator. */
static UBaseType_t ulNextRand;

static void prvSRand(UBaseType_t ulSeed)
{
    /* Utility function to seed the pseudo random number generator. */
    ulNextRand = ulSeed;
}

static void prvMiscInitialisation(void)
{
    time_t xTimeNow;
    uint32_t ulRandomNumbers[4];

    vLoggingInit(xLogToStdout, xLogToFile, pdFALSE, 0, 0);

    /* Seed the random number generator. */
    time(&xTimeNow);
    FreeRTOS_debug_printf(("Seed for randomiser: %lu\r\n", xTimeNow));
    prvSRand((uint32_t)xTimeNow);

    (void)xApplicationGetRandomNumber(&ulRandomNumbers[0]);
    (void)xApplicationGetRandomNumber(&ulRandomNumbers[1]);
    (void)xApplicationGetRandomNumber(&ulRandomNumbers[2]);
    (void)xApplicationGetRandomNumber(&ulRandomNumbers[3]);
    FreeRTOS_debug_printf(("Random numbers: %08X %08X %08X %08X\r\n",
        ulRandomNumbers[0],
        ulRandomNumbers[1],
        ulRandomNumbers[2],
        ulRandomNumbers[3]));
}


#define mainECHO_CLIENT_TASK_STACK_SIZE               ( configMINIMAL_STACK_SIZE * 2 )      /* Not used in the Windows port. */
#define mainECHO_CLIENT_TASK_PRIORITY                 ( tskIDLE_PRIORITY + 1 )

static void prvNamespaceLookupTask(void* arg)
{
    uint32_t addr = FreeRTOS_gethostbyname("www.astams.com");
    unsigned char* ip = (unsigned char*)&addr;
    printf("www.astams.com = %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
    addr = FreeRTOS_gethostbyname("www.naver.com");
    ip = (unsigned char*)&addr;
    printf("www.naver.com = %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);

    // 마지막으로 깨어난 시간을 기록할 변수 (처음에는 현재 시간으로 초기화)
    TickType_t xLastWakeTime = xTaskGetTickCount();

    // 1000ms를 Tick 단위로 변환
    const TickType_t xFrequency = pdMS_TO_TICKS(2000);

    for (;; ) // 무한 루프 (while(1)과 동일)
    {
        // 정확히 1000ms 주기로 태스크를 블록 상태로 전환
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        // 여기에 1초마다 실행할 코드 작성
        printf("Heat beat!\n");
    }
}

void vStartDemo()
{
    xTaskCreate(prvNamespaceLookupTask,
        "Lookup",
        1 * 1024,
        0,
        tskIDLE_PRIORITY + 2,
        0);

    //vStartTCPEchoClientTasks_SingleTasks(mainECHO_CLIENT_TASK_STACK_SIZE, mainECHO_CLIENT_TASK_PRIORITY);
}
