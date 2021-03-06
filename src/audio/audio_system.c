#include "audio_system.h"
#include "common/profiler.h"
#include <sokol/sokol_audio.h>

static void audio_main(float* buffer, i32 num_frames, i32 num_channels)
{
    for (i32 i = 0; i < num_frames; ++i)
    {
        *buffer++ = 0.0f;
        *buffer++ = 0.0f;
    }
}

void audio_sys_init(void)
{
    saudio_setup(&(saudio_desc)
    {
        .num_channels = 2,
        .sample_rate = 44100,
        .stream_cb = audio_main,
    });
}

ProfileMark(pm_update, audio_sys_update)
void audio_sys_update(void)
{
    ProfileBegin(pm_update);

    ProfileEnd(pm_update);
}

void audio_sys_shutdown(void)
{
    saudio_shutdown();
}
