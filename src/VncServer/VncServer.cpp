// VncServer.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1

#include <iostream>
#include <vector>

#include <winsock2.h>
#include "rfb/rfb.h"
#include "rfb/keysym.h"
#include <windows.h>

#include <cryptopp/md5.h>

#pragma comment(lib, "vncserver.lib")
#pragma comment(lib, "ws2_32.lib")


//
//
//

// 루프 제어용 원자적(Atomic) 플래그 변수
std::atomic<bool> g_running(true);

// Windows 콘솔 이벤트(Ctrl+C 등)를 가로채는 콜백 함수
BOOL WINAPI ConsoleHandler(DWORD ctrlType) 
{
    if (ctrlType == CTRL_C_EVENT) {
        std::cout << "\n[알림] Ctrl+C 감지! VNC 서버 종료 절차를 시작합니다..." << std::endl;
        g_running = false; // 플래그를 false로 바꾸어 메인 루프 탈출 유도
        return TRUE;       // 이 이벤트를 프로그램이 처리했음을 Windows에 알림
    }
    return FALSE;
}



//
//
//

struct MonitorInfo 
{
    int index;
    RECT rect; // 모니터의 가상 공간 내 좌표 (left, top, right, bottom)
    int width;
    int height;
};

// EnumDisplayMonitors에서 실행될 콜백 함수 (모니터를 순차 검색)
BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) 
{
    std::vector<MonitorInfo>* monitorList = reinterpret_cast<std::vector<MonitorInfo>*>(dwData);

    MonitorInfo info;
    info.index = static_cast<int>(monitorList->size());
    info.rect = *lprcMonitor;
    info.width = lprcMonitor->right - lprcMonitor->left;
    info.height = lprcMonitor->bottom - lprcMonitor->top;

    monitorList->push_back(info);
    return TRUE; // 계속해서 다음 모니터 검색
}


// Windows 화면을 캡처하여 VNC 프레임버퍼에 복사하는 함수
void CaptureScreen(char* buffer, int width, int height) 
{
    HDC hScreenDC = GetDC(NULL);
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemoryDC, hBitmap);

    // 화면 캡처 실행
    BitBlt(hMemoryDC, 0, 0, width, height, hScreenDC, 0, 0, SRCCOPY);

    BITMAPINFOHEADER bi = { 0 };
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = width;
    bi.biHeight = -height; // 위아래가 뒤집히는 것을 방지
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;

    // 비트맵 데이터를 BGRA 형태로 버퍼에 직접 추출
    GetDIBits(hMemoryDC, hBitmap, 0, height, buffer, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    // VNC는 기본적으로 RGBA 형식을 선호하므로 BGRA에서 B와 R의 위치를 스왑합니다
    for (int i = 0; i < width * height * 4; i += 4) 
    {
        char temp = buffer[i];     // Blue
        buffer[i] = buffer[i + 2]; // Red를 Blue 자리에
        buffer[i + 2] = temp;     // Blue를 Red 자리에
    }

    // 자원 해제
    SelectObject(hMemoryDC, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hMemoryDC);
    ReleaseDC(NULL, hScreenDC);
}


void CaptureScreenWithCursor(rfbScreenInfoPtr screen, const MonitorInfo& targetMonitor, HDC hMemoryDC)
{
    // 1. 화면 캡처 진행 (기존 소스코드의 캡처 로직)
    // hMemoryDC에 targetMonitor 영역의 화면이 이미 복사되어 있고,
    // 이 메모리가 screen->frameBuffer와 연결되어 있다고 가정합니다.

    // 2. 윈도우 마우스 커서 정보 가져오기
    CURSORINFO ci = { 0 };
    ci.cbSize = sizeof(CURSORINFO);

    if (GetCursorInfo(&ci) && (ci.flags == CURSOR_SHOWING))
    {
        POINT cursorPt = ci.ptScreenPos; // 가상 화면 기준 커서 위치

        // 3. 커서가 지정한 모니터 영역(RECT) 안에 있는지 검사
        // PtInRect API를 사용하면 좌표가 RECT 내부에 있는지 쉽게 확인할 수 있습니다.
        if (PtInRect(&targetMonitor.rect, cursorPt))
        {

            // 4. 모니터 기준의 상대 좌표(로컬 좌표)로 변환
            int localX = cursorPt.x - targetMonitor.rect.left;
            int localY = cursorPt.y - targetMonitor.rect.top;

            // 5. 메모리 DC(화면 버퍼) 위에 커서 그리기
            DrawIconEx(hMemoryDC, localX, localY, ci.hCursor, 0, 0, 0, NULL, DI_NORMAL);
        }
    }

    // 6. LibVNCServer에 변경 영역 알리기
    //rfbMarkRectAsModified(screen, 0, 0, screen->width, screen->height);
}


