#include <WinSock2.h>
#include <windows.h>
#include <stdio.h>
#include "FreeRTOS.h"
#include "stream_buffer.h"

#pragma comment(lib, "ws2_32.lib")



// FreeRTOS-Plus-TCP 표준 에러 코드 정의 표준 준수
#define FREERTOS_SOCKET_ERROR       ( -1 )
#define FREERTOS_EINTR              ( -102 )
#define FREERTOS_ENOTCONN           ( -128 )
#define FREERTOS_TIMEDOUT           ( -116 )


typedef enum {
    eSocketConnected = 0,
    eSocketDisconnected,
    eSocketError
} eSocketState_t;

typedef struct WinsockWrapperSocket
{
    SOCKET xClientSocket;

    HANDLE hWindowsTxThread;
    StreamBufferHandle_t xTxBuffer;
    TickType_t xSendTimeout;   // 송신 타임아웃 틱 수

    HANDLE hWindowsRxThread;
    HANDLE hEvent;
    StreamBufferHandle_t xRxBuffer;
    TickType_t xRecvTimeout;   // 수신 타임아웃 틱 수

    volatile eSocketState_t eState; // 소켓 상태 (멀티스레드 접근 유의)
} WinsockWrapperSocket_t;


/*
static StreamBufferHandle_t xTxStreamBuffer = NULL;
static HANDLE hWindowsTxThread = NULL;

static StreamBufferHandle_t xRxStreamBuffer = NULL;
static HANDLE hWindowsRxThread = NULL;
*/

static WinsockWrapperSocket_t clientSocket =
{
    .xClientSocket = INVALID_SOCKET,

    .hWindowsTxThread = NULL,
    .xTxBuffer = NULL,
    .xSendTimeout = portMAX_DELAY,

    .hWindowsRxThread = NULL,
    .hEvent = NULL,
    .xRxBuffer = NULL,
    .xRecvTimeout = portMAX_DELAY,

    .eState = 0,
};



// Windows Native 영역에서 실행되는 실제 고속 데이터 전송 스레드
DWORD WINAPI vWinsockTxThread(LPVOID lpParam) 
{
    WinsockWrapperSocket_t* pxSocket = (WinsockWrapperSocket_t*)lpParam;
    SOCKET xSocket = (SOCKET)pxSocket->xClientSocket;
    uint8_t ucBuffer[1024];
    size_t xReceivedBytes;

    while (1) 
    {
        // FreeRTOS 스트림 버퍼로부터 데이터를 읽어옴 (Non-blocking 또는 짧은 대기)
        // 주의: Windows 스레드에서 FreeRTOS API를 호출할 때는 안전한 스레드 세이프 모델인지 확인 필요
        xReceivedBytes = xStreamBufferReceiveFromISR(pxSocket->xTxBuffer, ucBuffer, sizeof(ucBuffer), 0);

        if (xReceivedBytes > 0) 
        {
            // Winsock을 사용해 무제한 속도로 외부 데이터 덤프/전송
            int ret = send(xSocket, (const char*)ucBuffer, (int)xReceivedBytes, 0);
            printf("send %d bytes\n", ret);
            if (ret <= 0)
                break;
        }
        else 
        {
            Sleep(1); // Windows 스레드가 CPU를 100% 점유하지 않도록 방지
        }
    }

    return 0;
}


// Windows Native 영역에서 실행되는 고속 데이터 수신 스레드
DWORD WINAPI vWinsockRxThread(LPVOID lpParam) 
{
    WinsockWrapperSocket_t* pxSocket = (WinsockWrapperSocket_t*)lpParam;
    SOCKET xWinsockHander = pxSocket->xClientSocket;
    static char pcWinsockBuffer[4096];
    int iBytesReceived;
    BaseType_t xHigherPriorityTaskWoken;

    printf("*\n");
    printf("* WinSock RxTask Started\n");
    printf("*\n");

    while (pxSocket->eState == eSocketConnected) 
    {
        iBytesReceived = recv(xWinsockHander, pcWinsockBuffer, sizeof(pcWinsockBuffer), 0);
        //printf("** recv  >> %d\n", iBytesReceived);

        if (iBytesReceived > 0) 
        {
            xHigherPriorityTaskWoken = pdFALSE;
            xStreamBufferSendFromISR(pxSocket->xRxBuffer, pcWinsockBuffer, iBytesReceived, &xHigherPriorityTaskWoken);
            //portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
            WaitForSingleObject(pxSocket->hEvent, 1000);
            //ResetEvent(pxSocket->hEvent);
            Sleep(2);
        }
        else if (iBytesReceived == 0) 
        {
            // 정상 종료 (Connection Closed by Peer)
            pxSocket->eState = eSocketDisconnected;
            // 대기 중인 Rx Task를 즉시 깨우기 위해 스트림 버퍼 공간 이벤트를 강제 유발 (스트림 버퍼 구조상 데이터가 차면 깨어남)
            // 에러 상태 진입 후 아래의 Wrapper 함수가 상태를 판별하게 됨
            break;
        }
        else 
        {
            // 소켓 에러 발생 (WSAECONNRESET 등)
            pxSocket->eState = eSocketError;
            break;
        }
    }

    printf("*\n");
    printf("* WinSock RxTask Terminated\n");
    printf("*\n");

    return 0;
}


