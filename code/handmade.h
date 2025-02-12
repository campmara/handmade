#ifndef HANDMADE_H
#define HANDMADE_H

/*
  NOTE(mara): Compile-time Defines:

  HANDMADE_INTERNAL:
      0 - Build for public release.
      1 - Build for developer only.
  HANDMADE_SLOW:
      0 - Slow code NOT allowed!
      1 - Slow code welcome. (Asserts)
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

#if HANDMADE_SLOW
#define Assert(expression) if (!(expression)) { *(int *)0 = 0; }
#else
#define Assert(expression)
#endif

// TODO(mara): Should these always use 64-bit?
#define Kilobytes(value) ((value) * 1024)
#define Megabytes(value) (Kilobytes(value) * 1024)
#define Gigabytes(value) (Megabytes(value) * 1024)
#define Terabytes(value) (Gigabytes(value) * 1024)

#define ArrayCount(array) (sizeof(array) / sizeof((array)[0]))
// TODO(mara): swap, min, max ... macros???

inline uint32 SafeTruncateUInt64(uint64 value)
{
    // TODO(mara): Defines for maximum values (uint32 max)
    Assert(value <= 0xFFFFFFFF);
    uint32 result = (uint32)value;
    return result;
}

/*
  ==================================================================================================
  NOTE(mara): Services that the platform layer provides to the game.
  ==================================================================================================
 */

#if HANDMADE_INTERNAL
/*
  IMPORTANT(mara):

  These are NOT for doing anything in the shipping game - they are blocking and the write doesn't
  protect against lost data!
 */

struct DEBUGReadFileResult
{
    uint32 content_size;
    void *content;
};

#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name(void *memory)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory);

#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) DEBUGReadFileResult name(char *filename)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) bool32 name(char *filename, uint32 memory_size, void *memory)
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);

#endif

/*
  ==================================================================================================
  NOTE(mara): services that the game provides to the platform layer.
  This may expand in the future - sound on separate thread, etc.
  ==================================================================================================
 */

// TODO(mara): In the future, rendering _specifically_ will become a three-tiered abstraction!!!
struct GameOffscreenBuffer
{
    void       *memory;
    int         width;
    int         height;
    int         pitch;
    int         bytes_per_pixel;
};

struct GameSoundOutputBuffer
{
    int sample_count;
    int samples_per_second;
    int16 *samples;
};

struct GameButtonState
{
    bool32 ended_down;
    int half_transition_count;
};

struct GameControllerInput
{
    bool32 is_connected;
    bool32 is_analog;
    real32 stick_average_x;
    real32 stick_average_y;

    union
    {
        GameButtonState buttons[13];
        struct
        {
            GameButtonState move_up;
            GameButtonState move_down;
            GameButtonState move_left;
            GameButtonState move_right;

            GameButtonState action_up;
            GameButtonState action_down;
            GameButtonState action_left;
            GameButtonState action_right;

            GameButtonState left_shoulder;
            GameButtonState right_shoulder;

            GameButtonState back;
            GameButtonState start;

            // NOTE(mara): All buttons must be added above this line.

            GameButtonState terminator;
         };
    };
};

struct GameInput
{
    // TODO(mara): Insert clock values here.

    // 1 Keyboard, 4 Gamepads.
    GameControllerInput controllers[5];
};
inline GameControllerInput *GetController(GameInput *input, unsigned int controller_index)
{
    Assert(controller_index < ArrayCount(input->controllers));
    GameControllerInput *result = &input->controllers[controller_index];
    return result;
}

struct GameMemory
{
    bool32 is_initialized;

    uint64 permanent_storage_size;
    void *permanent_storage; // NOTE(mara): REQUIRED to be cleared to zero at startup!

    uint64 transient_storage_size;
    void *transient_storage; // NOTE(mara): REQUIRED to be cleared to zero at startup!

    debug_platform_free_file_memory *DEBUGPlatformFreeFileMemory;
    debug_platform_read_entire_file *DEBUGPlatformReadEntireFile;
    debug_platform_write_entire_file *DEBUGPlatformWriteEntireFile;
};

#define GAME_UPDATE_AND_RENDER(name) void name(GameMemory *memory, GameInput *input, GameOffscreenBuffer *offscreen_buffer)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);
GAME_UPDATE_AND_RENDER(GameUpdateAndRenderStub)
{
}

// NOTE(mara): At the moment, this has to be a very fast function. It can not be > 1ms or so!!!!!
// TODO(mara): Reduce the pressure on this function's performance by measuring it / asking about it.
#define GAME_GET_SOUND_SAMPLES(name) void name(GameMemory *memory, GameSoundOutputBuffer *sound_buffer)
typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);
GAME_GET_SOUND_SAMPLES(GameGetSoundSamplesStub)
{
}

//
//
//

struct GameState
{
    int blue_offset;
    int green_offset;
    int tone_hz;

    real32 t_sine;

    int player_x;
    int player_y;
    real32 t_jump;
};

#endif
