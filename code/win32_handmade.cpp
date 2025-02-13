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

#include "handmade.h"

#include <Windows.h>
#include <stdio.h>
#include <malloc.h>
#include <xinput.h>
#include <dsound.h>

#include "win32_handmade.h"

// TODO(mara): These are globals for now
global bool32               global_is_running;
global bool32               global_pause;
global Win32OffscreenBuffer global_backbuffer;
global LPDIRECTSOUNDBUFFER  global_secondary_buffer;
global int64                global_perf_count_frequency;

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

void CatStrings(size_t source_a_count, char *source_a,
                size_t source_b_count, char *source_b,
                size_t dest_count, char *dest)
{
    // TODO(mara): Dest bounds checking!!!
    for (int i = 0; i < source_a_count; ++i)
    {
        *dest++ = *source_a++;
    }

    for (int i = 0; i < source_b_count; ++i)
    {
        *dest++ = *source_b++;
    }

    *dest++ = 0; // insert the null terminator
}

internal int StringLength(char *string)
{
    int count = 0;
    while(*string++) // if *string is not zero, we continue to increment count, then increment.
    {
        ++count;
    }
    return count;
}

internal void Win32GetEXEFilename(Win32State *state)
{
    // NOTE(mara): Never use MAX_PATH in code that is user-facing. because it can be dangerous
    // and lead to bad results.
    DWORD size_of_file_name = GetModuleFileNameA(0, state->exe_filename, sizeof(state->exe_filename));
    state->one_past_last_exe_filename_slash = state->exe_filename;
    for (char *scan = state->exe_filename; *scan; ++scan)
    {
        if (*scan == '\\')
        {
            state->one_past_last_exe_filename_slash = scan + 1;
        }
    }
}

internal void Win32BuildEXEPathFilename(Win32State *state, char *filename, int dest_count, char *dest)
{
    CatStrings(state->one_past_last_exe_filename_slash - state->exe_filename, state->exe_filename,
               StringLength(filename), filename,
               dest_count, dest);
}

DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory)
{
    if (memory)
    {
        VirtualFree(memory, 0, MEM_RELEASE);
    }
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile)
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

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile)
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

inline FILETIME Win32GetLastWriteTime(char *filename)
{
    FILETIME last_write_time = {};

    WIN32_FILE_ATTRIBUTE_DATA data;
    if(GetFileAttributesEx(filename, GetFileExInfoStandard, &data))
    {
        last_write_time = data.ftLastWriteTime;
    }

    return last_write_time;
}

internal Win32GameCode Win32LoadGameCode(char *source_dll_name, char *temp_dll_name)
{
    Win32GameCode result = {};

    // TODO(mara): Need to get the proper path here!
    // TODO(mara): Automatic determination of when updates are necessary.

    result.dll_last_write_time = Win32GetLastWriteTime(source_dll_name);

    CopyFile(source_dll_name, temp_dll_name, FALSE);

    result.game_code_dll = LoadLibraryA(temp_dll_name);
    if (result.game_code_dll)
    {
        result.UpdateAndRender = (game_update_and_render *)GetProcAddress(result.game_code_dll, "GameUpdateAndRender");
        result.GetSoundSamples = (game_get_sound_samples *)GetProcAddress(result.game_code_dll, "GameGetSoundSamples");

        result.is_valid = (result.UpdateAndRender && result.GetSoundSamples);
    }

    if (!result.is_valid)
    {
        result.UpdateAndRender = 0;
        result.GetSoundSamples = 0;
    }

    return result;
}

internal void Win32UnloadGameCode(Win32GameCode *game_code)
{
    if (game_code->game_code_dll)
    {
        FreeLibrary(game_code->game_code_dll);
        game_code->game_code_dll = 0;
    }

    game_code->is_valid = false;
    game_code->UpdateAndRender = 0;
    game_code->GetSoundSamples = 0;
}

