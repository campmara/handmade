/*
  TODO(mara): THIS IS NOT A FINAL PLATFORM LAYER!!!

  - saved game locations
  - getting a handle to our own executable file
  - asset loading path
  - threading (launch a thread)
  - raw input (support for multiple keyboards)
  - sleep / timeBeginPeriod
  - ClipCursor() (for multimonitor support)
  - fullscreen support
  - WM_SETCURSOR (control cursor visibility)
  - QueryCancelAutoplay
  - WM_ACTIVATEAPP (for when we are not the active application)
  - Blit speed improvements
  - Hardware acceleration (OpenGL or Direct3D or BOTH?)
  - GetKeyboardLayout (for French keyboards, international WASD support)

  Just a partial list of stuff!!
 */

// TODO(mara): implement sin ourselves!
#include <math.h>
#include <stdint.h>

#define global        static // truly global
#define internal      static // local to the file
#define local_persist static // local to the scope

#define PI_32 3.14159265359f

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

// Real-number types
typedef float real32;
typedef double real64;

// Booleans
typedef int32 bool32;

#include "handmade.cpp"

#include <Windows.h>
#include <stdio.h>
#include <winuser.h>
#include <xinput.h>
#include <dsound.h>

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
global LPDIRECTSOUNDBUFFER  global_secondary_buffer;

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
        xinput_library = LoadLibraryA("Xinput9_1_0.dll");
    }

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

internal void Win32InitDirectSound(HWND window, int32 samples_per_second, int32 buffer_size)
{
    // NOTE(mara): Load the library
    HMODULE direct_sound_library = LoadLibraryA("dsound.dll");

    if (direct_sound_library)
    {
        // NOTE(mara): Get a DirectSound Object! - cooperative
        direct_sound_create *DirectSoundCreate = (direct_sound_create *)GetProcAddress(direct_sound_library, "DirectSoundCreate");

        LPDIRECTSOUND direct_sound;
        if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &direct_sound, 0)))
        {
            WAVEFORMATEX wave_format = {};
            wave_format.wFormatTag = WAVE_FORMAT_PCM;
            wave_format.nChannels = 2;
            wave_format.nSamplesPerSec = samples_per_second;
            wave_format.wBitsPerSample = 16;
            wave_format.nBlockAlign = (wave_format.nChannels * wave_format.wBitsPerSample) / 8;
            wave_format.nAvgBytesPerSec = wave_format.nSamplesPerSec * wave_format.nBlockAlign;
            wave_format.cbSize = 0;

            if (SUCCEEDED(direct_sound->SetCooperativeLevel(window, DSSCL_PRIORITY)))
            {
                DSBUFFERDESC buffer_desc = {};
                buffer_desc.dwSize = sizeof(buffer_desc);
                buffer_desc.dwFlags = DSBCAPS_PRIMARYBUFFER;

                // NOTE(mara): Create a primary buffer. This is not a real buffer, it just gives us
                // a handle into the sound card in order to tell it how to handle our sound.
                LPDIRECTSOUNDBUFFER primary_buffer;
                if (SUCCEEDED(direct_sound->CreateSoundBuffer(&buffer_desc, &primary_buffer, 0)))
                {
                    HRESULT error = primary_buffer->SetFormat(&wave_format);
                    if (SUCCEEDED(error))
                    {
                        // NOTE(mara): we have finally set the format!
                        OutputDebugStringA("Primary buffer format was set.\n");
                    }
                    else
                    {
                        // TODO(mara): Diagnostic
                    }
                }
                else
                {
                    // TODO(mara): Diagnostic
                }
            }
            else
            {
                // TODO(mara): Diagnostic
            }

            // TODO(mara): DSBCAPS_GETCURRENTPOSITION2
            DSBUFFERDESC buffer_desc = {};
            buffer_desc.dwSize = sizeof(buffer_desc);
            buffer_desc.dwFlags = 0;
            buffer_desc.dwBufferBytes = buffer_size;
            buffer_desc.lpwfxFormat = &wave_format;

            // NOTE(mara): Create a secondary buffer, the buffer we can actually write into
            HRESULT error = direct_sound->CreateSoundBuffer(&buffer_desc, &global_secondary_buffer, 0);
            if (SUCCEEDED(error))
            {
                OutputDebugStringA("Secondary buffer created successfully.\n");
            }
            else
            {
                // TODO(mara): Diagnostic
            }

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
            result = DefWindowProcA(window, message, w_param, l_param);
            // OutputDebugStringA("default\n");
        } break;
    }

    return result;
}

