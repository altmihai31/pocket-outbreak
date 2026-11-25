#include "kx_bench.h"

#if KX_BENCH

#include "bn_algorithm.h"
#include "bn_core.h"
#include "bn_log.h"
#include "bn_optional.h"
#include "bn_timer.h"
#include "bn_timers.h"

namespace kx::bench
{

namespace
{
    constexpr const char* section_names[] = { "player", "zombies", "fx", "visuals", "camera", "bg_stream", "hud",
                                                "world_gen", "hordes", "flow", "save", "vehicles", "animals" };

    struct stats
    {
        int total = 0;
        int max = 0;
    };

    stats sections[SECTIONS_COUNT];
    stats frame;

    // This frame's ticks per section, and the frame before's: bn::core's figures for a frame arrive a frame late,
    // so a missed frame logs the breakdown of the frame before.
    int frame_sections[SECTIONS_COUNT];
    int previous_sections[SECTIONS_COUNT];
    int frames = 0;
    int missed_frames = 0;
    int current_section = -1;
    int warmup_frames = 4;          // the first frames still measure start-up work
    bn::optional<bn::timer> section_timer;

    // Per-mille of a frame, from timer ticks.
    [[nodiscard]] int per_mille(int ticks)
    {
        return (ticks * 1000) / bn::timers::ticks_per_frame();
    }
}

void begin(int section)
{
    current_section = section;

    if(! section_timer)
    {
        section_timer = bn::timer();
    }

    section_timer->restart();
}

void end()
{
    int ticks = section_timer->elapsed_ticks();
    stats& current = sections[current_section];
    frame_sections[current_section] += ticks;
    current.total += ticks;
    current.max = bn::max(current.max, ticks);
}

void frame_end()
{
    if(warmup_frames > 0)
    {
        --warmup_frames;
        return;
    }

    int ticks = bn::core::last_cpu_ticks();
    frame.total += ticks;
    frame.max = bn::max(frame.max, ticks);
    int missed = bn::core::last_missed_frames();

    if(missed)
    {
        BN_LOG("KXMISS frame=", frames, " missed=", missed, " permille=", per_mille(ticks));

        // Where that frame went, in per-mille: the sections, then what they leave (rendering, Butano, audio).
        int counted = 0;

        for(int index = 0; index < SECTIONS_COUNT; ++index)
        {
            counted += previous_sections[index];

            if(previous_sections[index] > 0)
            {
                BN_LOG("KXMISS   ", section_names[index], "=", per_mille(previous_sections[index]));
            }
        }

        BN_LOG("KXMISS   other=", per_mille(ticks - counted));
        missed_frames += missed;
    }

    for(int index = 0; index < SECTIONS_COUNT; ++index)
    {
        previous_sections[index] = frame_sections[index];
        frame_sections[index] = 0;
    }

    ++frames;
}

void report()
{
    BN_LOG("KXBENCH frames=", frames, " missed=", missed_frames, " ticks_per_frame=", bn::timers::ticks_per_frame());
    BN_LOG("KXBENCH frame avg_permille=", per_mille(frame.total / bn::max(frames, 1)),
           " max_permille=", per_mille(frame.max));

    for(int index = 0; index < SECTIONS_COUNT; ++index)
    {
        const stats& current = sections[index];
        BN_LOG("KXBENCH ", section_names[index], " avg_ticks=", current.total / bn::max(frames, 1),
               " max_ticks=", current.max, " max_permille=", per_mille(current.max));
    }

    BN_LOG("KXBENCH done");
}

}

#endif
