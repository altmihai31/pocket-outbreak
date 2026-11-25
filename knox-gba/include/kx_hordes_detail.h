#ifndef KX_HORDES_DETAIL_H
#define KX_HORDES_DETAIL_H

#include "bn_common.h"

#include "kx_horde_tuning.h"
#include "kx_macro_map.h"

// Horde internals shared by kx_hordes.cpp and its IWRAM half (the migration pass). Not for use elsewhere.
namespace kx::hordes::detail
{

// Recent loud noise, by macro cell.
struct attraction
{
    int cell_x;
    int cell_y;
    int heat;
    int hour;           // last heard
};

extern attraction attractions[horde::max_attractions];
extern int attractions_count;

// The migration pass: zombies leaving a cell arrive in `incoming`, added to the counts when the pass ends, so nobody
// moves twice in one hour.
extern uint8_t incoming[macro::cells * macro::cells];
extern int migration_next;          // next cell of the pass (moves, then cells + index for arrivals), or -1
extern int migration_hour;

// Runs `count` cells of the pass: first the moves, then the arrivals; at its end the noise cools.
BN_CODE_IWRAM void migrate_cells(int count);

// Sum of a whole map of 1-byte counts (read a word at a time).
[[nodiscard]] BN_CODE_IWRAM int sum_counts(const uint8_t* counts);

constexpr int migration_steps = 2 * macro::cells * macro::cells;

}

#endif