// 지정된 모니터의 영역 좌표(rect)를 기준으로 화면을 캡처하는 함수
void CaptureSpecificDisplay(rfbScreenInfoPtr screen, const MonitorInfo& targetMonitor)
{
    char* buffer = screen->frameBuffer;

    HDC hScreenDC = GetDC(NULL); // 전체 가상 화면의 DC 획득
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, targetMonitor.width, targetMonitor.height);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemoryDC, hBitmap);

    // 지정한 모니터의 시작 좌표(left, top)부터 화면 크기만큼 복사
    BitBlt(hMemoryDC, 0, 0, targetMonitor.width, targetMonitor.height,
        hScreenDC, targetMonitor.rect.left, targetMonitor.rect.top, SRCCOPY);

    // draw cursor
    CaptureScreenWithCursor(screen, targetMonitor, hMemoryDC);

    BITMAPINFOHEADER bi = { 0 };
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = targetMonitor.width;
    bi.biHeight = -targetMonitor.height; // 상하 반전 방지
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;

    GetDIBits(hMemoryDC, hBitmap, 0, targetMonitor.height, buffer, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    // BGRA -> RGBA 색상 채널 스왑
    for (int i = 0; i < targetMonitor.width * targetMonitor.height * 4; i += 4) 
    {
        char temp = buffer[i];
        buffer[i] = buffer[i + 2];
        buffer[i + 2] = temp;
    }

    SelectObject(hMemoryDC, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hMemoryDC);
    ReleaseDC(NULL, hScreenDC);
}



//
//
//

#if ENABLE_KEYBD_MOUSE_EVENT

MonitorInfo* activeMonitor = nullptr;

// 1. 마우스 이벤트 (네 번째 인자는 반드시 rfbClientPtr cl)
void MyPointerEvent(int buttonMask, int x, int y, rfbClientPtr cl) 
{
    // 1. Windows 전체 가상 가상 화면(모든 모니터가 포함된 영역)의 전체 크기 획득
    int virtualWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int virtualHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    // 가상 가상 화면의 시작점 (다중 모니터 배치에 따라 음수일 수도 있음)
    int virtualLeft = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int virtualTop = GetSystemMetrics(SM_YVIRTUALSCREEN);

    // 2. VNC 상대 좌표(x, y)를 Windows 전체 가상 공간의 절대 좌표로 변환
    // 선택한 모니터의 시작 좌표(rect.left/top)를 더해줍니다.
    int absoluteX = (activeMonitor ? activeMonitor->rect.left : 0) + x;
    int absoluteY = (activeMonitor ? activeMonitor->rect.top : 0) + y;

    // 3. 전체 가상 스크린 기준의 0 ~ 65535 비율로 변환
    INPUT inputMove = { 0 };
    inputMove.type = INPUT_MOUSE;
    inputMove.mi.dx = ((absoluteX - virtualLeft) * 65535) / virtualWidth;
    inputMove.mi.dy = ((absoluteY - virtualTop) * 65535) / virtualHeight;

    // 멀티 모니터 가상 공간 이동을 위해 MOUSEEVENTF_VIRTUALDESK 플래그 필수 추가
    inputMove.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE | MOUSEEVENTF_VIRTUALDESK;
    SendInput(1, &inputMove, sizeof(INPUT));

    // 4. 클릭 이벤트 처리 (이전 로직과 동일)
    INPUT inputButton = { 0 };
    inputButton.type = INPUT_MOUSE;
    if (buttonMask & 1) inputButton.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    else inputButton.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(1, &inputButton, sizeof(INPUT));

    if (buttonMask & 4) inputButton.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
    else inputButton.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
    SendInput(1, &inputButton, sizeof(INPUT));

    rfbDefaultPtrAddEvent(buttonMask, x, y, cl);
}

// 2. 키보드 이벤트 (세 번째 인자는 rfbKeySym, 네 번째 인자는 rfbClientPtr cl)
void MyKeyboardEvent(rfbBool down, rfbKeySym key, rfbClientPtr cl) 
{
    INPUT input = { 0 };
    input.type = INPUT_KEYBOARD;

    WORD vkCode = 0;
    if (key >= 'a' && key <= 'z') vkCode = VkKeyScanA(key) & 0xFF;
    else if (key >= 'A' && key <= 'Z') vkCode = VkKeyScanA(key) & 0xFF;
    else if (key >= '0' && key <= '9') vkCode = key;
    else if (key == XK_Return) vkCode = VK_RETURN;
    else if (key == XK_BackSpace) vkCode = VK_BACK;
    else if (key == XK_Escape) vkCode = VK_ESCAPE;

    if (vkCode == 0) return;

    input.ki.wVk = vkCode;
    input.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;

    SendInput(1, &input, sizeof(INPUT));
}

#endif // ENABLE_KEYBD_MOUSE_EVENT


static bool isNumber(const std::string& str) 
{
    if (str.empty()) return false;

    for (char const& c : str) 
        if (!std::isdigit(c)) return false; // 숫자가 아닌 문자가 있으면 false

    return true;
}



//
//
//

