#include <Windows.h>

#define global        static // truly global
#define internal      static // local to the file
#define local_persist static // local to the scope

// TODO(mara): This is a global for now
global bool       app_is_running;
global BITMAPINFO bitmap_info;
global void      *bitmap_memory;
global HBITMAP    bitmap_handle;
global HDC        bitmap_device_context;

internal void Win32ResizeDIBSection(int width, int height)
{
    // TODO(mara): Bulletproof this.
    // Maybe don't free first, free after, then free first if that fails.

    if (bitmap_handle)
    {
        DeleteObject(bitmap_handle);
    }

    if (!bitmap_device_context)
    {
        // TODO(mara): Should we recreate these under special certain circumstances?
        bitmap_device_context = CreateCompatibleDC(0);
    }

    bitmap_info.bmiHeader.biSize = sizeof(bitmap_info.bmiHeader);
    bitmap_info.bmiHeader.biWidth = width;
    bitmap_info.bmiHeader.biHeight = height;
    bitmap_info.bmiHeader.biPlanes = 1;
    bitmap_info.bmiHeader.biBitCount = 32; // 8 bits for each RGB, but extra is for DWORD-alignment.
    bitmap_info.bmiHeader.biCompression = BI_RGB;

    bitmap_handle = CreateDIBSection(bitmap_device_context, 
                                     &bitmap_info,
                                     DIB_RGB_COLORS, 
                                     &bitmap_memory,
                                     0, 0);
}

internal void Win32UpdateWindow(HDC device_context, int x, int y, int width, int height)
{   
    StretchDIBits(device_context, 
                  x, y, width, height, 
                  x, y, width, height, 
                  bitmap_memory, 
                  &bitmap_info,
                  DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK Win32MainWindowCallback(HWND Window,
                                    UINT Message,
                                    WPARAM WParam,
                                    LPARAM LParam)
{
    LRESULT result = 0; // Set to 0 and assume that we handled all the messages that we cared about.

    switch (Message)
    {
        case WM_SIZE:
        {
            RECT client_rect;
            GetClientRect(Window, &client_rect);
            int width = client_rect.right - client_rect.left;
            int height = client_rect.bottom - client_rect.top;
            Win32ResizeDIBSection(width, height);
            OutputDebugStringA("WM_SIZE\n");
        } break;

        case WM_ACTIVATEAPP:
        {
            OutputDebugStringA("WM_ACTIVATEAPP\n");
        } break;

        case WM_CLOSE:
        {
            // TODO(mara): Handle this with a message to the user?
            app_is_running = false;
        } break;

        case WM_DESTROY:
        {
            // TODO(mara): Handle this as an error, recreate window?
            app_is_running = false;
        } break;

        case WM_PAINT:
        {
            PAINTSTRUCT paint;

            HDC device_context = BeginPaint(Window, &paint);
            int x = paint.rcPaint.left;
            int y = paint.rcPaint.top;
            int width = paint.rcPaint.right - paint.rcPaint.left;
            int height = paint.rcPaint.bottom - paint.rcPaint.top;
            Win32UpdateWindow(device_context, x, y, width, height);
            EndPaint(Window, &paint);
        } break;

        default:
        {
            result = DefWindowProc(Window, Message, WParam, LParam);
            // OutputDebugStringA("default\n");
        } break;
    }

    return result;
}

int CALLBACK WinMain(HINSTANCE Instance,
                     HINSTANCE PrevInstance,
                     LPSTR CommandLine,
                     int ShowCode)
{
    WNDCLASS window_class = {};
    window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    window_class.lpfnWndProc = Win32MainWindowCallback;
    window_class.hInstance = Instance;
    window_class.lpszClassName = "HandmadeHeroWindowClass";

    if (RegisterClass(&window_class))
    {
        HWND window_handle = CreateWindowEx(0, 
                                            window_class.lpszClassName,
                                            "Handmade Hero",
                                            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                            CW_USEDEFAULT,
                                            CW_USEDEFAULT,
                                            CW_USEDEFAULT,
                                            CW_USEDEFAULT,
                                            0,
                                            0,
                                            Instance,
                                            0);

        if (window_handle)
        {
            app_is_running = true;
            while (app_is_running)
            {
                MSG message;
                BOOL message_result = GetMessage(&message, 0, 0, 0);
                if (message_result > 0)
                {
                    TranslateMessage(&message);
                    DispatchMessage(&message);
                }
                else
                {
                    break;
                }
            }
        }
        else
        {
            // TODO(mara): Logging here
        }
    }
    else
    {
        // TODO(mara): Logging here
    }

    return(0);
}
