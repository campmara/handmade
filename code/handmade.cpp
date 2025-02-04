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
            uint8 blue = (x + blue_offset);
            uint8 green = (y + green_offset);
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
    GameState *game_state = (GameState *)memory->permanent_storage;

    GameControllerInput *input_0 = &input->controllers[0];
    if (input_0->is_analog)
    {
        // NOTE(mara): Use analog movement tuning.
        blue_offset += (int)4.0f * (input_0->end_x);
        tone_hz = 256 + (int)(120.0f * (input_0->end_y));
    }
    else
    {
        // NOTE(mara): Use digital movement tuning.
    }

    // Input.a_button_ended_down;
    // Input.a_button_half_transition_count;
    if (input_0->down.ended_down)
    {
        green_offset += 1;
    }

    // TODO(mara): Allow sample offsets here for more robust platform options.
    GameOutputSound(sound_buffer, tone_hz);
    RenderWeirdGradient(offscreen_buffer, blue_offset, green_offset);
}
