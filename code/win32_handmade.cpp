#include <Windows.h>
#include <stdint.h>
#include <winuser.h>

#define global        static // truly global
#define internal      static // local to the file
#define local_persist static // local to the scope

// Unsigned integer types
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

// Signed integer types
typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

struct Win32OffscreenBuffer
{
    BITMAPINFO  info;
    void       *memory;
    HBITMAP     handle;
    HDC         device_context;
    int         width;
    int         height;
    int         pitch;
    int         bytes_per_pixel;
};

struct Win32WindowDimensions
{
    int width;
    int height;
};

// TODO(mara): These are globals for now
global bool                 global_is_running;
global Win32OffscreenBuffer global_backbuffer;

Win32WindowDimensions GetWindowDimensions(HWND window)
{
    Win32WindowDimensions result;

    RECT client_rect;
    GetClientRect(window, &client_rect);
    result.width = client_rect.right - client_rect.left;
    result.height = client_rect.bottom - client_rect.top;

    return result;
}

internal void RenderWeirdGradient(Win32OffscreenBuffer buffer, int blue_offset, int green_offset)
{
    // read the bitmap memory in a form we understand, not just void *
    uint8 *row = (uint8 *)buffer.memory;
    for (int y = 0; y < buffer.height; ++y)
    {
        uint32 *pixel = (uint32 *)row;
        for (int x = 0; x < buffer.width; ++x)
        {
            /*
              Memory:   BB GG RR xx
              Register: xx RR GG BB

              LITTLE ENDIAN ARCHITECTURE! REMEMBER IT'S BACKWARDS IF WE'RE DEALING WITH BYTES
              Windows programmers wanted it to look right in the hex values, so it looks like this:

              0x xxRRGGBB
             */
            uint8 blue = (x + blue_offset);
            uint8 green = (y + green_offset);
            *pixel++ = ((green << 8) | blue);
        }

        row += buffer.pitch;
    }
}

internal void Win32ResizeDIBSection(Win32OffscreenBuffer *buffer, int width, int height)
{
    // TODO(mara): Bulletproof this
    // Maybe don't free first, free after, then free first if that fails

    if (buffer->memory)
    {
        VirtualFree(buffer->memory, 0, MEM_RELEASE);
    }

    buffer->width = width;
    buffer->height = height;
    buffer->bytes_per_pixel = 4;

    // NOTE(mara): when the biHeight field is negative, this is the clue to Windows to treat this 
    // bitmap as top-down, not bottom-up, meaning that the first three bytes of the image are the 
    // color for the top left pixel in the bitmap, not the bottom left!
    buffer->info.bmiHeader.biSize = sizeof(buffer->info.bmiHeader);
    buffer->info.bmiHeader.biWidth = buffer->width;
    buffer->info.bmiHeader.biHeight = -buffer->height;
    buffer->info.bmiHeader.biPlanes = 1;
    buffer->info.bmiHeader.biBitCount = 32; // 8 bits for each RGB, padding for DWORD-alignment.
    buffer->info.bmiHeader.biCompression = BI_RGB;

    // No more DC for us.
    int bitmap_memory_size = (buffer->width * buffer->height) * buffer->bytes_per_pixel;

    buffer->memory = VirtualAlloc(0, bitmap_memory_size, MEM_COMMIT, PAGE_READWRITE);

    // TODO(mara): Clear this to black

    buffer->pitch = width * buffer->bytes_per_pixel;
}

internal void Win32DisplayBufferInWindow(HDC device_context,
                                         int window_width,
                                         int window_height,
                                         Win32OffscreenBuffer buffer)
{
    // TODO(mara): aspect ratio correction
    // TODO(mara): play with stretch modes
    StretchDIBits(device_context,
                  0, 0, window_width, window_height, // dest
                  0, 0, buffer.width, buffer.height, // src
                  buffer.memory,
                  &buffer.info,
                  DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK Win32MainWindowCallback(HWND window,
                                         UINT message,
                                         WPARAM w_param,
                                         LPARAM l_param)
{
    // Set to 0 and assume that we handled all the messages that we cared about
    LRESULT result = 0;

    switch (message)
    {
        case WM_SIZE:
        {
        } break;

        case WM_ACTIVATEAPP:
        {
            OutputDebugStringA("WM_ACTIVATEAPP\n");
        } break;

        case WM_CLOSE:
        {
            // TODO(mara): Handle this with a message to the user?
            global_is_running = false;
        } break;

        case WM_DESTROY:
        {
            // TODO(mara): Handle this as an error, recreate window?
            global_is_running = false;
        } break;

        case WM_PAINT:
        {
            PAINTSTRUCT paint;

            HDC device_context = BeginPaint(window, &paint);
            int x = paint.rcPaint.left;
            int y = paint.rcPaint.top;
            int width = paint.rcPaint.right - paint.rcPaint.left;
            int height = paint.rcPaint.bottom - paint.rcPaint.top;

            Win32WindowDimensions dimensions = GetWindowDimensions(window);

            Win32DisplayBufferInWindow(device_context, dimensions.width, dimensions.height,
                                       global_backbuffer);
            EndPaint(window, &paint);
        } break;

        default:
        {
            result = DefWindowProc(window, message, w_param, l_param);
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

    Win32ResizeDIBSection(&global_backbuffer, 1280, 720);

    window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    window_class.lpfnWndProc = Win32MainWindowCallback;
    window_class.hInstance = Instance;
    window_class.lpszClassName = "HandmadeHeroWindowClass";

    if (RegisterClass(&window_class))
    {
        HWND window = CreateWindowEx(0,
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

        if (window)
        {
            // NOTE(mara): Since we specified CS_OWNDC, we can just get one device context and use
            // it forever because we are not sharing it with anyone
            int x_offset = 0;
            int y_offset = 0;

            global_is_running = true;
            while (global_is_running)
            {
                MSG message;
                while (PeekMessage(&message, 0, 0, 0, PM_REMOVE))
                {
                    if (message.message == WM_QUIT)
                    {
                        global_is_running = false;
                    }
                    TranslateMessage(&message);
                    DispatchMessage(&message);
                }

                RenderWeirdGradient(global_backbuffer, x_offset, y_offset);

                HDC device_context = GetDC(window);
                Win32WindowDimensions dimensions = GetWindowDimensions(window);
                Win32DisplayBufferInWindow(device_context, dimensions.width, dimensions.height,
                                           global_backbuffer);
                ReleaseDC(window, device_context);

                ++x_offset;
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
