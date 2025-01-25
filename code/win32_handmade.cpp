#include <Windows.h>
#include <stdint.h>
#include <winuser.h>
#include <xinput.h>
#include <dsound.h>

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

// Booleans
typedef int32 bool32;

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

// NOTE(mara): XInputGetState
#define XINPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef XINPUT_GET_STATE(xinput_get_state);

XINPUT_GET_STATE(XInputGetStateStub)
{
    return(ERROR_DEVICE_NOT_CONNECTED);
}
global xinput_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// NOTE(mara): XInputSetState
#define XINPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef XINPUT_SET_STATE(xinput_set_state);
XINPUT_SET_STATE(XInputSetStateStub)
{
    return(ERROR_DEVICE_NOT_CONNECTED);
}
global xinput_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

internal void Win32LoadXInput(void)
{
    // TODO(mara): Test this on Windows 7
    HMODULE xinput_library = LoadLibraryA("xinput1_4.dll");
    if (!xinput_library)
    {
        //TODO(mara): Diagnostic
        xinput_library = LoadLibraryA("xinput1_3.dll");
    }

    if (xinput_library)
    {
        XInputGetState = (xinput_get_state *)GetProcAddress(xinput_library, "XInputGetState");
        if (!XInputGetState) { XInputGetState = XInputGetStateStub; }
        XInputSetState = (xinput_set_state *)GetProcAddress(xinput_library, "XInputSetState");
        if (!XInputSetState) { XInputSetState = XInputSetStateStub; }
    }
    else
    {
        // TODO(mara): Diagnostic
    }
}

internal void Win32InitDirectSound(HWND window)
{
    // NOTE(mara): Load the library
    HMODULE direct_sound_library = LoadLibraryA("dsound.dll");

    if (direct_sound_library)
    {
        // NOTE(mara): Get a DirectSound Object! - cooperative
        direct_sound_create *DirectSoundCreate = 
            (direct_sound_create *)GetProcAddress(direct_sound_library, "DirectSoundCreate");

        LPDIRECTSOUND direct_sound;
        if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &direct_sound, 0)))
        {
            if (SUCCEEDED(direct_sound->SetCooperativeLevel(window, DSSCL_PRIORITY)))
            {
                DSBUFFERDESC buffer_desc = {sizeof(buffer_desc)};
                //buffer_desc.
                LPDIRECTSOUNDBUFFER primary_buffer;
                if (SUCCEEDED(direct_sound->CreateSoundBuffer(&buffer_desc, &primary_buffer, 0)))
                {

                }
            }
            else
            {
                // TODO(mara): Diagnostic
            }
            // NOTE(mara): Create a primary buffer

            // NOTE(mara): Create a secondary buffer

            // NOTE(mara): Start playing sound!
        }
        else
        {
            // TODO(mara): Diagnostic
        }
    }
    else
    {
        // TODO(mara) Diagnostic
    }
}

internal Win32WindowDimensions GetWindowDimensions(HWND window)
{
    Win32WindowDimensions result;

    RECT client_rect;
    GetClientRect(window, &client_rect);
    result.width = client_rect.right - client_rect.left;
    result.height = client_rect.bottom - client_rect.top;

    return result;
}

