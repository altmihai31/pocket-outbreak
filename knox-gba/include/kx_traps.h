#ifndef KX_TRAPS_H
#define KX_TRAPS_H

#include <cstdint>

namespace kx
{

namespace save
{
    class reader;
    class writer;
}

// Traps (spec: snare and box traps placed in forests catch small game overnight, Foraging skill). A box trap is
// built in build mode once the Trapping magazine is read; a snare (milestone 13) needs no magazine, only rope and a
// plank. Catches are lazy: each night since the trap was set is rolled from hash(seed, trap, day) when the trap is
// looked at, so the result is the same whenever it is checked. Snares catch rabbits; box traps rabbits or birds.
namespace trap_tuning
{
    constexpr int max_traps = 16;                   // box traps and snares together
    constexpr int catch_percent = 30;               // a night's chance of a catch
    constexpr int percent_per_level = 8;            // more per Foraging level (box traps)
    constexpr int snare_percent_per_level = 5;      // snares
    constexpr int catch_xp = 10;                    // Foraging XP for taking a catch
    constexpr int extra_meat_level = 3;             // a second piece of meat from this Foraging level
}

namespace traps
{
    void reset();

    // Traps set (of 16).
    [[nodiscard]] int count();

    bool add(int tile_x, int tile_y, int today, bool snare = false);

    // A snare rather than a box trap.
    [[nodiscard]] bool snare(int index);

    // What is in it once caught: a bird (box traps only, from the seed) or a rabbit.
    [[nodiscard]] bool caught_bird(int index);

    // The trap on a tile, or -1.
    [[nodiscard]] int at(int tile_x, int tile_y);

    // Something caught (rolled for each night since it was set, at a Foraging level).
    [[nodiscard]] bool caught(int index, int today, int foraging);

    // Takes the catch and sets the trap again.
    void reset_trap(int index, int today);

    void save(save::writer& out);
    void load(save::reader& in);
}

}

#endif
