#ifndef KX_HORDE_TUNING_H
#define KX_HORDE_TUNING_H

namespace kx::horde
{

// Hordes (spec: Zombies > Hordes and migration). Every macro cell holds a zombie head count; near the player the
// counts become individual zombies and go back to being counts when left behind. These shape the world, so like
// world generation they are fixed values rather than a live-edited table.

// Head counts per cell at world creation, by zone (wilderness, farm, village, town, military): min, max. The
// spec's risk gradient: villages and the coast are quieter, towns and the base crowded. Water holds none; coast
// cells hold half.
constexpr int start_counts[][2] = { { 0, 4 }, { 2, 8 }, { 15, 30 }, { 60, 110 }, { 140, 200 } };
constexpr int coast_percent = 50;

// Materialising: within active_tiles of the player (about 2 chunks; spawns only land in generated chunks) the
// counts turn into zombies, a few per step, up to max_active at once (the 40-sprite pool, with CPU to spare for
// the flow field). They appear out of sight, at least min_spawn_tiles away, indoors when a try finds a floor.
// Zombies beyond dissolve_tiles, or in chunks no longer generated, become counts of the cell they are in again.
constexpr int active_tiles = 48;
constexpr int max_active = 32;
constexpr int step_frames = 10;
constexpr int spawns_per_step = 1;
constexpr int dissolves_per_step = 2;
constexpr int spawn_tries = 8;
constexpr int min_spawn_tiles = 12;
constexpr int dissolve_tiles = 56;

// Migration, every in-game hour (spec: counts move one cell along the gradient towards recent loud noise). Noises
// of at least loud_tiles add their radius to their cell's heat; cells with enough heat in the last memory_hours
// draw counts from up to attract_cells away: move_percent of each such cell's zombies step one cell towards the
// hottest pull. Elsewhere, a cell's zombies drift to a random neighbour now and then. Heat cools by a quarter an
// hour. The pass is spread over frames (cells_per_frame), a whole one at once while sleeping.
constexpr int loud_tiles = 4;
constexpr int max_attractions = 8;
constexpr int heat_threshold = 24;
constexpr int memory_hours = 6;
constexpr int attract_cells = 6;
constexpr int move_percent = 30;
constexpr int wander_percent = 6;          // chance a cell's zombies wander this hour
constexpr int wander_share = 20;            // percent of them that go
constexpr int cells_per_frame = 128;        // a pass takes 64 frames (moves, then arrivals)

// Edge indicators (spec: a horde larger than the sprite pool shows as its front rank plus off-screen noise and edge
// indicators). While the pool is full, the counts still waiting within active range are summed by direction in
// blocks of edge_block_tiles (blocks on screen skipped); a direction with at least edge_min zombies shows a red
// arrow at that screen edge, pulsing faster the bigger it is, and now and then a distant groan comes from there.
constexpr int edge_block_tiles = 16;
constexpr int edge_min = 3;
constexpr int edge_x = 110;                 // arrow positions from the screen centre, clear of the HUD
constexpr int edge_y = 50;
constexpr int edge_pulse_frames = 48;       // pulse period for a small horde; big ones down to a quarter
constexpr int edge_big = 60;                // zombies counted as "big" (fastest pulse, loudest groans)
constexpr int groan_min_frames = 90;
constexpr int groan_extra_frames = 150;

}

#endif
