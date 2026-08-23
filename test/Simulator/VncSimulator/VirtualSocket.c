// VirtualSocket.c
//

#include <WinSock2.h>
#include <windows.h>
#include <stdint.h>
#include <stdbool.h>
#include "VirtualSocket.h"


#if defined(_SIMULATOR)
#include "FreeRTOS.h"
#include "task.h"
#else
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif


#pragma comment(lib, "ws2_32.lib")


//
//
//

struct RingBuffer_s
{
	uint8_t* dataPtr;
	uint32_t dataLen;
	uint32_t front;
	uint32_t rear;
};

typedef struct RingBuffer_s RingBuffer;


static void RB_Init(RingBuffer* pBuf, uint32_t size)
{
	pBuf->dataPtr = (uint8_t*)malloc(size);
	pBuf->dataLen = size;
	pBuf->front = 0;
	pBuf->rear = 0;
}

static void RB_DeInit(RingBuffer* pBuf)
{
	free(pBuf->dataPtr);

	pBuf->dataPtr = NULL;
	pBuf->dataLen = 0;
	pBuf->front = 0;
	pBuf->rear = 0;
}

static bool RB_IsEmpty(RingBuffer* pBuf)
{
	return pBuf->front == pBuf->rear;
}

static bool RB_IsFull(RingBuffer* pBuf)
{
	return ((pBuf->front + 1) & (pBuf->dataLen - 1)) == pBuf->rear;
}

static uint32_t RB_DataSize(RingBuffer* pBuf)
{
	return (pBuf->front - pBuf->rear) & (pBuf->dataLen - 1);
}

static uint32_t RB_AvailableSize(RingBuffer* pBuf)
{
	return pBuf->dataLen - (RB_DataSize(pBuf) + 1); // full state: data-length - 1
}

static uint32_t RB_Read(RingBuffer* pBuf, uint8_t* pData, uint32_t nDataLen)
{
	uint32_t nDataSize = RB_DataSize(pBuf);
	//if (nDataLen > nDataSize)
	if (nDataSize == 0)
		return 0;

	uint32_t nRemain = min(nDataLen, nDataSize);
	uint8_t* pSrc = pBuf->dataPtr + pBuf->rear;
	uint8_t* pDst = pData;

	if (pBuf->front < pBuf->rear)
	{
		uint32_t nSpace = pBuf->dataLen - pBuf->rear;
		uint32_t nCopy = min(nSpace, nRemain);
		memcpy(pDst, pSrc, nCopy);

		pBuf->rear = (pBuf->rear + nCopy) & (pBuf->dataLen - 1);
		nRemain -= nCopy;
		pDst += nCopy;
		pSrc = pBuf->dataPtr + pBuf->rear;
	}

	if (nRemain > 0)
	{
		uint32_t nSpace = pBuf->front - pBuf->rear;
		uint32_t nCopy = min(nSpace, nRemain);
		memcpy(pDst, pSrc, nCopy);

		pBuf->rear = (pBuf->rear + nCopy) & (pBuf->dataLen - 1);
	}

	return min(nDataLen, nDataSize);
}

static uint32_t RB_Write(RingBuffer* pBuf, uint8_t* pData, uint32_t nDataLen)
{
	uint32_t nAvailable = RB_AvailableSize(pBuf);
	if (nDataLen > nAvailable)
		return 0;

	uint32_t nRemain = nDataLen;
	uint8_t* pSrc = pData;
	uint8_t* pDst = pBuf->dataPtr + pBuf->front;

	if (pBuf->front >= pBuf->rear)
	{
		uint32_t nSpace = pBuf->dataLen - pBuf->front;
		uint32_t nCopy = min(nSpace, nRemain);
		memcpy(pDst, pSrc, nCopy);

		pBuf->front = (pBuf->front + nCopy) & (pBuf->dataLen - 1);
		nRemain -= nCopy;
		pSrc += nCopy;
		pDst = pBuf->dataPtr + pBuf->front;
	}

	if (nRemain > 0)
	{
		uint32_t nSpace = pBuf->rear - pBuf->front;
		uint32_t nCopy = min(nSpace, nRemain);
		memcpy(pDst, pSrc, nCopy);

		pBuf->front = (pBuf->front + nCopy) & (pBuf->dataLen - 1);
	}

	return nDataLen;
}




//
//
//

struct VirtualSocket
{
	SOCKET socket;
	VirtualSocket_State nState;

	// rx
	CRITICAL_SECTION rxSection;
	HANDLE rxThread;
	RingBuffer rxBuffer;

	// tx
	CRITICAL_SECTION txSection;
	HANDLE txThread;
	RingBuffer txBuffer;
};


static DWORD WINAPI VirtualSocket_Recv(LPVOID pParam)
{
	VirtualSocket_t* s = (VirtualSocket_t*)pParam;
	uint32_t len = 8 * 1024;
	uint8_t* buf = (uint8_t*)malloc(len);
	if (!buf)
		return 0;

	while (1)
	{
		int ret = recv(s->socket, buf, len, 0);
		if (ret > 0)
		{
			uint8_t* ptr = buf;
			while (ret > 0)
			{
				EnterCriticalSection(&s->rxSection);
				uint32_t n = RB_Write(&s->rxBuffer, ptr, ret);				
				LeaveCriticalSection(&s->rxSection);

				ptr = ptr + n;
				ret = ret - n;

				if (ret)
					Sleep(100);
			}
		}
		else
		{
			if (ret == 0)
			{
				s->nState = E_DISCONNECTED;
			}
			else
			{
				s->nState = E_ERROR;
			}

			break;
		}
	}

	free(buf);
	return 0;
}

