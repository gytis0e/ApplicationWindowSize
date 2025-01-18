#include <windows.h>
#include <string>
#include <sstream>

HWND overlayWindow;
HWND trackedWindow = NULL;
HWINEVENTHOOK moveSizeHook;

void UpdateOverlayPositionAndText(HWND hwnd, const std::wstring& text)
{
    POINT cursorPos;
    GetCursorPos(&cursorPos);

    SIZE textSize;
    HDC hdc = GetDC(overlayWindow);
    GetTextExtentPoint32(hdc, text.c_str(), (int)text.length(), &textSize);
    ReleaseDC(overlayWindow, hdc);

    int width = textSize.cx + 20;
    int height = textSize.cy + 10;

    SetWindowPos(overlayWindow, HWND_TOPMOST, cursorPos.x + 10, cursorPos.y + 10, width, height, SWP_NOACTIVATE);

    SetWindowText(overlayWindow, text.c_str());

    HDC overlayDC = GetDC(overlayWindow);
    HBRUSH backgroundBrush = CreateSolidBrush(RGB(255, 255, 224));
    RECT rect;
    GetClientRect(overlayWindow, &rect);
    FillRect(overlayDC, &rect, backgroundBrush);
    DeleteObject(backgroundBrush);

    SetTextColor(overlayDC, RGB(0, 0, 0));
    SetBkMode(overlayDC, TRANSPARENT);

    HFONT hFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, TEXT("Arial"));
    SelectObject(overlayDC, hFont);

    DrawText(overlayDC, text.c_str(), -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DeleteObject(hFont);
    ReleaseDC(overlayWindow, overlayDC);
}

void UpdateTrackedWindowSize()
{
    if (trackedWindow != NULL)
    {
        RECT rect;
        if (GetWindowRect(trackedWindow, &rect))
        {
            RECT clientRect;
            GetClientRect(trackedWindow, &clientRect);

            wchar_t title[256];
            GetWindowText(trackedWindow, title, sizeof(title) / sizeof(wchar_t));

            std::wstringstream ss;
            ss << L"x:" << rect.left
                << L", y:" << rect.top
                << L", w:" << rect.right - rect.left
                << L", h:" << rect.bottom - rect.top
                << L" (Client w:" << clientRect.right - clientRect.left
                << L", h:" << clientRect.bottom - clientRect.top
                << L") "; // << (wcslen(title) > 0 ? title : L"[No Title]");

            UpdateOverlayPositionAndText(overlayWindow, ss.str());
        }
    }
}

void CALLBACK TimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
    UpdateTrackedWindowSize();
}

LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT rect;
            GetClientRect(hwnd, &rect);

            wchar_t text[256];
            GetWindowText(hwnd, text, 256);

            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(0, 0, 0));
            DrawText(hdc, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            EndPaint(hwnd, &ps);
            break;
        }
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

void CALLBACK WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime)
{
    if (event == EVENT_SYSTEM_MOVESIZESTART)
    {
        trackedWindow = hwnd;
        ShowWindow(overlayWindow, SW_SHOWNOACTIVATE);
    }
    else if (event == EVENT_SYSTEM_MOVESIZEEND)
    {
        trackedWindow = NULL;
        ShowWindow(overlayWindow, SW_HIDE);
    }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    const wchar_t OVERLAY_CLASS_NAME[] = L"OverlayWindowClass";

    WNDCLASS overlayClass = {};
    overlayClass.lpfnWndProc = OverlayWndProc;
    overlayClass.hInstance = hInstance;
    overlayClass.lpszClassName = OVERLAY_CLASS_NAME;

    RegisterClass(&overlayClass);

    overlayWindow = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        OVERLAY_CLASS_NAME,
        L"",
        WS_POPUP,
        CW_USEDEFAULT, CW_USEDEFAULT, 200, 50,
        NULL,
        NULL,
        hInstance,
        NULL);

    if (!overlayWindow)
    {
        return 1;
    }

    SetLayeredWindowAttributes(overlayWindow, 0, (255 * 90) / 100, LWA_ALPHA);

    //ShowWindow(overlayWindow, SW_SHOWNOACTIVATE);

    moveSizeHook = SetWinEventHook(
        EVENT_SYSTEM_MOVESIZESTART,
        EVENT_SYSTEM_MOVESIZEEND,
        NULL,
        WinEventProc,
        0,
        0,
        WINEVENT_OUTOFCONTEXT);

    if (!moveSizeHook)
    {
        MessageBox(NULL, L"Failed to set WinEvent hook!", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    SetTimer(NULL, 1, 10, (TIMERPROC)TimerProc); // 10 ms interval

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Clean up
    UnhookWinEvent(moveSizeHook);
    KillTimer(NULL, 1); // Stop the timer
    DestroyWindow(overlayWindow);

    return 0;
}
