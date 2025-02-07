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

#include "handmade.h"
#include "handmade.cpp"

#include <Windows.h>
#include <stdio.h>
#include <malloc.h>
#include <xinput.h>
#include <dsound.h>

#include "win32_handmade.h"

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

internal DEBUGReadFileResult DEBUGPlatformReadEntireFile(char *filename)
{
    DEBUGReadFileResult result = {};

    HANDLE file_handle = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);

    if (file_handle != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER file_size;
        if (GetFileSizeEx(file_handle, &file_size))
        {
            uint32 file_size_32 = SafeTruncateUInt64(file_size.QuadPart);
            result.content = VirtualAlloc(0, file_size_32, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            if (result.content)
            {
                DWORD bytes_read;
                if (ReadFile(file_handle, result.content, file_size_32, &bytes_read, 0) &&
                    (file_size_32 == bytes_read))
                {
                    // NOTE(mara): File read successfully.
                    result.content_size = file_size_32;
                }
                else
                {
                    // TODO(mara): Logging
                    DEBUGPlatformFreeFileMemory(result.content);
                    result.content = 0;
                }
            }
            else
            {
                // TODO(mara): Logging
            }
        }
        else
        {
            // TODO(mara): Logging
        }

        CloseHandle(file_handle);
    }
    else
    {
        // TODO(mara): Logging
    }

    return result;
}

internal void DEBUGPlatformFreeFileMemory(void *memory)
{
    if (memory)
    {
        VirtualFree(memory, 0, MEM_RELEASE);
    }
}

internal bool32 DEBUGPlatformWriteEntireFile(char *filename, uint32 memory_size, void *memory)
{
    bool32 result = false;

    HANDLE file_handle = CreateFileA(filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);

    if (file_handle != INVALID_HANDLE_VALUE)
    {
        DWORD bytes_written;
        if (WriteFile(file_handle, memory, memory_size, &bytes_written, 0))
        {
            // NOTE(mara): File written successfully.
            result = (bytes_written == memory_size);
        }
        else
        {
            // TODO(mara): Logging
        }

        CloseHandle(file_handle);
    }
    else
    {
        // TODO(mara): Logging
    }

    return result;
}

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
            Assert(!"Keyboard input came through a non-dispatch message!");
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

void Win32ClearSoundBuffer(Win32SoundOutput *sound_output)
{
    void *region1;
    DWORD region1_size;
    VOID *region2;
    DWORD region2_size;
    if (SUCCEEDED(global_secondary_buffer->Lock(0, sound_output->secondary_buffer_size,
                                                &region1, &region1_size,
                                                &region2, &region2_size,
                                                0)))
    {
        uint8 *dest_byte = (uint8 *)region1;
        for (DWORD byte_index = 0; byte_index < region1_size; ++byte_index)
        {
            *dest_byte++ = 0;
        }

        dest_byte = (uint8 *)region2;
        for (DWORD byte_index = 0; byte_index < region2_size; ++byte_index)
        {
            *dest_byte++ = 0;
        }

        global_secondary_buffer->Unlock(region1, region1_size, region2, region2_size);
    }
}

void Win32FillSoundBuffer(Win32SoundOutput *sound_output,
                          DWORD byte_to_lock,
                          DWORD bytes_to_write,
                          GameSoundOutputBuffer *source_buffer)
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
        int16 *dest_sample = (int16 *)region1;
        int16 *source_sample = source_buffer->samples;
        for (DWORD sample_index = 0; sample_index < region1_sample_count; ++sample_index)
        {
            *dest_sample++ = *source_sample++;
            *dest_sample++ = *source_sample++;
            ++sound_output->running_sample_index;
        }

        DWORD region2_sample_count = region2_size / sound_output->bytes_per_sample;
        dest_sample = (int16 *)region2;
        for (DWORD sample_index = 0; sample_index < region2_sample_count; ++sample_index)
        {
            *dest_sample++ = *source_sample++;
            *dest_sample++ = *source_sample++;
            ++sound_output->running_sample_index;
        }

        global_secondary_buffer->Unlock(region1, region1_size, region2, region2_size);
    }
}