static DWORD WINAPI VirtualSocket_Send(LPVOID pParam)
{
	VirtualSocket_t* s = (VirtualSocket_t*)pParam;
	uint32_t len = 1 * 1024;
	uint8_t* buf = (uint8_t*)malloc(len);
	if (!buf)
		return 0;

	while (1)
	{
		EnterCriticalSection(&s->txSection);
		uint32_t n = RB_Read(&s->txBuffer, buf, len);
		LeaveCriticalSection(&s->txSection);

		if (n > 0)
		{
			uint8_t* ptr = buf;
			while (n > 0 && s->nState == E_CONNECTED)
			{
				int ret = send(s->socket, ptr, n, 0);
				if (ret > 0)
				{
					n -= ret;
				}
				else
				{
					if (ret == 0)
					{
						s->nState = E_DISCONNECTED;
					}
					else
					{
						s->nState = E_ERROR;
					}
				}

				Sleep(10);
			}

			if (s->nState != E_CONNECTED)
				break;
		}

		Sleep(10);
	}

	free(buf);
	return 0;
}


VirtualSocket_t* VirtualSocket_socket(int af, int type, int protocol)
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	VirtualSocket_t* s = malloc(sizeof(VirtualSocket_t));
	if (!s)
		return (VirtualSocket_t*)(uint32_t)-1;

	s->socket = socket(af, type, protocol);
	if (s->socket == INVALID_SOCKET)
		return (VirtualSocket_t*)(uint32_t)-1;

	InitializeCriticalSection(&s->rxSection);
	InitializeCriticalSection(&s->txSection);

	RB_Init(&s->rxBuffer, 1024 * 1024);
	RB_Init(&s->txBuffer, 2 * 1024);

	s->rxThread = CreateThread(NULL, 0, VirtualSocket_Recv, s, CREATE_SUSPENDED, NULL);
	s->txThread = CreateThread(NULL, 0, VirtualSocket_Send, s, CREATE_SUSPENDED, NULL);

	s->nState = E_SUSPENDED;

	return s;
}

void VirtualSocket_closesocket(VirtualSocket_t* s)
{
	//
	closesocket(s->socket);

	CloseHandle(s->rxThread);
	CloseHandle(s->txThread);

	WaitForSingleObject(s->rxThread, INFINITE);
	WaitForSingleObject(s->txThread, INFINITE);

	DeleteCriticalSection(&s->rxSection);
	DeleteCriticalSection(&s->txSection);

	RB_DeInit(&s->rxBuffer);
	RB_DeInit(&s->txBuffer);

	free(s);
}




int VirtualSocket_connect(VirtualSocket_t* s, struct VirtualSocket_sockaddr* sockaddr, int namelen)
{
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = sockaddr->port;
	addr.sin_addr.s_addr = sockaddr->addr;

	int ret = connect(s->socket, (const struct sockaddr *)&addr, sizeof(addr));
	if (ret == 0)
	{
		s->nState = E_CONNECTED;

		ResumeThread(s->rxThread);
		ResumeThread(s->txThread);
	}
	else
	{
		s->nState = E_ERROR;
	}

	return ret;
}


int VirtualSocket_setsockopt(VirtualSocket_t* s, int lLevel, int lOptionName, const char* pvOptionValue, int xOptionLength)
{
	//setsockopt(SOCKET, int, int, const char* int);
	return 0;
}



int VirtualSocket_send(VirtualSocket_t* s, const void* pvBuffer, int xTotalLength, int ulFlags)
{
	EnterCriticalSection(&s->txSection);
	uint32_t n = RB_Write(&s->txBuffer, pvBuffer, xTotalLength);
	LeaveCriticalSection(&s->txSection);

	return n;
}

int VirtualSocket_recv(VirtualSocket_t* s, void* pvBuffer, int xBufferLength, int ulFlags)
{
	do
	{
		EnterCriticalSection(&s->rxSection);
		uint32_t n = RB_Read(&s->rxBuffer, pvBuffer, xBufferLength);
		LeaveCriticalSection(&s->rxSection);

		if (n > 0)
			return n;

		if (s->nState != E_CONNECTED)
			return s->nState;

		vTaskDelay(pdMS_TO_TICKS(10));

	} while (1);

	return 0;
}


unsigned long VirtualSocket_inet_addr(const char* cp)
{
	return inet_addr(cp);
}

unsigned long VirtualSocket_htonl(unsigned long hostlong)
{
	return htonl(hostlong);
}

unsigned short VirtualSocket_htons(unsigned short hostshort)
{
	return htons(hostshort);
}

unsigned long VirtualSocket_ntohl(unsigned long netlong)
{
	return ntohl(netlong);
}

unsigned short VirtualSocket_ntohs(unsigned short netshort)
{
	return ntohs(netshort);
}

