#ifndef HANDMADE_H
#define HANDMADE_H

#define ArrayCount(array) (sizeof(array) / sizeof((array)[0]))
// TODO(mara): swap, min, max ... macros???


/*
 TODO(mara): Services that the platform layer provides to the game.
 */

/*
  NOTE(mara): services that the game provides to the platform layer.
  This may expand in the future - sound on separate thread, etc.
 */

// TODO(mara): In the future, rendering _specifically_ will become a three-tiered abstraction!!!
struct GameOffscreenBuffer
{
    void       *memory;
    int         width;
    int         height;
    int         pitch;
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
    bool32 is_analog;
    
    real32 start_x;
    real32 start_y;

    real32 min_x;
    real32 min_y;

    real32 max_x;
    real32 max_y;

    real32 end_x;
    real32 end_y;

    union
    {
        GameButtonState buttons[12];
        struct
        {
            GameButtonState up;
            GameButtonState down;
            GameButtonState left;
            GameButtonState right;
            GameButtonState dpad_up;
            GameButtonState dpad_down;
            GameButtonState dpad_left;
            GameButtonState dpad_right;
            GameButtonState left_shoulder;
            GameButtonState right_shoulder;
            GameButtonState start;
            GameButtonState back;
        };
    };
};

struct GameInput
{
    GameControllerInput controllers[4];
};

struct GameMemory
{
    uint64 permanent_storage_space;
    void *permanent_storage;
};

internal void GameUpdateAndRender(GameMemory *memory,
                                  GameInput *input,
                                  GameOffscreenBuffer *offscreen_buffer,
                                  GameSoundOutputBuffer *sound_buffer);

//
//
//

struct GameState
{
    int blue_offset;
    int green_offset;
    int tone_hz;
};

#endif