WinsockWrapperSocket_t* Winsock_connect_server(const char* addr, uint16_t port, int timeout)
{
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // 1. 통신용 FreeRTOS 스트림 버퍼 생성
    //vStreamBufferDelete(x);
    //xStreamBufferCreateStatic();

    //vPortGenerateSimulatedInterrupt();

    if (!clientSocket.xTxBuffer)
        clientSocket.xTxBuffer = xStreamBufferCreate(1024 * 4, 1);
    if (!clientSocket.xRxBuffer)
        clientSocket.xRxBuffer = xStreamBufferCreate(1024 * 512, 1);
    clientSocket.xSendTimeout = portMAX_DELAY;
    clientSocket.xRecvTimeout = portMAX_DELAY;
    clientSocket.eState = eSocketConnected;

    // 2. Winsock TCP 소켓 연결 생성
    if (clientSocket.xClientSocket != INVALID_SOCKET)
    {
        closesocket(clientSocket.xClientSocket);
        clientSocket.xClientSocket = INVALID_SOCKET;
    }
    clientSocket.xClientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in xServerAddr;
    xServerAddr.sin_family = AF_INET;
    xServerAddr.sin_port = htons(port);
    xServerAddr.sin_addr.s_addr = inet_addr(addr);

    int ret = connect(clientSocket.xClientSocket, (struct sockaddr*)&xServerAddr, sizeof(xServerAddr));
    if (ret < 0)
        return INVALID_SOCKET;

    // 3. ★중요★ FreeRTOS 스케줄러 외부의 Windows Native 스레드로 분리 생성
    if (clientSocket.hEvent != NULL)
    {
        CloseHandle(clientSocket.hEvent);
        clientSocket.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    }
    clientSocket.hWindowsRxThread = CreateThread(NULL, 0, vWinsockRxThread, (LPVOID)&clientSocket, 0, NULL);
    clientSocket.hWindowsTxThread = CreateThread(NULL, 0, vWinsockTxThread, (LPVOID)&clientSocket, 0, NULL);

    return &clientSocket;
}


WinsockWrapperSocket_t* Winsock_socket(int af, int type, int protocol)
{
    return NULL;
}

int Winsock_connect(WinsockWrapperSocket_t* s, const char* name, int namelen)
{
    return -1;
}

void Winsock_closesocket(WinsockWrapperSocket_t* s)
{
}


int32_t Winsock_setsockopt(WinsockWrapperSocket_t* pxSocket, int32_t lLevel, int32_t lOptionName, const void* pvOptionValue, size_t xOptionLength) 
{
    (void)lLevel;
    (void)xOptionLength;

    if (pxSocket == NULL) return FREERTOS_SOCKET_ERROR;

    // 타임아웃 값은 보통 수 밀리초(uint32_t) 형태로 전달됨
    uint32_t ulTimeoutMs = *((const uint32_t*)pvOptionValue);
    TickType_t xTimeoutTicks = pdMS_TO_TICKS(ulTimeoutMs);

    // FreeRTOS 표준은 0일 경우 블록(Infinite) 혹은 드물게 비블록으로 작동하므로 매핑 유의
    if (ulTimeoutMs == 0) 
    {
        xTimeoutTicks = portMAX_DELAY;
    }

    switch (lOptionName) 
    {
    case 1: // FREERTOS_SO_SNDTIMEO 대체
        pxSocket->xSendTimeout = xTimeoutTicks;
        break;
    case 2: // FREERTOS_SO_RCVTIMEO 대체
        pxSocket->xRecvTimeout = xTimeoutTicks;
        break;
    default:
        return FREERTOS_SOCKET_ERROR;
    }
    return 0;
}

