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
    DWORD  secondary_buffer_size;
    DWORD  safety_bytes;
    real32 t_sine;
    int    latency_sample_count; // How many samples away from the cursor we'd like to be in general.
    // TODO(mara): Math gets simpler if we add a bytes_per_second field?
    // TODO(mara): Should running sample index be in bytes as well?
};

struct Win32DebugTimeMarker
{
    DWORD output_play_cursor;
    DWORD output_write_cursor;
    DWORD output_location;
    DWORD output_byte_count;
    DWORD expected_flip_play_cursor;

    DWORD flip_play_cursor;
    DWORD flip_write_cursor;
};

struct Win32GameCode
{
    HMODULE game_code_dll;
    FILETIME dll_last_write_time;

    // Function Pointers
    game_update_and_render *UpdateAndRender;
    game_get_sound_samples *GetSoundSamples;

    bool32 is_valid;
};

struct Win32State
{
    uint64 total_size;
    void *game_memory_block;

    HANDLE recording_handle;
    int input_recording_index;

    HANDLE playback_handle;
    int input_playing_index;
};
