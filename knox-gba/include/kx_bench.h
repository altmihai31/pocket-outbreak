#ifndef KX_BENCH_H
#define KX_BENCH_H

#ifndef KX_BENCH
    #define KX_BENCH 0
#endif

// The tester build (make DEBUG=1).
#ifndef KX_DEBUG
    #define KX_DEBUG 0
#endif

#ifndef KX_DUMP
    #define KX_DUMP 0
#endif

// CHECK=1 (milestone 14): benches compare the world renderer's visible cells with the world every 30 frames.
#ifndef KX_RENDER_CHECK
    #define KX_RENDER_CHECK 0
#endif

// Bench builds (make BENCH=1) time each game system with a hardware timer and log the totals through
// BN_LOG (mGBA's debug log) when the scripted route ends, so the numbers can be read without a screen.
#if KX_BENCH
    namespace kx::bench
    {
        void begin(int section);
        void end();
        void frame_end();
        void report();
    }

    #define KX_BENCH_BEGIN(section) kx::bench::begin(section)
    #define KX_BENCH_END() kx::bench::end()
#else
    #define KX_BENCH_BEGIN(section) do { } while(false)
    #define KX_BENCH_END() do { } while(false)
#endif

namespace kx::bench
{
    enum section : int
    {
        PLAYER,
        ZOMBIES,
        FX,
        VISUALS,
        CAMERA,
        BG_STREAM,
        HUD,
        WORLD_GEN,
        HORDES,
        FLOW,
        SAVE,
        VEHICLES,
        ANIMALS,        // animals plan stage 7: their update (their drawing is in VISUALS)
        SECTIONS_COUNT
    };
}

#endif