internal void Win32LoadXInput()
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
            buffer_desc.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
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
    // NOTE(mara): For prototyping purposes, we're going to always blit 1-to-1 pixels to make sure
    // we don't introduce artifacts with stretching while we are learning to code the renderer!
    StretchDIBits(device_context,
                  0, 0, buffer->width, buffer->height, // dest
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
#if 0
            if (w_param == TRUE)
            {
                SetLayeredWindowAttributes(window, RGB(0, 0, 0), 255, LWA_ALPHA);
            }
            else
            {
                SetLayeredWindowAttributes(window, RGB(0, 0, 0), 64, LWA_ALPHA);
            }
#endif
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

internal void Win32GetInputFileLocation(Win32State *state, int slot_index, int dest_count, char *dest)
{
    Assert(slot_index == 1);
    Win32BuildEXEPathFilename(state, "loop_edit.hmi", dest_count, dest);
}

internal void Win32BeginRecordingInput(Win32State *state, int input_recording_index)
{
    state->input_recording_index = input_recording_index;
    // TODO(mara): These files must go in a temporary/build directory!!
    // TODO(mara): Lazily write the giant memory block and use a memory copy instead?

    char filename[WIN32_STATE_FILE_NAME_COUNT];
    Win32GetInputFileLocation(state, input_recording_index, sizeof(filename), filename);

    state->recording_handle = CreateFileA(filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);

    DWORD bytes_to_write = (DWORD)state->total_size;
    Assert(state->total_size == bytes_to_write);
    DWORD bytes_written;
    WriteFile(state->recording_handle, state->game_memory_block, bytes_to_write, &bytes_written, 0);
}

internal void Win32EndRecordingInput(Win32State *state)
{
    CloseHandle(state->recording_handle);
    state->input_recording_index = 0;
}

internal void Win32BeginInputPlayback(Win32State *state, int input_playing_index)
{
    state->input_playing_index = input_playing_index;

    char filename[WIN32_STATE_FILE_NAME_COUNT];
    Win32GetInputFileLocation(state, input_playing_index, sizeof(filename), filename);

    state->playback_handle = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);

    DWORD bytes_to_read = (DWORD)state->total_size;
    Assert(state->total_size == bytes_to_read);
    DWORD bytes_read;
    ReadFile(state->playback_handle, state->game_memory_block, bytes_to_read, &bytes_read, 0);
}

internal void Win32EndInputPlayback(Win32State *state)
{
    CloseHandle(state->playback_handle);
    state->input_playing_index;
}

internal void Win32RecordInput(Win32State *state, GameInput *new_input)
{
    DWORD bytes_written;
    WriteFile(state->recording_handle, new_input, sizeof(*new_input), &bytes_written, 0);
}

internal void Win32PlaybackInput(Win32State *state, GameInput *new_input)
{
    DWORD bytes_read = 0;
    if (ReadFile(state->playback_handle, new_input, sizeof(*new_input), &bytes_read, 0))
    {
        if (bytes_read == 0)
        {
            // We've hit the end of the stream, go back to the beginning.
            int playing_index = state->input_playing_index;
            Win32EndInputPlayback(state);
            Win32BeginInputPlayback(state, playing_index);
            ReadFile(state->playback_handle, new_input, sizeof(*new_input), &bytes_read, 0);
        }
    }
}

internal void Win32ProcessPendingMessages(Win32State *state, GameControllerInput *keyboard_controller)
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
                // NOTE(mara): Since we are comparing was_down to is_down, we MUST use == and != to
                // convert these bit tests to actual 0 or 1 values.
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
#if HANDMADE_INTERNAL
                    else if (vk_code == 'P')
                    {
                        if (is_down)
                        {
                            global_pause = !global_pause;
                        }
                    }
                    else if (vk_code == 'L')
                    {
                        if (is_down)
                        {
                            if (state->input_recording_index == 0)
                            {
                                Win32BeginRecordingInput(state, 1);
                            }
                            else
                            {
                                Win32EndRecordingInput(state);
                                Win32BeginInputPlayback(state, 1);
                            }
                        }
                    }
#endif
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

inline LARGE_INTEGER Win32GetWallClock()
{
    LARGE_INTEGER result;
    QueryPerformanceCounter(&result);
    return result;
}