internal void Win32ProcessKeyboardMessage(GameButtonState *new_state,
                                          bool32 is_down)
{
    Assert(new_state->ended_down != is_down);
    new_state->ended_down = is_down;
    ++new_state->half_transition_count;
}

internal void Win32ProcessXInputDigitalButton(DWORD xinput_button_state, DWORD button_bit,
                                              GameButtonState *prev_state,
                                              GameButtonState *new_state)
{
    new_state->ended_down = ((xinput_button_state & button_bit) == button_bit);
    new_state->half_transition_count = (prev_state->ended_down != new_state->ended_down) ? 1 : 0;
}

internal real32 Win32ProcessXInputStickValue(SHORT value, SHORT deadzone_threshold)
{
    real32 result = 0;
    // Treat the deadzone boundary as the 0 point for stick input.
    if (value < -deadzone_threshold)
    {
        result = (real32)((value + deadzone_threshold) / (32768.0f - deadzone_threshold));
    }
    else if (value > deadzone_threshold)
    {
        result = (real32)((value - deadzone_threshold) / (32768.0f - deadzone_threshold));
    }
    return result;
}

internal void Win32ProcessPendingMessages(GameControllerInput *keyboard_controller)
{
    MSG message;
    while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE))
    {
        if (message.message == WM_QUIT)
        {
            global_is_running = false;
        }

        switch(message.message)
        {
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP:
            {
                // NOTE(mara): the vk code will always come down through the WPARAM
                uint32 vk_code = (uint32)message.wParam; // VK_Codes are not 64-bit, so this truncation is ok.
#define KEY_MESSAGE_WAS_DOWN_BIT (1 << 30)
#define KEY_MESSAGE_IS_DOWN_BIT (1 << 31)
                bool was_down = ((message.lParam & KEY_MESSAGE_WAS_DOWN_BIT) != 0);
                bool is_down = ((message.lParam & KEY_MESSAGE_IS_DOWN_BIT) == 0);
                if (was_down != is_down)
                {
                    if (vk_code == 'W')
                    {
                        Win32ProcessKeyboardMessage(&keyboard_controller->move_up, is_down);
                    }
                    else if (vk_code == 'A')
                    {
                        Win32ProcessKeyboardMessage(&keyboard_controller->move_left, is_down);
                    }
                    else if (vk_code == 'S')
                    {
                        Win32ProcessKeyboardMessage(&keyboard_controller->move_down, is_down);
                    }
                    else if (vk_code == 'D')
                    {
                        Win32ProcessKeyboardMessage(&keyboard_controller->move_right, is_down);
                    }
                    else if (vk_code == 'Q')
                    {
                        Win32ProcessKeyboardMessage(&keyboard_controller->left_shoulder, is_down);
                    }
                    else if (vk_code == 'E')
                    {
                        Win32ProcessKeyboardMessage(&keyboard_controller->right_shoulder, is_down);
                    }
                    else if (vk_code == VK_UP)
                    {
                        Win32ProcessKeyboardMessage(&keyboard_controller->action_up, is_down);
                    }
                    else if (vk_code == VK_LEFT)
                    {
                        Win32ProcessKeyboardMessage(&keyboard_controller->action_left, is_down);
                    }
                    else if (vk_code == VK_DOWN)
                    {
                        Win32ProcessKeyboardMessage(&keyboard_controller->action_down, is_down);
                    }
                    else if (vk_code == VK_RIGHT)
                    {
                        Win32ProcessKeyboardMessage(&keyboard_controller->action_right, is_down);
                    }
                    else if (vk_code == VK_ESCAPE)
                    {
                        Win32ProcessKeyboardMessage(&keyboard_controller->start, is_down);
                    }
                    else if (vk_code == VK_SPACE)
                    {
                        Win32ProcessKeyboardMessage(&keyboard_controller->back, is_down);
                    }
                }

                bool32 alt_key_was_down = (message.lParam & (1 << 29));
                if (vk_code == VK_F4 && alt_key_was_down)
                {
                    global_is_running = false;
                }
            } break;
            default:
            {
                TranslateMessage(&message);
                DispatchMessageA(&message);
            } break;
        }
    }
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

            // TODO(mara): make this 60 seconds?
            Win32SoundOutput sound_output = {};
            sound_output.samples_per_second = 48000;
            sound_output.running_sample_index = 0;
            sound_output.bytes_per_sample = sizeof(int16) * 2;
            sound_output.secondary_buffer_size = sound_output.samples_per_second * sound_output.bytes_per_sample;
            sound_output.latency_sample_count = sound_output.samples_per_second / 15;
            sound_output.t_sine = 0;

            Win32InitDirectSound(window, sound_output.samples_per_second, sound_output.secondary_buffer_size);
            Win32ClearSoundBuffer(&sound_output);
            global_secondary_buffer->Play(0, 0, DSBPLAY_LOOPING);

            global_is_running = true;

            int16 *samples = (int16 *)VirtualAlloc(0, sound_output.secondary_buffer_size,
                                                   MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

#if HANDMADE_INTERNAL
            LPVOID base_address = (LPVOID)Terabytes((uint64)2);
#else
            LPVOID base_address = 0;
#endif

            GameMemory game_memory = {};
            game_memory.permanent_storage_size = Megabytes(64);
            game_memory.transient_storage_size = Gigabytes(1);

            // TODO(mara): Handle various memory footprints.
            uint64 total_size = game_memory.permanent_storage_size + game_memory.transient_storage_size;
            game_memory.permanent_storage = VirtualAlloc(base_address,
                                                         (size_t)total_size,
                                                         MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            game_memory.transient_storage = ((uint8 *)game_memory.permanent_storage +
                                             game_memory.permanent_storage_size);

            if (samples && game_memory.permanent_storage && game_memory.transient_storage)
            {
                GameInput input[2] = {};
                GameInput *new_input = &input[0];
                GameInput *prev_input = &input[1];

                // Most recent frame clock time
                uint64 last_cycle_count = __rdtsc();
                LARGE_INTEGER last_counter;
                QueryPerformanceCounter(&last_counter);
                while (global_is_running)
                {
                    // TODO(mara): Zeroing macro.
                    // TODO(mara): We can't zero everything because the up/down state will be wrong!
                    GameControllerInput *prev_keyboard_controller = GetController(prev_input, 0);
                    GameControllerInput *new_keyboard_controller = GetController(new_input, 0);
                    *new_keyboard_controller = {};
                    new_keyboard_controller->is_connected = true;
                    for (int button_index = 0; button_index < ArrayCount(new_keyboard_controller->buttons); ++button_index)
                    {
                        new_keyboard_controller->buttons[button_index].ended_down = prev_keyboard_controller->buttons[button_index].ended_down;
                    }

                    Win32ProcessPendingMessages(new_keyboard_controller);

                    // TODO(mara): Need to not poll disconnected controllers to avoid xinput
                    // framerate hit on older
                    // TODO(mara): Should we poll this more frequently?
                    DWORD max_controller_count = XUSER_MAX_COUNT;
                    if (max_controller_count > (ArrayCount(new_input->controllers) - 1))
                    {
                        max_controller_count = (ArrayCount(new_input->controllers) - 1);
                    }

                    for (DWORD controller_index = 0; controller_index < max_controller_count; ++controller_index)
                    {
                        DWORD our_controller_index = controller_index + 1;
                        GameControllerInput *prev_controller = GetController(prev_input, our_controller_index);
                        GameControllerInput *new_controller = GetController(new_input, our_controller_index);

                        XINPUT_STATE controller_state;
                        if (XInputGetState(controller_index, &controller_state) == ERROR_SUCCESS)
                        {
                            new_controller->is_connected = true;

                            // NOTE(mara): This controller is plugged in.
                            // TODO(mara): See if controller_state.dwPacketNumber increments too rapidly.
                            XINPUT_GAMEPAD *pad = &controller_state.Gamepad;

                            // TODO(mara): This is a square deadzone, check xinput to verify that
                            // the deadzone is "round" and show how to do round deadzone processing.
                            new_controller->stick_average_x = Win32ProcessXInputStickValue(pad->sThumbLX,
                                                                    XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                            new_controller->stick_average_y = Win32ProcessXInputStickValue(pad->sThumbLY,
                                                                    XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                            if (new_controller->stick_average_x != 0 ||
                                new_controller->stick_average_y != 0)
                            {
                                new_controller->is_analog = true;
                            }

                            if (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)
                            {
                                new_controller->stick_average_y = 1.0f;
                                new_controller->is_analog = false;
                            }

                            if (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
                            {
                                new_controller->stick_average_y = -1.0f;
                                new_controller->is_analog = false;
                            }

                            if (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
                            {
                                new_controller->stick_average_x = -1.0f;
                                new_controller->is_analog = false;
                            }

                            if (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
                            {
                                new_controller->stick_average_x = 1.0f;
                                new_controller->is_analog = false;
                            }

                            real32 threshold = 0.5f;
                            Win32ProcessXInputDigitalButton((new_controller->stick_average_y > threshold) ? 1 : 0,
                                                            1,
                                                            &prev_controller->move_up,
                                                            &new_controller->move_up);
                            Win32ProcessXInputDigitalButton((new_controller->stick_average_y < -threshold) ? 1 : 0,
                                                            1,
                                                            &prev_controller->move_down,
                                                            &new_controller->move_down);
                            Win32ProcessXInputDigitalButton((new_controller->stick_average_x < -threshold) ? 1 : 0,
                                                            1,
                                                            &prev_controller->move_left,
                                                            &new_controller->move_left);
                            Win32ProcessXInputDigitalButton((new_controller->stick_average_x > threshold) ? 1 : 0,
                                                            1,
                                                            &prev_controller->move_right,
                                                            &new_controller->move_right);

                            Win32ProcessXInputDigitalButton(pad->wButtons, XINPUT_GAMEPAD_A,
                                                            &prev_controller->action_down,
                                                            &new_controller->action_down);
                            Win32ProcessXInputDigitalButton(pad->wButtons, XINPUT_GAMEPAD_B,
                                                            &prev_controller->action_right,
                                                            &new_controller->action_right);
                            Win32ProcessXInputDigitalButton(pad->wButtons, XINPUT_GAMEPAD_X,
                                                            &prev_controller->action_left,
                                                            &new_controller->action_left);
                            Win32ProcessXInputDigitalButton(pad->wButtons, XINPUT_GAMEPAD_Y,
                                                            &prev_controller->action_up,
                                                            &new_controller->action_up);

                            /*
                            Win32ProcessXInputDigitalButton(pad->wButtons, XINPUT_GAMEPAD_DPAD_UP,
                                                            &prev_controller->move_up,
                                                            &new_controller->move_up);
                            Win32ProcessXInputDigitalButton(pad->wButtons, XINPUT_GAMEPAD_DPAD_DOWN,
                                                            &prev_controller->move_down,
                                                            &new_controller->move_down);
                            Win32ProcessXInputDigitalButton(pad->wButtons, XINPUT_GAMEPAD_DPAD_LEFT,
                                                            &prev_controller->move_left,
                                                            &new_controller->move_left);
                            Win32ProcessXInputDigitalButton(pad->wButtons, XINPUT_GAMEPAD_DPAD_RIGHT,
                                                            &prev_controller->move_right,
                                                            &new_controller->move_right);*/

                            Win32ProcessXInputDigitalButton(pad->wButtons, XINPUT_GAMEPAD_LEFT_SHOULDER,
                                                            &prev_controller->left_shoulder,
                                                            &new_controller->left_shoulder);
                            Win32ProcessXInputDigitalButton(pad->wButtons, XINPUT_GAMEPAD_RIGHT_SHOULDER,
                                                            &prev_controller->right_shoulder,
                                                            &new_controller->right_shoulder);

                            Win32ProcessXInputDigitalButton(pad->wButtons, XINPUT_GAMEPAD_START,
                                                            &prev_controller->start,
                                                            &new_controller->start);
                            Win32ProcessXInputDigitalButton(pad->wButtons, XINPUT_GAMEPAD_BACK,
                                                            &prev_controller->back,
                                                            &new_controller->back);
                        }
                        else
                        {
                            // NOTE(mara): The controller is not available
                            new_controller->is_connected = false;
                        }
                    }

                    DWORD byte_to_lock = 0;
                    DWORD target_cursor = 0;
                    DWORD bytes_to_write = 0; // number of bytes total to write into the buffer.
                    DWORD play_cursor = 0;
                    DWORD write_cursor = 0;
                    bool32 sound_is_valid = false;

                    // TODO(mara): Tighten up sound logic so that we know where we should be writing to
                    // and can anticipate the time spent in the GameUpdateAndRender function!
                    if (SUCCEEDED(global_secondary_buffer->GetCurrentPosition(&play_cursor, &write_cursor)))
                    {
                        byte_to_lock = (sound_output.running_sample_index * sound_output.bytes_per_sample) % sound_output.secondary_buffer_size;

                        target_cursor = ((play_cursor +
                                                (sound_output.latency_sample_count * sound_output.bytes_per_sample)) %
                                               sound_output.secondary_buffer_size);

                        if (byte_to_lock > target_cursor)
                        {
                            bytes_to_write = (sound_output.secondary_buffer_size - byte_to_lock);
                            bytes_to_write += target_cursor;
                        }
                        else
                        {
                            bytes_to_write = target_cursor - byte_to_lock;
                        }

                        sound_is_valid = true;
                    }

                    // Interleaved sound sample buffer.
                    GameSoundOutputBuffer sound_buffer = {};
                    sound_buffer.samples_per_second = sound_output.samples_per_second;
                    sound_buffer.sample_count = bytes_to_write / sound_output.bytes_per_sample;
                    sound_buffer.samples = samples;

                    GameOffscreenBuffer offscreen_buffer = {};
                    offscreen_buffer.memory = global_backbuffer.memory;
                    offscreen_buffer.width = global_backbuffer.width;
                    offscreen_buffer.height = global_backbuffer.height;
                    offscreen_buffer.pitch = global_backbuffer.pitch;

                    GameUpdateAndRender(&game_memory, new_input, &offscreen_buffer, &sound_buffer);

                    // NOTE(mara): DirectSound output test
                    if (sound_is_valid)
                    {
                        Win32FillSoundBuffer(&sound_output, byte_to_lock, bytes_to_write, &sound_buffer);
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

#if 0
                    char buffer[256];
                    sprintf(buffer, "| %.02fms/f | %.02ff/s | %.02fmc/f |\n", ms_per_frame, fps, mcpf);
                    OutputDebugStringA(buffer);
#endif

                    last_counter = end_counter;
                    last_cycle_count = end_cycle_count;

                    GameInput *temp = new_input;
                    new_input = prev_input;
                    prev_input = temp;
                    // TODO(mara): Should i clear these here?
                }
            }
            else
            {
                // TODO(mara): Logging here.
            }
        }
        else
        {
            // TODO(mara): Logging here.
        }
    }
    else
    {
        // TODO(mara): Logging here.
    }

    return(0);
}
