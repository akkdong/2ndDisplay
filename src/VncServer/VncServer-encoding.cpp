// VncServer-encoding.cpp : JPEG encoding이 적용된 VNC 서버
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

std::atomic<bool> g_running(true);

BOOL WINAPI ConsoleHandler(DWORD ctrlType) 
{
    if (ctrlType == CTRL_C_EVENT) {
        std::cout << "\n[알림] Ctrl+C 감지! VNC 서버 종료 절차를 시작합니다..." << std::endl;
        g_running = false;
        return TRUE;
    }
    return FALSE;
}



//
//
//

struct MonitorInfo 
{
    int index;
    RECT rect;
    int width;
    int height;
};

BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) 
{
    std::vector<MonitorInfo>* monitorList = reinterpret_cast<std::vector<MonitorInfo>*>(dwData);

    MonitorInfo info;
    info.index = static_cast<int>(monitorList->size());
    info.rect = *lprcMonitor;
    info.width = lprcMonitor->right - lprcMonitor->left;
    info.height = lprcMonitor->bottom - lprcMonitor->top;

    monitorList->push_back(info);
    return TRUE;
}


void CaptureScreen(char* buffer, int width, int height) 
{
    HDC hScreenDC = GetDC(NULL);
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemoryDC, hBitmap);

    BitBlt(hMemoryDC, 0, 0, width, height, hScreenDC, 0, 0, SRCCOPY);

    BITMAPINFOHEADER bi = { 0 };
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = width;
    bi.biHeight = -height;
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;

    GetDIBits(hMemoryDC, hBitmap, 0, height, buffer, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    for (int i = 0; i < width * height * 4; i += 4) 
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


void CaptureScreenWithCursor(rfbScreenInfoPtr screen, const MonitorInfo& targetMonitor, HDC hMemoryDC)
{
    CURSORINFO ci = { 0 };
    ci.cbSize = sizeof(CURSORINFO);

    if (GetCursorInfo(&ci) && (ci.flags == CURSOR_SHOWING))
    {
        POINT cursorPt = ci.ptScreenPos;

        if (PtInRect(&targetMonitor.rect, cursorPt))
        {
            int localX = cursorPt.x - targetMonitor.rect.left;
            int localY = cursorPt.y - targetMonitor.rect.top;
            DrawIconEx(hMemoryDC, localX, localY, ci.hCursor, 0, 0, 0, NULL, DI_NORMAL);
        }
    }
}


void CaptureSpecificDisplay(rfbScreenInfoPtr screen, const MonitorInfo& targetMonitor)
{
    char* buffer = screen->frameBuffer;

    HDC hScreenDC = GetDC(NULL);
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, targetMonitor.width, targetMonitor.height);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemoryDC, hBitmap);

    BitBlt(hMemoryDC, 0, 0, targetMonitor.width, targetMonitor.height,
        hScreenDC, targetMonitor.rect.left, targetMonitor.rect.top, SRCCOPY);

    CaptureScreenWithCursor(screen, targetMonitor, hMemoryDC);

    WORD biBitCount = 24;
    int bytesPerPixel = biBitCount / 8;
    BITMAPINFOHEADER bi = { 0 };
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = targetMonitor.width;
    bi.biHeight = -targetMonitor.height;
    bi.biPlanes = 1;
    bi.biBitCount = biBitCount;
    bi.biCompression = BI_RGB;

    GetDIBits(hMemoryDC, hBitmap, 0, targetMonitor.height, buffer, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    for (int i = 0; i < targetMonitor.width * targetMonitor.height * bytesPerPixel; i += bytesPerPixel)
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

void MyPointerEvent(int buttonMask, int x, int y, rfbClientPtr cl) 
{
    int virtualWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int virtualHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    int virtualLeft = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int virtualTop = GetSystemMetrics(SM_YVIRTUALSCREEN);

    int absoluteX = (activeMonitor ? activeMonitor->rect.left : 0) + x;
    int absoluteY = (activeMonitor ? activeMonitor->rect.top : 0) + y;

    INPUT inputMove = { 0 };
    inputMove.type = INPUT_MOUSE;
    inputMove.mi.dx = ((absoluteX - virtualLeft) * 65535) / virtualWidth;
    inputMove.mi.dy = ((absoluteY - virtualTop) * 65535) / virtualHeight;
    inputMove.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE | MOUSEEVENTF_VIRTUALDESK;
    SendInput(1, &inputMove, sizeof(INPUT));

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
        if (!std::isdigit(c)) return false;

    return true;
}



//
//
//

int main(int argc, char* argv[]) 
{

    if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE)) 
    {
        std::cerr << "콘솔 핸들러 등록 실패" << std::endl;
        return 1;
    }

    std::vector<MonitorInfo> monitorList;
    EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, reinterpret_cast<LPARAM>(&monitorList));

    if (monitorList.empty()) 
    {
        std::cerr << "연결된 디스플레이를 찾을 수 없습니다." << std::endl;
        return 1;
    }

    std::cout << "=== 발견된 디스플레이 목록 ===" << std::endl;
    for (const auto& mon : monitorList) 
    {
        std::cout << "디스플레이 [" << mon.index << "] -> 크기: "
            << mon.width << "x" << mon.height
            << ", 시작좌표: (" << mon.rect.left << ", " << mon.rect.top << ")" << std::endl;
    }

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



    int display_number = 0;
    int vnc_port = 5900 + display_number;

    int width = selectedMonitor.width;
    int height = selectedMonitor.height;
    const int bpp = 3;

    std::vector<char> frame_buffer(width * height * bpp, 0);

    rfbScreenInfoPtr server = rfbGetScreen(nullptr, nullptr, width, height, 8, 3, bpp);
    if (!server) 
    {
        std::cerr << "VNC 서버 초기화 실패" << std::endl;
        return 1;
    }

    // ============================================================
    // JPEG 인코딩 설정 (Tight 인코딩 + JPEG)
    // libvncserver가 libjpeg/libjpeg-turbo와 함께 빌드되어야 합니다.
    // VNC 프로토콜에서 인코딩은 클라이언트가 SetEncodings으로 지정하며,
    // 서버는 newClientHook을 통해 클라이언트별 JPEG 품질을 설정합니다.
    // ============================================================
