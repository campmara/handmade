#include "handmade.h"

internal void GameOutputSound(GameSoundOutputBuffer *buffer, int tone_hz)
{
    local_persist real32 t_sine;
    int16 tone_volume = 3000;
    int wave_period = buffer->samples_per_second / tone_hz;

    int16 *sample_out = buffer->samples;
    for (int sample_index = 0; sample_index < buffer->sample_count; ++sample_index)
    {
        real32 sine_value = sinf(t_sine);
        int16 sample_value = (int16)(sine_value * tone_volume); // scale up to the volume hz
        *sample_out++ = sample_value;
        *sample_out++ = sample_value;

        t_sine += 2.0f * PI_32 * 1.0f / (real32)wave_period;
    }
}

internal void RenderWeirdGradient(GameOffscreenBuffer *buffer, int blue_offset, int green_offset)
{
    // Read the bitmap memory in a form we understand, not just void *.
    uint8 *row = (uint8 *)buffer->memory;
    for (int y = 0; y < buffer->height; ++y)
    {
        uint32 *pixel = (uint32 *)row;
        for (int x = 0; x < buffer->width; ++x)
        {
            /*
              Memory:   BB GG RR xx
              Register: xx RR GG BB

              LITTLE ENDIAN ARCHITECTURE! REMEMBER IT'S BACKWARDS IF WE'RE DEALING WITH BYTES.
              Windows programmers wanted it to look right in the hex values, so it looks like this:

              0x xxRRGGBB
            */
            uint8 blue = (uint8)(x + blue_offset);
            uint8 green = (uint8)(y + green_offset);
            *pixel++ = ((green << 8) | blue);
        }

        row += buffer->pitch;
    }
}

/*
void Win32ChangeSoundTone(Win32SoundOutput *sound_output, int new_hz)
{
    sound_output->tone_hz = new_hz;
    sound_output->wave_period = sound_output->samples_per_second / sound_output->tone_hz;
}
*/

internal void GameUpdateAndRender(GameMemory *memory,
                                  GameInput *input,
                                  GameOffscreenBuffer *offscreen_buffer,
                                  GameSoundOutputBuffer *sound_buffer)
{
    Assert((&input->controllers[0].terminator - &input->controllers[0].buttons[0]) ==
           (ArrayCount(input->controllers[0].buttons) - 1));
    Assert(sizeof(GameState) <= memory->permanent_storage_size);

    GameState *game_state = (GameState *)memory->permanent_storage;
    if (!memory->is_initialized)
    {
        char *filename = __FILE__;

        DEBUGReadFileResult file = DEBUGPlatformReadEntireFile(filename);
        if (file.content)
        {
            DEBUGPlatformWriteEntireFile("handmade_test.out",
                                         file.content_size, file.content);
            DEBUGPlatformFreeFileMemory(file.content);
        }

        // VirtualAlloc will clear game state to zero, so we only need to set the nonzero values
        // on initialization.
        game_state->tone_hz = 256;

        // TODO(mara): This may be more appropriate to do in the platform layer.
        memory->is_initialized = true;
    }

    for (int controller_index = 0; controller_index < ArrayCount(input->controllers); ++controller_index)
    {
        GameControllerInput *controller = GetController(input, controller_index);
        if (controller->is_analog)
        {
            // NOTE(mara): Use analog movement tuning.
            game_state->blue_offset += (int)(4.0f * controller->stick_average_x);
            game_state->tone_hz = 256 + (int)(120.0f * controller->stick_average_y);
        }
        else
        {
            // NOTE(mara): Use digital movement tuning.
            if (controller->move_left.ended_down)
            {
                game_state->blue_offset -= 1;
            }

            if (controller->move_right.ended_down)
            {
                game_state->blue_offset += 1;
            }
        }

        // Input.a_button_ended_down;
        // Input.a_button_half_transition_count;
        if (controller->action_down.ended_down)
        {
            game_state->green_offset += 1;
        }
    }

    // TODO(mara): Allow sample offsets here for more robust platform options.
    GameOutputSound(sound_buffer, game_state->tone_hz);
    RenderWeirdGradient(offscreen_buffer, game_state->blue_offset, game_state->green_offset);
}