internal void RenderWeirdGradient(Win32OffscreenBuffer *buffer, int blue_offset, int green_offset)
{
    // read the bitmap memory in a form we understand, not just void *
    uint8 *row = (uint8 *)buffer->memory;
    for (int y = 0; y < buffer->height; ++y)
    {
        uint32 *pixel = (uint32 *)row;
        for (int x = 0; x < buffer->width; ++x)
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

        row += buffer->pitch;
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

internal void Win32DisplayBufferInWindow(Win32OffscreenBuffer *buffer,
                                         HDC device_context,
                                         int window_width,
                                         int window_height)
{
    // TODO(mara): aspect ratio correction
    // TODO(mara): play with stretch modes
    StretchDIBits(device_context,
                  0, 0, window_width, window_height, // dest
                  0, 0, buffer->width, buffer->height, // src
                  buffer->memory,
                  &buffer->info,
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

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            // NOTE(mara): the vk code will always come down through the WPARAM
            uint32 vk_code = w_param;
#define KEY_MESSAGE_WAS_DOWN_BIT (1 << 30)
#define KEY_MESSAGE_IS_DOWN_BIT (1 << 31)
            bool was_down = ((l_param & KEY_MESSAGE_WAS_DOWN_BIT) != 0);
            bool is_down = ((l_param & KEY_MESSAGE_IS_DOWN_BIT) == 0);
            if (was_down != is_down)
            {
                if (vk_code == 'W')
                {
                }
                else if (vk_code == 'A')
                {
                }
                else if (vk_code == 'S')
                {
                }
                else if (vk_code == 'D')
                {
                }
                else if (vk_code == 'Q')
                {
                }
                else if (vk_code == 'E')
                {
                }
                else if (vk_code == VK_UP)
                {
                }
                else if (vk_code == VK_LEFT)
                {
                }
                else if (vk_code == VK_DOWN)
                {
                }
                else if (vk_code == VK_RIGHT)
                {
                }
                else if (vk_code == VK_ESCAPE)
                {
                    OutputDebugStringA("ESCAPE: ");
                    if (is_down)
                    {
                        OutputDebugStringA("is_down ");
                    }
                    if (was_down)
                    {
                        OutputDebugStringA("was_down ");
                    }
                    OutputDebugStringA("\n");
                }
                else if (vk_code == VK_SPACE)
                {
                }
            }

            bool32 alt_key_was_down = (l_param & (1 << 29));
            if (vk_code == VK_F4 && alt_key_was_down)
            {
                global_is_running = false;
            }
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

            Win32DisplayBufferInWindow(&global_backbuffer,
                                       device_context,
                                       dimensions.width,
                                       dimensions.height);
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
    Win32LoadXInput();

    WNDCLASSA window_class = {};

    Win32ResizeDIBSection(&global_backbuffer, 1280, 720);

    window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    window_class.lpfnWndProc = Win32MainWindowCallback;
    window_class.hInstance = Instance;
    window_class.lpszClassName = "HandmadeHeroWindowClass";

    if (RegisterClassA(&window_class))
    {
        HWND window = CreateWindowExA(0,
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
            HDC device_context = GetDC(window);

            int x_offset = 0;
            int y_offset = 0;

            Win32InitDirectSound(window);

            global_is_running = true;
            while (global_is_running)
            {
                MSG message;
                while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE))
                {
                    if (message.message == WM_QUIT)
                    {
                        global_is_running = false;
                    }
                    TranslateMessage(&message);
                    DispatchMessageA(&message);
                }

                // TODO(mara): Should we poll this more frequently?
                for (DWORD controller_idx = 0; controller_idx < XUSER_MAX_COUNT; ++controller_idx)
                {
                    XINPUT_STATE controller_state;
                    if (XInputGetState(controller_idx, &controller_state) == ERROR_SUCCESS)
                    {
                        // NOTE(mara): This controller is plugged in
                        // TODO(mara): See if controller_state.dwPacketNumber increments
                        XINPUT_GAMEPAD *pad = &controller_state.Gamepad;
                        bool dpad_up = (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
                        bool dpad_down = (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
                        bool dpad_left = (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
                        bool dpad_right = (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
                        bool start = (pad->wButtons & XINPUT_GAMEPAD_START);
                        bool back = (pad->wButtons & XINPUT_GAMEPAD_BACK);
                        bool left_shoulder = (pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
                        bool right_shoulder = (pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
                        bool a_button = (pad->wButtons & XINPUT_GAMEPAD_A);
                        bool b_button = (pad->wButtons & XINPUT_GAMEPAD_B);
                        bool x_button = (pad->wButtons & XINPUT_GAMEPAD_X);
                        bool y_button = (pad->wButtons & XINPUT_GAMEPAD_Y);

                        int16 stick_x = pad->sThumbLX;
                        int16 stick_y = pad->sThumbLY;

                        x_offset += stick_x >> 12;
                        y_offset -= stick_y >> 12;
                    }
                    else
                    {
                        // NOTE(mara): The controller is not available
                        
                    }
                }

                /*XINPUT_VIBRATION vibration;
                vibration.wLeftMotorSpeed = 6000;
                vibration.wRightMotorSpeed = 6000;
                XInputSetState(0, &vibration);*/

                RenderWeirdGradient(&global_backbuffer, x_offset, y_offset);

                Win32WindowDimensions dimensions = GetWindowDimensions(window);
                Win32DisplayBufferInWindow(&global_backbuffer,
                                           device_context,
                                           dimensions.width,
                                           dimensions.height);
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