struct Win32SoundOutput
{
    int    samples_per_second;
    int    tone_hz;
    int16  tone_volume;
    uint32 running_sample_index;
    int    wave_period;
    int    bytes_per_sample;
    int    secondary_buffer_size;
    real32 t_sine;
    int    latency_sample_count; // how many samples away from the cursor we'd like to be in general
};

void Win32FillSoundBuffer(Win32SoundOutput *sound_output, DWORD byte_to_lock, DWORD bytes_to_write)
{
    // TODO(mara): More strenuous test please!
    void *region1;
    DWORD region1_size;
    VOID *region2;
    DWORD region2_size;
    if (SUCCEEDED(global_secondary_buffer->Lock(byte_to_lock, bytes_to_write,
                                                &region1, &region1_size,
                                                &region2, &region2_size,
                                                0)))
    {
        DWORD region1_sample_count = region1_size / sound_output->bytes_per_sample;
        int16 *sample_out = (int16 *)region1;
        for (DWORD sample_index = 0; sample_index < region1_sample_count; ++sample_index)
        {
            real32 sine_value = sinf(sound_output->t_sine);
            int16 sample_value = (int16)(sine_value * sound_output->tone_volume); // scale up to the volume hz
            *sample_out++ = sample_value;
            *sample_out++ = sample_value;

            sound_output->t_sine += 2.0f * PI_32 * 1.0f / (real32)sound_output->wave_period;
            ++sound_output->running_sample_index;
        }

        DWORD region2_sample_count = region2_size / sound_output->bytes_per_sample;
        sample_out = (int16 *)region2;
        for (DWORD sample_index = 0; sample_index < region2_sample_count; ++sample_index)
        {
            real32 sine_value = sinf(sound_output->t_sine);
            int16 sample_value = (int16)(sine_value * sound_output->tone_volume); // scale up to the volume hz
            *sample_out++ = sample_value;
            *sample_out++ = sample_value;

            sound_output->t_sine += 2.0f * PI_32 * 1.0f / (real32)sound_output->wave_period;
            ++sound_output->running_sample_index;
        }

        global_secondary_buffer->Unlock(region1, region1_size,
                region2, region2_size);

    }
}

void Win32ChangeSoundTone(Win32SoundOutput *sound_output, int new_hz)
{
    sound_output->tone_hz = new_hz;
    sound_output->wave_period = sound_output->samples_per_second / sound_output->tone_hz;
}

