#include "handmade.h"

internal void RenderWeirdGradient(GameOffscreenBuffer *buffer, int blue_offset, int green_offset)
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

internal void GameUpdateAndRender(GameOffscreenBuffer *buffer, int blue_offset, int green_offset)
{
    RenderWeirdGradient(buffer, blue_offset, green_offset);
}
