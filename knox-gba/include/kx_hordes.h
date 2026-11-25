#ifndef KX_HORDES_H
#define KX_HORDES_H

#include "bn_common.h"
#include "bn_fixed_point.h"

namespace kx
{

class game;

namespace save
{
    class reader;
    class writer;
}

// Hordes as head counts (spec: Zombies > Hordes and migration): every macro cell holds a 1-byte count; within about
// 2 chunks of the player the counts materialise as individual zombies, and zombies left behind dissolve back into
// the count of the cell they are in. Killed zombies are simply gone, so kills lower the counts for good.
namespace hordes
{
    // Fills the counts of a new generated world.
    void seed_counts(uint32_t seed);

    // Zombies counted near a point (the cells within active range), for the debug map and the edge indicators.
    [[nodiscard]] int counted_near(const bn::fixed_point& position);

    // Once per frame while playing a generated world: dissolves far zombies and materialises nearby counts.
    void update(game& owner);

    // At game start and after teleports: materialises the surroundings at once.
    void fill(game& owner);

    // A noise in the world: loud ones heat their cell, and hot cells draw hordes when they migrate.
    void noise(const bn::fixed_point& position, int radius_tiles, int hour);

    // Every in-game hour: starts the migration pass (run over the next frames by update), or runs it at once.
    void hour_passed(int hour, bool at_once);

    // Completes a migration pass in progress (before a save).
    void finish_migration();

    // Save block: the counts, the recent noise and the hour of the last pass. Zombies on screen are not counted;
    // the caller adds them.
    void save(save::writer& out);
    void load(save::reader& in);

    // All counted zombies in the world (debug and bench checks).
    [[nodiscard]] int total_counted();

    // Directions from the player: east, north-east, north, north-west, west, south-west, south, south-east.
    constexpr int edge_directions = 8;

    // Zombies waiting off-screen in a direction because the sprite pool is full (updated every horde step).
    [[nodiscard]] int edge_count(int direction);
}

}

#endif
