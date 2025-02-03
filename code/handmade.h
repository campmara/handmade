#ifndef HANDMADE_H
#define HANDMADE_H

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

// FOUR THINGS - timing, controller / keyboard input, bitmap buffer to use, sound buffer to use
internal void GameUpdateAndRender(GameOffscreenBuffer *offscreen_buffer,
                                  int blue_offset, int green_offset,
                                  GameSoundOutputBuffer *sound_buffer,
                                  int tone_hz);

#endif
