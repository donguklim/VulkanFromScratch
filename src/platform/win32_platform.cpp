#ifndef UNICODE
#define UNICODE
#endif 


#include <windows.h>

static bool running = true;

LRESULT CALLBACK platform_window_callback(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CLOSE:
        running = false;
        break;
    }

    return DefWindowProc(window, msg, wParam, lParam);
}

bool platform_create_window(HWND window)
{
    HINSTANCE instance = GetModuleHandle(0);

    WNDCLASSW wc = {};
    wc.lpfnWndProc = platform_window_callback;
    wc.hInstance = instance;
    wc.lpszClassName = L"vulkan_engine_class";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    if (!RegisterClass(&wc))
    {
        MessageBox(window, L"Failed registering window class", L"Error", MB_ICONEXCLAMATION | MB_OK);
        return false;
    }

    window = CreateWindowEx(
        WS_EX_APPWINDOW,
        L"vulkan_engine_class",
        L"Scratch 짜잔",
        WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_OVERLAPPED,
        100, 100, 1600, 720, 0, 0, instance, 0);

    if (window == 0)
    {
        MessageBox(window, L"Failed creating window", L"Error", MB_ICONEXCLAMATION | MB_OK);
        return false;
    }

    ShowWindow(window, SW_SHOW);
    return true;
}

int main()
{
    
    HWND window = 0;
    if (!platform_create_window(window))
    {
        return -1;
    }


    while (running)
    {

    }

    return 0;
}