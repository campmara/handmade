#include "handmade.h"

internal void GameOutputSound(GameState *game_state, GameSoundOutputBuffer *buffer, int tone_hz)
{
    int16 tone_volume = 3000;
    int wave_period = buffer->samples_per_second / tone_hz;

    int16 *sample_out = buffer->samples;
    for (int sample_index = 0; sample_index < buffer->sample_count; ++sample_index)
    {
#if 1
        real32 sine_value = sinf(game_state->t_sine);
        int16 sample_value = (int16)(sine_value * tone_volume); // scale up to the volume hz
#else
        int16 sample_value = 0;
#endif
        *sample_out++ = sample_value;
        *sample_out++ = sample_value;

        game_state->t_sine += 2.0f * PI_32 * 1.0f / (real32)wave_period;
        if (game_state->t_sine > 2.0f * PI_32)
        {
            game_state->t_sine -= 2.0f * PI_32;
        }
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
            *pixel++ = ((green << 16) | blue);
        }

        row += buffer->pitch;
    }
}

internal void RenderPlayer(GameOffscreenBuffer *buffer, int player_x, int player_y)
{
    uint8 *end_of_buffer = (uint8 *)buffer->memory + buffer->pitch * buffer->height;
    uint32 color = 0xFFFFFFFF;
    int top = player_y;
    int bottom = player_y + 10;

    for (int x = player_x; x < player_x + 10; ++x)
    {
        uint8 *pixel = ((uint8 *)buffer->memory +
                        x * buffer->bytes_per_pixel +
                        top * buffer->pitch);
        for (int y = top; y < bottom; ++y)
        {
            if (pixel >= buffer->memory && (pixel + 4) <= end_of_buffer)
            {
                *(uint32 *)pixel = color;
            }
            pixel += buffer->pitch;
        }
    }
}

/*
void Win32ChangeSoundTone(Win32SoundOutput *sound_output, int new_hz)
{
    sound_output->tone_hz = new_hz;
    sound_output->wave_period = sound_output->samples_per_second / sound_output->tone_hz;
}
*/

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    Assert((&input->controllers[0].terminator - &input->controllers[0].buttons[0]) ==
           (ArrayCount(input->controllers[0].buttons) - 1));
    Assert(sizeof(GameState) <= memory->permanent_storage_size);

    GameState *game_state = (GameState *)memory->permanent_storage;
    if (!memory->is_initialized)
    {
        char *filename = __FILE__;

        DEBUGReadFileResult file = memory->DEBUGPlatformReadEntireFile(filename);
        if (file.content)
        {
            memory->DEBUGPlatformWriteEntireFile("handmade_test.out",
                                         file.content_size, file.content);
            memory->DEBUGPlatformFreeFileMemory(file.content);
        }

        // VirtualAlloc will clear game state to zero, so we only need to set the nonzero values
        // on initialization.
        game_state->tone_hz = 512;
        game_state->t_sine = 0.0f;

        game_state->player_x = 100;
        game_state->player_y = 100;

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
            game_state->tone_hz = 512 + (int)(120.0f * controller->stick_average_y);
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

        game_state->player_x += (int)(4.0f * controller->stick_average_x);
        game_state->player_y -= (int)(4.0f * controller->stick_average_y);
        if (game_state->t_jump > 0)
        {
            game_state->player_y += (int)(5.0f * sinf(0.5f * PI_32 * game_state->t_jump));
        }
        if (controller->action_down.ended_down)
        {
            game_state->t_jump = 4.0;
        }
        game_state->t_jump -= 0.033f;
    }

    RenderWeirdGradient(offscreen_buffer, game_state->blue_offset, game_state->green_offset);
    RenderPlayer(offscreen_buffer, game_state->player_x, game_state->player_y);
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    GameState *game_state = (GameState *)memory->permanent_storage;
    GameOutputSound(game_state, sound_buffer, game_state->tone_hz);
}
