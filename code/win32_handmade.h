struct Win32OffscreenBuffer
{
    BITMAPINFO  info;
    void       *memory;
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

struct Win32SoundOutput
{
    int    samples_per_second;
    uint32 running_sample_index;
    int    bytes_per_sample;
    DWORD    secondary_buffer_size;
    real32 t_sine;
    int    latency_sample_count; // how many samples away from the cursor we'd like to be in general
};

struct Win32DebugTimeMarker
{
    DWORD play_cursor;
    DWORD write_cursor;
};
