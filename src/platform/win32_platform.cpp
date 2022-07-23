#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include "defines.h"
#include "platform.h"
#include "renderer/vk_renderer.cpp"

global_variable bool running = true;
global_variable HWND window;

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

bool platform_create_window()
{
    HINSTANCE instance = GetModuleHandle(nullptr);

    WNDCLASS wc{};
    wc.lpfnWndProc = platform_window_callback;
    wc.hInstance = instance;
    wc.lpszClassName = L"vulkan_engine_class";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    if (!RegisterClass(&wc))
    {
        MessageBox(nullptr, L"Failed registering window class", L"Error", MB_ICONEXCLAMATION | MB_OK);
        return false;
    }

    window = CreateWindowEx(
        WS_EX_APPWINDOW,
        L"vulkan_engine_class",
        L"Scratch 짜잔",
        WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_OVERLAPPED,
        100, 100, 1600, 720, 0, 0, instance, nullptr);

    if (window == nullptr)
    {
        MessageBox(nullptr, L"Failed creating window", L"Error", MB_ICONEXCLAMATION | MB_OK);
        return false;
    }

    ShowWindow(window, SW_SHOW);

    return true;
}

void platform_update_window(HWND window)
{
    MSG msg;

    while (PeekMessage(&msg, window, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

int main()
{
    VkContext vkcontext = {};

    if (!platform_create_window())
    {
        return -1;
    }

    if (!vk_init(&vkcontext, window))
    {
        return -1;
    }

    while (running)
    {
        platform_update_window(window);
        if (!vk_render(&vkcontext))
        {
            return -1;
        }

    }

    return 0;
}

void platform_get_window_size(uint32_t* width, uint32_t* height)
{
    RECT rect;
    GetClientRect(window, &rect);

    *width = rect.right - rect.left;
    *height = rect.bottom - rect.top;
}

char* platform_read_file(LPCWSTR path, uint32_t* length)
{
    char* result = nullptr;

    HANDLE file = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if(file != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER size;
        if (GetFileSizeEx(file, &size))
        {
            *length = static_cast<uint32_t>(size.QuadPart);
            result = new char[*length];

            DWORD bytesRead;
            if (ReadFile(file, result, *length, &bytesRead, nullptr))
            {
                // Success
            }
            else 
            {
                std::cout << "Failed reading file" << std::endl;
            }
        }
        else
        {
            std::cout << "Failed getting size of file" << std::endl;    
        }

        CloseHandle(file);
    }
    else
    {
        std::cout << "Failed opening file" << std::endl;
    }

    return result;
}