int CALLBACK WinMain(HINSTANCE Instance,
                     HINSTANCE PrevInstance,
                     LPSTR CommandLine,
                     int ShowCode)
{
    LARGE_INTEGER perf_count_frequency_result;
    QueryPerformanceFrequency(&perf_count_frequency_result);
    int64 perf_count_frequency = perf_count_frequency_result.QuadPart;

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

            // NOTE(mara): graphics test
            int x_offset = 0;
            int y_offset = 0;

            // TODO(mara): make this 60 seconds?
            Win32SoundOutput sound_output = {};
            sound_output.samples_per_second = 48000;
            sound_output.tone_hz = 256;
            sound_output.tone_volume = 1000;
            sound_output.running_sample_index = 0;
            sound_output.wave_period = sound_output.samples_per_second / sound_output.tone_hz;
            sound_output.bytes_per_sample = sizeof(int16) * 2;
            sound_output.secondary_buffer_size = sound_output.samples_per_second * sound_output.bytes_per_sample;
            sound_output.latency_sample_count = sound_output.samples_per_second / 15;
            sound_output.t_sine = 0;

            Win32InitDirectSound(window, sound_output.samples_per_second, sound_output.secondary_buffer_size);
            Win32FillSoundBuffer(&sound_output, 0, sound_output.latency_sample_count * sound_output.secondary_buffer_size);
            global_secondary_buffer->Play(0, 0, DSBPLAY_LOOPING);

            // Most recent frame clock time
            uint64 last_cycle_count = __rdtsc();
            LARGE_INTEGER last_counter;
            QueryPerformanceCounter(&last_counter);

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
                        bool32 dpad_up = (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
                        bool32 dpad_down = (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
                        bool32 dpad_left = (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
                        bool32 dpad_right = (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
                        bool32 start = (pad->wButtons & XINPUT_GAMEPAD_START);
                        bool32 back = (pad->wButtons & XINPUT_GAMEPAD_BACK);
                        bool32 left_shoulder = (pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
                        bool32 right_shoulder = (pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
                        bool32 a_button = (pad->wButtons & XINPUT_GAMEPAD_A);
                        bool32 b_button = (pad->wButtons & XINPUT_GAMEPAD_B);
                        bool32 x_button = (pad->wButtons & XINPUT_GAMEPAD_X);
                        bool32 y_button = (pad->wButtons & XINPUT_GAMEPAD_Y);

                        int16 stick_x = pad->sThumbLX;
                        int16 stick_y = pad->sThumbLY;

                        //TODO(mara): we will do deadzone handling later properly using
                        // XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE and
                        // XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE

                        // Kill the controller noise at deadzones
                        x_offset += stick_x / 4096;
                        y_offset -= stick_y / 4096;

                        sound_output.tone_hz = 512 + (int)(256.0f * ((real32)stick_y / 30000.0f));
                        sound_output.wave_period = sound_output.samples_per_second / sound_output.tone_hz;
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

                GameSoundOutputBuffer sound_buffer = {};

                GameOffscreenBuffer offscreen_buffer = {};
                offscreen_buffer.memory = global_backbuffer.memory;
                offscreen_buffer.width = global_backbuffer.width;
                offscreen_buffer.weight = global_backbuffer.height;
                offscreen_buffer.pitch = global_backbuffer.pitch;
                GameUpdateAndRender(&offscreen_buffer, x_offset, y_offset);

                // NOTE(mara): DirectSound output test
                DWORD play_cursor;
                DWORD write_cursor;
                if (SUCCEEDED(global_secondary_buffer->GetCurrentPosition(&play_cursor, &write_cursor)))
                {
                    DWORD byte_to_lock = (sound_output.running_sample_index * sound_output.bytes_per_sample) % sound_output.secondary_buffer_size;

                    DWORD target_cursor = ((play_cursor +
                                            (sound_output.latency_sample_count * sound_output.bytes_per_sample)) %
                                           sound_output.secondary_buffer_size);
                    DWORD bytes_to_write; // number of bytes total to write into the buffer.

                    if (byte_to_lock > target_cursor)
                    {
                        bytes_to_write = (sound_output.secondary_buffer_size - byte_to_lock);
                        bytes_to_write += target_cursor;
                    }
                    else
                    {
                        bytes_to_write = target_cursor - byte_to_lock;
                    }

                    Win32FillSoundBuffer(&sound_output, byte_to_lock, bytes_to_write);
                }

                Win32WindowDimensions dimensions = GetWindowDimensions(window);
                Win32DisplayBufferInWindow(&global_backbuffer,
                                           device_context,
                                           dimensions.width,
                                           dimensions.height);

                // Counter that tracks the end of every frame
                uint64 end_cycle_count = __rdtsc();
                LARGE_INTEGER end_counter;
                QueryPerformanceCounter(&end_counter);

                uint64 cycles_elapsed = end_cycle_count - last_cycle_count;
                int64 counter_elapsed = end_counter.QuadPart - last_counter.QuadPart;
                real64 ms_per_frame = ((1000.0 * (real64)counter_elapsed) / (real64)perf_count_frequency);
                real64 fps = (real64)perf_count_frequency / (real64)counter_elapsed;
                real64 mcpf = ((real64)cycles_elapsed / (1000.0 * 1000.0));

if 0
                char buffer[256];
                sprintf(buffer, "| %.02fms/f | %.02ff/s | %.02fmc/f |\n", ms_per_frame, fps, mcpf);
                OutputDebugStringA(buffer);
#endif

                last_counter = end_counter;
                last_cycle_count = end_cycle_count;
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
