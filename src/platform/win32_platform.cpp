#include <windows.h>
#include <tchar.h>

static bool running = true;

LRESULT CALLBACK platform_window_callback(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch(msg)
    {
        case WM_CLOSE:
        running = false;
        break;
    }

    return DefWindowProc(window, msg, wParam, lParam);
}

bool platform_create_window(HWND window){
    HINSTANCE instance = GetModuleHandleA(0);

    WNDCLASS wc{};
    wc.lpfnWndProc = platform_window_callback;
    wc.hInstance = instance;
    wc.lpszClassName = _T("vulkan_engine_class");

    if(!RegisterClass(&wc)){
        MessageBox(window, _T("Failed registering window class"), _T("Error"), MB_ICONEXCLAMATION | MB_OK);
    }

    CreateWindowEx(
        WS_EX_APPWINDOW,
        _T("vulkan_engine_class"),
        _T("Scratch"),
        WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_OVERLAPPED, 
        100, 100, 1600, 720, 0, 0, instance, 0
    );

    if(window == 0){
         MessageBox(window, _T("Failed creating window"), _T("Error"), MB_ICONEXCLAMATION | MB_OK);
         return false;
    }

    ShowWindow(window, SW_SHOW);
    return true;
}

int main()
{
    HWND window = 0;
    if (platform_create_window(window)){
        return -1;
    }
    while (running)
    {

    }

	return 0;
}