int main(int argc, char* argv[]) 
{

    // Windows 콘솔 컨트롤 핸들러 등록
    if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE)) 
    {
        std::cerr << "콘솔 핸들러 등록 실패" << std::endl;
        return 1;
    }

    // 현재 PC에 연결된 모든 디스플레이 정보 수집
    std::vector<MonitorInfo> monitorList;
    EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, reinterpret_cast<LPARAM>(&monitorList));

    if (monitorList.empty()) 
    {
        std::cerr << "연결된 디스플레이를 찾을 수 없습니다." << std::endl;
        return 1;
    }

    // 연결된 모니터 목록 출력
    std::cout << "=== 발견된 디스플레이 목록 ===" << std::endl;
    for (const auto& mon : monitorList) 
    {
        std::cout << "디스플레이 [" << mon.index << "] -> 크기: "
            << mon.width << "x" << mon.height
            << ", 시작좌표: (" << mon.rect.left << ", " << mon.rect.top << ")" << std::endl;
    }

    // 전송할 디스플레이 선택 (예: 2번 보조 모니터 선택)
    int target_index = -1;
    std::cout << std::endl << std::endl << "전송할 모니터를 선택합니다." << std::endl;
    while (target_index < 0 || target_index >= (int)monitorList.size())
    {
        std::string no;
        std::cout << "[" << 0 << " ... " << (monitorList.size() - 1) << "] > ";
        std::cin >> no;

        if (!isNumber(no))
            continue;
     
        target_index = std::stoi(no);
    }

    if (target_index >= monitorList.size()) 
    {
        std::cout << "\n지정한 디스플레이 번호가 없어 기본값(0번)으로 설정합니다." << std::endl;
        target_index = 0;
    }

    MonitorInfo& selectedMonitor = monitorList[target_index];
    std::cout << "\n선택된 디스플레이: [" << selectedMonitor.index << "] 전송 시작." << std::endl;



    int display_number = 0; // 지정할 디스플레이 번호 (:1 = 5901 포트)   selectedMonitor.index
    int vnc_port = 5900 + display_number;

    // Windows 현재 모니터 해상도 가져오기
    int width = selectedMonitor.width; // GetSystemMetrics(SM_CXSCREEN);
    int height = selectedMonitor.height; // GetSystemMetrics(SM_CYSCREEN);
    const int bpp = 4; // 32비트 색상 (RGBA)

    std::vector<char> frame_buffer(width * height * bpp, 0);

    // VNC 서버 초기화 (vnc_port 변수의 포인터를 전달)
    rfbScreenInfoPtr server = rfbGetScreen(nullptr, nullptr, width, height, 8, 3, bpp);
    if (!server) 
    {
        std::cerr << "VNC 서버 초기화 실패" << std::endl;
        return 1;
    }

    // [비밀번호 인증 설정 추가 시작]
    // 1. 일반 텍스트 패스워드 목록 정의 (마지막 원소는 반드시 0 또는 nullptr)
    static const char* passwords[] = { "pasword", nullptr };

    // 2. 인증 데이터 주입
    server->authPasswdData = (void*)passwords;

    // 3. 올바른 타입의 라이브러리 내장 인증 검증 함수 매핑
    server->passwordCheck = rfbCheckPasswordByList;

    // [원격 제어 콜백 함수 등록]
#if ENABLE_KEYBD_MOUSE_EVENT
    activeMonitor = &selectedMonitor;
    server->ptrAddEvent = MyPointerEvent;   // 마우스 핸들러 연결
    server->kbdAddEvent = MyKeyboardEvent; // 키보드 핸들러 연결
#endif

    server->desktopName = "Windows C++ VNC Server";
    server->frameBuffer = frame_buffer.data();
    server->alwaysShared = TRUE;
    server->port = vnc_port;

    rfbInitServer(server);
    std::cout << "Windows VNC 서버 시작됨. 포트: " << vnc_port << " (" << width << "x" << height << ")" << std::endl;

    // 메인 루프: 실시간 화면 전송
    while (g_running && rfbIsActive(server)) 
    {
#if 0
        // 1. Windows 현재 화면을 캡처하여 버퍼에 업데이트
        //CaptureScreen(server->frameBuffer, width, height);
#else   
        // 선택한 디스플레이의 좌표 영역만 타겟 캡처
        CaptureSpecificDisplay(server, selectedMonitor);
#endif

        // 2. 전체 화면 영역이 수정되었음을 표시
        rfbMarkRectAsModified(server, 0, 0, width, height);

        // 3. VNC 네트워크 이벤트 처리 및 프레임 전송 (30ms 대기)
        rfbProcessEvents(server, 30000);
    }

    // 연결된 모든 클라이언트를 안전하게 디스connect 처리 (TRUE 옵션)
    rfbShutdownServer(server, TRUE);

    // 서버 메모리 최종 자원 해제
    rfbScreenCleanup(server);

    std::cout << "VNC 서버가 정상 종료되었습니다." << std::endl;
    return 0;
}