/**
 * @brief Winsock 우회용 송신 래퍼 함수 (FreeRTOS_send 대체)
 */
int32_t Winsock_send(WinsockWrapperSocket_t* pxSocket, const void* pvBuffer, size_t xTotalLength, uint32_t ulFlags) 
{
    (void)ulFlags;

    if (pxSocket == NULL) 
        return FREERTOS_SOCKET_ERROR;
    if (pxSocket->eState != eSocketConnected) 
        return FREERTOS_ENOTCONN;

#if 1
    // 송신 스트림 버퍼에 공간이 날 때까지 xSendTimeout 만큼 대기
    size_t xBytesSent = xStreamBufferSend(
        pxSocket->xTxBuffer,
        pvBuffer,
        xTotalLength,
        pxSocket->xSendTimeout
    );

    if (xBytesSent == 0) {
        // 버퍼가 가득 차서 타임아웃 시간 내에 데이터를 밀어 넣지 못함
        return FREERTOS_TIMEDOUT;
    }

    // 송신 스레드 영역에서 에러가 비동기적으로 감지되었는지 최종 체크
    if (pxSocket->eState == eSocketError) {
        return FREERTOS_ENOTCONN;
    }

    return (int32_t)xBytesSent;
#else
    return send(pxSocket->xClientSocket, (const char*)pvBuffer, (int)xTotalLength, 0);
#endif
}

/**
 * @brief Winsock 우회용 수신 래퍼 함수 (FreeRTOS_recv 대체)
 */
int32_t Winsock_recv(WinsockWrapperSocket_t* pxSocket, void* pvBuffer, size_t xBufferLength, uint32_t ulFlags)
{
    (void)ulFlags;

    if (pxSocket == NULL) return FREERTOS_SOCKET_ERROR;
    if (pxSocket->eState == eSocketDisconnected) return 0; // 연결 종료 상태 시 0 반환
    if (pxSocket->eState == eSocketError) return FREERTOS_ENOTCONN;

    // 설정된 수신 타임아웃(xRecvTimeout) 동안 대기
    size_t xBytesReceived = xStreamBufferReceive(
        pxSocket->xRxBuffer,
        pvBuffer,
        xBufferLength,
        pxSocket->xRecvTimeout
    );
    SetEvent(pxSocket->hEvent);

    // 1. 데이터를 정상적으로 수신한 경우
    if (xBytesReceived > 0) {
        return (int32_t)xBytesReceived;
    }

    // 2. 0바이트 수신 시: 타임아웃이 발생했거나 수신 스레드에서 에러로 인해 깨어난 경우 확인
    if (pxSocket->eState == eSocketDisconnected) {
        return 0; // 상대방이 연결을 끊음
    }
    if (pxSocket->eState == eSocketError) {
        return FREERTOS_ENOTCONN; // 소켓 장애
    }

    // 상태는 정상인데 데이터가 없다면 순수 타임아웃 발생
    return FREERTOS_TIMEDOUT;
}





#if 0

void vMyApplicationTxTask(void* pvParameters) {
    uint8_t ucDataPayload[512] = { 0, }; // 전송할 대량의 데이터

    for (;; ) {
        // 비즈니스 로직 처리...

        // FreeRTOS-Plus-TCP 스택을 우회하고 Winsock 스레드로 데이터 바인딩
        xStreamBufferSend(xTxStreamBuffer, (const void*)ucDataPayload, sizeof(ucDataPayload), portMAX_DELAY);

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void vMyApplicationRxTask(void* pvParameters) {
    static uint8_t ucAppBuffer[1024];
    size_t xReceivedBytes;

    for (;; ) {
        // FreeRTOS 스택을 우회하여 Winsock 스레드가 채워주는 스트림 버퍼로부터 데이터 수신
        // 데이터가 들어올 때까지 이 태스크는 Blocked 상태로 대기합니다.
        xReceivedBytes = xStreamBufferReceive(
            xRxStreamBuffer,
            (void*)ucAppBuffer,
            sizeof(ucAppBuffer),
            portMAX_DELAY
        );

        if (xReceivedBytes > 0) {
            // 수신된 데이터(ucAppBuffer) 처리 로직을 여기에 작성합니다.
            // 예: 프로토콜 파싱, 파일 저장 등
            vProcessReceivedData(ucAppBuffer, xReceivedBytes);
        }
    }
}

#endif