inline real32 Win32GetSecondsElapsed(LARGE_INTEGER start, LARGE_INTEGER end)
{
    real32 result = ((real32)(end.QuadPart - start.QuadPart) /
                     (real32)global_perf_count_frequency);
    return result;
}

internal void Win32DebugDrawVertical(Win32OffscreenBuffer *backbuffer,
                                     int x, int top, int bottom, uint32 color)
{
    if (top <= 0)
    {
        top = 0;
    }

    if (bottom > backbuffer->height)
    {
        bottom = backbuffer->height;
    }

    if ((x  >= 0) && (x < backbuffer->width))
    {
        uint8 *pixel = ((uint8 *)backbuffer->memory +
                        x * backbuffer->bytes_per_pixel +
                        top * backbuffer->pitch);
        for (int y = top; y < bottom; ++y)
        {
            *(uint32 *)pixel = color;
            pixel += backbuffer->pitch;
        }
    }
}

internal void Win32DrawSoundBufferMarker(Win32OffscreenBuffer *backbuffer,
                                         Win32SoundOutput *sound_output,
                                         real32 c, int pad_x, int top, int bottom,
                                         DWORD cursor_value, uint32 color)
{
    real32 x_real32 = (c * (real32)cursor_value);
    int x = pad_x + (int)x_real32;
    Win32DebugDrawVertical(backbuffer, x, top, bottom, color);
}

internal void Win32DebugSyncDisplay(Win32OffscreenBuffer *backbuffer,
                                    Win32SoundOutput *sound_output,
                                    int marker_count, Win32DebugTimeMarker *markers,
                                    int current_marker_index,
                                    real32 target_seconds_per_frame)
{

    int pad_x = 16;
    int pad_y = 16;

    int line_height = 64;

    real32 c = (real32)(backbuffer->width - (2 * pad_x)) / (real32)sound_output->secondary_buffer_size;
    for (int marker_index = 0; marker_index < marker_count; ++marker_index)
    {
        Win32DebugTimeMarker *this_marker = &markers[marker_index];
        Assert(this_marker->output_play_cursor < sound_output->secondary_buffer_size);
        Assert(this_marker->output_write_cursor < sound_output->secondary_buffer_size);
        Assert(this_marker->output_location < sound_output->secondary_buffer_size);
        Assert(this_marker->output_byte_count < sound_output->secondary_buffer_size);
        Assert(this_marker->flip_play_cursor < sound_output->secondary_buffer_size);
        Assert(this_marker->flip_write_cursor < sound_output->secondary_buffer_size);

        DWORD play_color = 0xFFFFFFFF;
        DWORD write_color = 0xFFFF0000;
        DWORD expected_flip_color = 0xFFFFFF00;
        DWORD play_window_color = 0xFFFF00FF;

        int top = pad_y;
        int bottom = pad_y + line_height;
        if (marker_index == current_marker_index)
        {
            top += line_height + pad_y;
            bottom += line_height + pad_y;

            int first_top = top;

            Win32DrawSoundBufferMarker(backbuffer, sound_output, c, pad_x, top, bottom, this_marker->output_play_cursor, play_color);
            Win32DrawSoundBufferMarker(backbuffer, sound_output, c, pad_x, top, bottom, this_marker->output_write_cursor, write_color);

            top += line_height + pad_y;
            bottom += line_height + pad_y;

            Win32DrawSoundBufferMarker(backbuffer, sound_output, c, pad_x, top, bottom, this_marker->output_location, play_color);
            Win32DrawSoundBufferMarker(backbuffer, sound_output, c, pad_x, top, bottom, this_marker->output_location + this_marker->output_byte_count, write_color);

            top += line_height + pad_y;
            bottom += line_height + pad_y;

            Win32DrawSoundBufferMarker(backbuffer, sound_output, c, pad_x, first_top, bottom, this_marker->expected_flip_play_cursor, expected_flip_color);
        }

        Win32DrawSoundBufferMarker(backbuffer, sound_output, c, pad_x, top, bottom, this_marker->flip_play_cursor, play_color);
        Win32DrawSoundBufferMarker(backbuffer, sound_output, c, pad_x, top, bottom, this_marker->flip_play_cursor + 480 * sound_output->bytes_per_sample, play_window_color);
        Win32DrawSoundBufferMarker(backbuffer, sound_output, c, pad_x, top, bottom, this_marker->flip_write_cursor, write_color);
    }
}