#if defined(LIBVNCSERVER_HAVE_LIBZ) || defined(LIBVNCSERVER_HAVE_LIBPNG)
    server->newClientHook = [](rfbClientPtr cl) -> enum rfbNewClientAction {
#ifdef LIBVNCSERVER_HAVE_LIBJPEG
        cl->tightQualityLevel = 80;  // JPEG 품질 (1~100, 낮을수록 높은 압축)
        cl->tightUsePixelFormat24 = TRUE;  // 24비트 색상 최적화
#endif
        return RFB_CLIENT_ACCEPT;
    };
#endif
    // ============================================================

    // 비밀번호 인증 설정
    static const char* passwords[] = { "password", nullptr };
    server->authPasswdData = (void*)passwords;
    server->passwordCheck = rfbCheckPasswordByList;

    // 원격 제어 콜백 함수 등록
#if ENABLE_KEYBD_MOUSE_EVENT
    activeMonitor = &selectedMonitor;
    server->ptrAddEvent = MyPointerEvent;
    server->kbdAddEvent = MyKeyboardEvent;
#endif

    server->desktopName = "Windows C++ VNC Server (JPEG)";
    server->frameBuffer = frame_buffer.data();
    server->alwaysShared = TRUE;
    server->port = vnc_port;

    rfbInitServer(server);
    std::cout << "Windows VNC 서버 시작됨 (JPEG 인코딩). 포트: " << vnc_port << " (" << width << "x" << height << ")" << std::endl;

    // 메인 루프: 실시간 화면 전송
    while (g_running && rfbIsActive(server)) 
    {
#if 0
        CaptureScreen(server->frameBuffer, width, height);
#else   
        CaptureSpecificDisplay(server, selectedMonitor);
#endif

        rfbMarkRectAsModified(server, 0, 0, width, height);

        rfbProcessEvents(server, 30000);
    }

    rfbShutdownServer(server, TRUE);
    rfbScreenCleanup(server);

    std::cout << "VNC 서버가 정상 종료되었습니다." << std::endl;
    return 0;
}