int CALLBACK WinMain(HINSTANCE Instance,
                     HINSTANCE PrevInstance,
                     LPSTR CommandLine,
                     int ShowCode)
{
    Win32State win32_state = {};

    LARGE_INTEGER perf_count_frequency_result;
    QueryPerformanceFrequency(&perf_count_frequency_result);
    global_perf_count_frequency = perf_count_frequency_result.QuadPart;

    Win32GetEXEFilename(&win32_state);

    char source_game_code_dll_full_path[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildEXEPathFilename(&win32_state, "handmade.dll",
                              sizeof(source_game_code_dll_full_path), source_game_code_dll_full_path);

    char temp_game_code_dll_full_path[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildEXEPathFilename(&win32_state, "handmade_temp.dll",
                              sizeof(temp_game_code_dll_full_path), temp_game_code_dll_full_path);

    // NOTE(mara): Set the windows scheduler granularity to 1 ms so that our sleep can be more
    // granular.
    UINT desired_scheduler_ms = 1;
    bool32 sleep_is_granular = (timeBeginPeriod(desired_scheduler_ms) == TIMERR_NOERROR);

    Win32LoadXInput();

    Win32ResizeDIBSection(&global_backbuffer, 1280, 720);

    WNDCLASSA window_class = {};
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = Win32MainWindowCallback;
    window_class.hInstance = Instance;
    window_class.lpszClassName = "HandmadeHeroWindowClass";

    // TODO(mara): How do we reliably query this on windows??
#define MONITOR_REFRESH_HZ 60
#define GAME_UPDATE_HZ (MONITOR_REFRESH_HZ / 2)
    real32 target_seconds_per_frame = 1.0f / (real32)GAME_UPDATE_HZ;

    if (RegisterClassA(&window_class))
    {
        HWND window = CreateWindowExA(0, //WS_EX_TOPMOST | WS_EX_LAYERED,
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
            // TODO(mara): make this 60 seconds?
            Win32SoundOutput sound_output = {};
            sound_output.samples_per_second = 48000;
            sound_output.bytes_per_sample = sizeof(int16) * 2;
            sound_output.secondary_buffer_size = sound_output.samples_per_second * sound_output.bytes_per_sample;
            // TODO(mara): Get rid of latency_sample_count.
            sound_output.latency_sample_count = 3 * (sound_output.samples_per_second / GAME_UPDATE_HZ);
            // TODO(mara): Actually compute this variance and see was the lowest reasonable value is.
            sound_output.safety_bytes = ((sound_output.samples_per_second * sound_output.bytes_per_sample) /
                                         GAME_UPDATE_HZ) / 3;

            Win32InitDirectSound(window, sound_output.samples_per_second, sound_output.secondary_buffer_size);
            Win32ClearSoundBuffer(&sound_output);
            global_secondary_buffer->Play(0, 0, DSBPLAY_LOOPING);

            global_is_running = true;

#if 0
            // TODO(mara): This tests the play_cursor / write_cursor update frequency on mara's
            // computer. It was 480 samples.
            while(global_is_running)
            {
                DWORD play_cursor;
                DWORD write_cursor;
                global_secondary_buffer->GetCurrentPosition(&play_cursor, &write_cursor);

                char text_buffer[256];
                _snprintf_s(text_buffer, sizeof(text_buffer),
                            "PC: %u | WC: %u\n", play_cursor, write_cursor);
                OutputDebugStringA(text_buffer);
            }
#endif

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

            game_memory.DEBUGPlatformFreeFileMemory = DEBUGPlatformFreeFileMemory;
            game_memory.DEBUGPlatformReadEntireFile = DEBUGPlatformReadEntireFile;
            game_memory.DEBUGPlatformWriteEntireFile = DEBUGPlatformWriteEntireFile;

            // TODO(mara): Handle various memory footprints (USING SYSTEM METRICS).
            // TODO(mara); Use MEM_LARGE_PAGES and call AdjustTokenPrivileges when not on Windows XP?
            win32_state.total_size = game_memory.permanent_storage_size + game_memory.transient_storage_size;
            win32_state.game_memory_block = VirtualAlloc(base_address,
                                                         (size_t)win32_state.total_size,
                                                         MEM_RESERVE | MEM_COMMIT,
                                                         PAGE_READWRITE);
            game_memory.permanent_storage = win32_state.game_memory_block;
            game_memory.transient_storage = ((uint8 *)game_memory.permanent_storage +
                                             game_memory.permanent_storage_size);

            if (samples && game_memory.permanent_storage && game_memory.transient_storage)
            {
                GameInput input[2] = {};
                GameInput *new_input = &input[0];
                GameInput *old_input = &input[1];

                LARGE_INTEGER last_counter = Win32GetWallClock();
                LARGE_INTEGER flip_wall_clock = Win32GetWallClock();

                int debug_time_marker_index = 0;
                Win32DebugTimeMarker debug_time_markers[GAME_UPDATE_HZ / 2] = {0};

                DWORD audio_latency_bytes = 0;
                real32 audio_latency_seconds = 0;
                bool32 sound_is_valid = false;

                Win32GameCode game = Win32LoadGameCode(source_game_code_dll_full_path, temp_game_code_dll_full_path);

                uint64 last_cycle_count = __rdtsc();
                while (global_is_running)
                {
                    FILETIME new_dll_write_time = Win32GetLastWriteTime(source_game_code_dll_full_path);
                    if (CompareFileTime(&new_dll_write_time, &game.dll_last_write_time) != 0)
                    {
                        Win32UnloadGameCode(&game);
                        game = Win32LoadGameCode(source_game_code_dll_full_path, temp_game_code_dll_full_path);
                    }

                    // TODO(mara): Zeroing macro.
                    // TODO(mara): We can't zero everything because the up/down state will be wrong!
                    GameControllerInput *old_keyboard_controller = GetController(old_input, 0);
                    GameControllerInput *new_keyboard_controller = GetController(new_input, 0);
                    *new_keyboard_controller = {};
                    new_keyboard_controller->is_connected = true;
                    for (int button_index = 0; button_index < ArrayCount(new_keyboard_controller->buttons); ++button_index)
                    {
                        new_keyboard_controller->buttons[button_index].ended_down = old_keyboard_controller->buttons[button_index].ended_down;
                    }

                    Win32ProcessPendingMessages(&win32_state, new_keyboard_controller);

                    if (!global_pause)
                    {
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
                            GameControllerInput *old_controller = GetController(old_input, our_controller_index);
                            GameControllerInput *new_controller = GetController(new_input, our_controller_index);

                            XINPUT_STATE controller_state;
                            if (XInputGetState(controller_index, &controller_state) == ERROR_SUCCESS)
                            {
                                new_controller->is_connected = true;
                                new_controller->is_analog = old_controller->is_analog;

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
                                                                &old_controller->move_up,
                                                                &new_controller->move_up);
                                Win32ProcessXInputDigitalButton((new_controller->stick_average_y < -threshold) ? 1 : 0,
                                                                1,
                                                                &old_controller->move_down,
                                                                &new_controller->move_down);
                                Win32ProcessXInputDigitalButton((new_controller->stick_average_x < -threshold) ? 1 : 0,
                                                                1,
                                                                &old_controller->move_left,
                                                                &new_controller->move_left);
                                Win32ProcessXInputDigitalButton((new_controller->stick_average_x > threshold) ? 1 : 0,
                                                                1,
                                                                &old_controller->move_right,
                                                                &new_controller->move_right);

                                Win32ProcessXInputDigitalButton(pad->wButtons, XINPUT_GAMEPAD_A,
                                                                &old_controller->action_down,
                                                                &new_controller->action_down);
                                Win32ProcessXInputDigitalButton(pad->wButtons, XINPUT_GAMEPAD_B,
                                                                &old_controller->action_right,
                                                                &new_controller->action_right);
                                Win32ProcessXInputDigitalButton(pad->wButtons, XINPUT_GAMEPAD_X,
                                                                &old_controller->action_left,
                                                                &new_controller->action_left);
                                Win32ProcessXInputDigitalButton(pad->wButtons, XINPUT_GAMEPAD_Y,
                                                                &old_controller->action_up,
                                                                &new_controller->action_up);

                                /*
                                  Win32ProcessXInputDigitalButton(pad->wButtons, XINPUT_GAMEPAD_DPAD_UP,
                                  &old_controller->move_up,
                                  &new_controller->move_up);
                                  Win32ProcessXInputDigitalButton(pad->wButtons, XINPUT_GAMEPAD_DPAD_DOWN,
                                  &old_controller->move_down,
                                  &new_controller->move_down);
                                  Win32ProcessXInputDigitalButton(pad->wButtons, XINPUT_GAMEPAD_DPAD_LEFT,
                                  &old_controller->move_left,
                                  &new_controller->move_left);
                                  Win32ProcessXInputDigitalButton(pad->wButtons, XINPUT_GAMEPAD_DPAD_RIGHT,
                                  &old_controller->move_right,
                                  &new_controller->move_right);*/

                                Win32ProcessXInputDigitalButton(pad->wButtons, XINPUT_GAMEPAD_LEFT_SHOULDER,
                                                                &old_controller->left_shoulder,
                                                                &new_controller->left_shoulder);
                                Win32ProcessXInputDigitalButton(pad->wButtons, XINPUT_GAMEPAD_RIGHT_SHOULDER,
                                                                &old_controller->right_shoulder,
                                                                &new_controller->right_shoulder);

                                Win32ProcessXInputDigitalButton(pad->wButtons, XINPUT_GAMEPAD_START,
                                                                &old_controller->start,
                                                                &new_controller->start);
                                Win32ProcessXInputDigitalButton(pad->wButtons, XINPUT_GAMEPAD_BACK,
                                                                &old_controller->back,
                                                                &new_controller->back);
                            }
                            else
                            {
                                // NOTE(mara): The controller is not available
                                new_controller->is_connected = false;
                            }
                        }

                        GameOffscreenBuffer offscreen_buffer = {};
                        offscreen_buffer.memory = global_backbuffer.memory;
                        offscreen_buffer.width = global_backbuffer.width;
                        offscreen_buffer.height = global_backbuffer.height;
                        offscreen_buffer.pitch = global_backbuffer.pitch;
                        offscreen_buffer.bytes_per_pixel = global_backbuffer.bytes_per_pixel;

                        if (win32_state.input_recording_index)
                        {
                            Win32RecordInput(&win32_state, new_input);
                        }

                        if (win32_state.input_playing_index)
                        {
                            Win32PlaybackInput(&win32_state, new_input);
                        }
                        if (game.UpdateAndRender)
                        {
                            game.UpdateAndRender(&game_memory, new_input, &offscreen_buffer);
                        }

                        LARGE_INTEGER audio_wall_clock = Win32GetWallClock();
                        real32 from_begin_to_audio_seconds = Win32GetSecondsElapsed(flip_wall_clock, audio_wall_clock);

                        DWORD play_cursor;
                        DWORD write_cursor;
                        if (global_secondary_buffer->GetCurrentPosition(&play_cursor, &write_cursor) == DS_OK)
                        {
                            /* NOTE(mara):

                               Here is how sound output computation works:

                               We define a safety value that is the number of samples we think our game
                               update loop may vary by (let's say up to 2ms).

                               When we wake up to write audio, we will look at the play cursor position,
                               and forecast ahead where we think the play cursor will be on the next
                               frame boundary.

                               We will then look to see if the write cursor is before that by at least
                               our safety value. If it is, the target fill position is that frame
                               boundary plus one frame. This gives us perfect audio sync in the case of
                               a card that has low enough latency.

                               If the write cursor is _after_ that safety margin, then we assume we can
                               never sync the audio perfectly, so we will write one frame's worth of
                               audio plus the safety margin's worth of guard samples.
                            */

                            if (!sound_is_valid)
                            {
                                sound_output.running_sample_index = write_cursor / sound_output.bytes_per_sample;
                                sound_is_valid = true;
                            }

                            DWORD byte_to_lock = ((sound_output.running_sample_index * sound_output.bytes_per_sample) %
                                                  sound_output.secondary_buffer_size);

                            DWORD expected_sound_bytes_per_frame = ((sound_output.samples_per_second * sound_output.bytes_per_sample) /
                                                                    GAME_UPDATE_HZ);
                            real32 seconds_left_until_flip = (target_seconds_per_frame - from_begin_to_audio_seconds);
                            DWORD expected_bytes_until_flip = (DWORD)((seconds_left_until_flip / target_seconds_per_frame) *
                                                                      (real32)expected_sound_bytes_per_frame);

                            DWORD expected_frame_boundary_byte = play_cursor + expected_bytes_until_flip;

                            DWORD safe_write_cursor = write_cursor;
                            if (safe_write_cursor < play_cursor)
                            {
                                safe_write_cursor += sound_output.secondary_buffer_size;
                            }
                            Assert(safe_write_cursor >= play_cursor);
                            safe_write_cursor += sound_output.safety_bytes;

                            bool32 audio_card_is_low_latency = (safe_write_cursor < expected_frame_boundary_byte);
                            DWORD target_cursor = 0;
                            if (audio_card_is_low_latency)
                            {
                                target_cursor = (expected_frame_boundary_byte + expected_sound_bytes_per_frame);
                            }
                            else
                            {
                                target_cursor = (write_cursor + expected_sound_bytes_per_frame +
                                                 sound_output.safety_bytes);
                            }
                            // Wrap the cursor around the buffer size to avoid overflow.
                            target_cursor = (target_cursor % sound_output.secondary_buffer_size);

                            DWORD bytes_to_write = 0; // number of bytes total to write into the buffer.
                            if (byte_to_lock > target_cursor)
                            {
                                bytes_to_write = (sound_output.secondary_buffer_size - byte_to_lock);
                                bytes_to_write += target_cursor;
                            }
                            else
                            {
                                bytes_to_write = target_cursor - byte_to_lock;
                            }

                            // Interleaved sound sample buffer.
                            GameSoundOutputBuffer sound_buffer = {};
                            sound_buffer.samples_per_second = sound_output.samples_per_second;
                            sound_buffer.sample_count = bytes_to_write / sound_output.bytes_per_sample;
                            sound_buffer.samples = samples;
                            if (game.GetSoundSamples)
                            {
                                game.GetSoundSamples(&game_memory, &sound_buffer);
                            }

#if HANDMADE_INTERNAL
                            Win32DebugTimeMarker *marker = &debug_time_markers[debug_time_marker_index];
                            marker->output_play_cursor = play_cursor;
                            marker->output_write_cursor = write_cursor;
                            marker->output_location = byte_to_lock;
                            marker->output_byte_count = bytes_to_write;
                            marker->expected_flip_play_cursor = expected_frame_boundary_byte;

                            DWORD unwrapped_write_cursor = write_cursor;
                            if (unwrapped_write_cursor < play_cursor)
                            {
                                unwrapped_write_cursor += sound_output.secondary_buffer_size;
                            }
                            audio_latency_bytes = unwrapped_write_cursor - play_cursor;
                            audio_latency_seconds = (((real32)audio_latency_bytes / (real32)sound_output.bytes_per_sample) /
                                                     (real32)sound_output.samples_per_second);

                            char text_buffer[256];
                            _snprintf_s(text_buffer, sizeof(text_buffer),
                                        "BTL: %u | TC: %u | BTW: %u | PC: %u | WC: %u | DELTA: %u (%fs)\n",
                                        byte_to_lock, target_cursor, bytes_to_write,
                                        play_cursor, write_cursor, audio_latency_bytes, audio_latency_seconds);
                            OutputDebugStringA(text_buffer);
#endif
                            Win32FillSoundBuffer(&sound_output, byte_to_lock, bytes_to_write, &sound_buffer);
                        }
                        else
                        {
                            sound_is_valid = false;
                        }

                        LARGE_INTEGER work_counter = Win32GetWallClock();
                        real32 work_seconds_elapsed = Win32GetSecondsElapsed(last_counter, work_counter);

                        // TODO(mara): NOT TESTED YET! PROBABLY BUGGY!!!!!!!!
                        real32 seconds_elapsed_for_frame = work_seconds_elapsed;
                        if (seconds_elapsed_for_frame < target_seconds_per_frame)
                        {
                            if (sleep_is_granular)
                            {
                                DWORD sleep_ms = (DWORD)(1000.0f * (target_seconds_per_frame -
                                                                    seconds_elapsed_for_frame));
                                if(sleep_ms > 0)
                                {
                                    Sleep(sleep_ms);
                                }
                            }

                            real32 test_seconds_elapsed_for_frame = Win32GetSecondsElapsed(last_counter, Win32GetWallClock());

                            if (test_seconds_elapsed_for_frame < target_seconds_per_frame)
                            {
                                // TODO(mara): LOG MISSED SLEEP HERE!!!!
                            }

                            while (seconds_elapsed_for_frame < target_seconds_per_frame)
                            {
                                seconds_elapsed_for_frame = Win32GetSecondsElapsed(last_counter, Win32GetWallClock());
                            }
                        }
                        else
                        {
                            // TODO(mara): MISSED FRAME RATE!!!
                            // TODO(mara): Logging
                        }

                        LARGE_INTEGER end_counter = Win32GetWallClock();
                        real32 ms_per_frame = 1000.0f * Win32GetSecondsElapsed(last_counter, end_counter);
                        last_counter = end_counter;

                        Win32WindowDimensions dimensions = GetWindowDimensions(window);
#if HANDMADE_INTERNAL
                        // TODO(mara): Current is wrong on the 0th index!!
                        Win32DebugSyncDisplay(&global_backbuffer,
                                              &sound_output,
                                              ArrayCount(debug_time_markers), debug_time_markers,
                                              debug_time_marker_index - 1,
                                              target_seconds_per_frame);
#endif
                        HDC device_context = GetDC(window);
                        Win32DisplayBufferInWindow(&global_backbuffer, device_context,
                                                   dimensions.width, dimensions.height);
                        ReleaseDC(window, device_context);

                        flip_wall_clock = Win32GetWallClock();
#if HANDMADE_INTERNAL
                        // NOTE(mara): This is debug code!
                        {
                            DWORD debug_play_cursor;
                            DWORD debug_write_cursor;
                            if (global_secondary_buffer->GetCurrentPosition(&debug_play_cursor, &debug_write_cursor) == DS_OK)
                            {
                                Assert(debug_time_marker_index < ArrayCount(debug_time_markers));
                                Win32DebugTimeMarker *marker = &debug_time_markers[debug_time_marker_index];
                                marker->flip_play_cursor = debug_play_cursor;
                                marker->flip_write_cursor = debug_write_cursor;
                            }
                        }
#endif

                        GameInput *temp = new_input;
                        new_input = old_input;
                        old_input = temp;
                        // TODO(mara): Should i clear these here?

                        uint64 end_cycle_count = __rdtsc();
                        uint64 cycles_elapsed = end_cycle_count - last_cycle_count;
                        last_cycle_count = end_cycle_count;

                        real64 fps = 0.0f;
                        real64 mcpf = ((real64)cycles_elapsed / (1000.0 * 1000.0));

                        char fps_buffer[256];
                        _snprintf_s(fps_buffer, sizeof(fps_buffer),
                                    "| %.02fms/f | %.02ff/s | %.02fmc/f |\n", ms_per_frame, fps, mcpf);
                        OutputDebugStringA(fps_buffer);

#if HANDMADE_INTERNAL
                        ++debug_time_marker_index;
                        if (debug_time_marker_index == ArrayCount(debug_time_markers))
                        {
                            debug_time_marker_index = 0;
                        }
#endif
                    }
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
