#ifndef KX_WORLD_GEN_TUNING_H
#define KX_WORLD_GEN_TUNING_H

#include "kx_buildings.h"
#include "kx_world_gfx_ids.h"

namespace kx::world_gen
{

// World generation (spec: World). These shape a world at creation, so they are not live-edited like the other
// tuning tables: a change needs a new world.

// Noise: value noise on lattices of these sizes (tiles), weights out of 16. Values run 0..255.
constexpr int elevation_scales[] = { 512, 128, 32 };
constexpr int elevation_weights[] = { 9, 5, 2 };
constexpr int moisture_scales[] = { 256, 64 };
constexpr int moisture_weights[] = { 11, 5 };

// The sea comes in over the last tiles of the east edge (spec: coast is safe but poor).
constexpr int coast_start_tile = 3200;
constexpr int coast_falloff_per_64_tiles = 9;

// Terrain from elevation and moisture.
constexpr int deep_water_below = 72;
constexpr int shallow_water_below = 82;
constexpr int coast_below = 86;             // a band of open ground along the water
constexpr int hills_above = 182;
constexpr int forest_moisture = 150;
constexpr int woods_moisture = 126;         // scattered trees

// Trees per 256 tiles of each terrain; boulders on the hills; mud in the wettest forest.
constexpr int forest_trees = 72;
constexpr int woods_trees = 22;
constexpr int open_trees = 2;
constexpr int hills_trees = 10;
constexpr int hills_rocks = 14;
constexpr int forest_mud_moisture = 212;

// Grass: lush where moisture is above this, dry where elevation is; tufts and flowers per 256 tiles.
constexpr int lush_grass_moisture = 112;
constexpr int dry_grass_elevation = 168;
constexpr int grass_tufts = 40;
constexpr int grass_flowers = 12;

// Bushes per 256 tiles (after trees and rocks).
constexpr int forest_bushes = 6;
constexpr int woods_bushes = 10;
constexpr int open_bushes = 3;
constexpr int hills_bushes = 4;

// Which trees grow where, out of 256: oak, then pine, the rest birch. Forest above high_forest_elevation is
// mostly pine.
struct tree_mix
{
    int oak;
    int pine;
};

constexpr tree_mix forest_mix = { 150, 80 };
constexpr tree_mix high_forest_mix = { 40, 200 };
constexpr tree_mix woods_mix = { 130, 30 };
constexpr tree_mix open_mix = { 180, 0 };
constexpr tree_mix hills_mix = { 0, 180 };
constexpr int high_forest_elevation = 150;

// Settlements clear a round area of trees and rocks (radius in tiles from the settlement's centre); the moisture
// noise wobbles the edge by up to (moisture - 128) >> wobble_shift tiles. Outside it their cells stay natural.
constexpr int village_clearing = 22;
constexpr int town_clearing = 50;
constexpr int military_clearing = 46;
constexpr int clearing_wobble_shift = 3;

// Farms: plots of crops, plowed soil or fallow grass with grass lanes between them, inside a round area.
constexpr int farm_area = 25;
constexpr int plot_width = 11;              // tiles, then a 1-tile lane
constexpr int plot_height = 7;
constexpr int crops_percent = 55;
constexpr int soil_percent = 30;            // the rest lies fallow

// Settlements, placed before the roads (sizes in macro cells of 64x64 tiles).
constexpr int towns = 3;                    // 2x2 cells, inland, dangerous and rich
constexpr int villages = 9;                 // 1 cell
constexpr int military_bases = 1;           // 2x2 cells, far inland
constexpr int settlement_spacing = 7;       // minimum cells between settlement centres (Chebyshev)
constexpr int inland_cells = 40;            // towns and the base sit west of this cell column
constexpr int placement_attempts = 400;
constexpr int farm_percent = 45;            // chance of a farm cell next to each village side

// Roads: a minimum spanning tree over the settlements, plus extra links for loops.
constexpr int extra_links = 3;

// Roads in a chunk: 3 tiles wide on the macro cell's centre lines, with a pavement either side in settlements.
constexpr int road_half_width = 1;

// Buildings (spec: Towns and buildings): square lots line the roads out of each settlement cell's crossroads, two
// deep, starting beside the pavement. A lot is built on when its centre lies inside the clearing (less a margin)
// on dry land, its front door facing the road. One lot per cell (picked by the seed) holds the zone's landmark,
// the others homes; a farm has only the landmark lot. Weights in kx::building order, then an empty lot.
constexpr int lot_size = 14;
constexpr int lots_deep = 2;
constexpr int lot_first_offset = road_half_width + 2;       // tiles from the cell's centre line
constexpr int lot_clearing_margin = 4;
constexpr int lot_dry_elevation = coast_below;              // the lot's centre must be above this
// Weights per template in kx::building order (tools/assets/buildings.py), then an empty lot; each row sums to 100.
// Milestone 14: 25 more templates; farms may have a second, smaller building (farm_homes, on one of the inner
// lots), and some forest cells of the wild hold a cabin (cabin_landmarks).
// small_house large_house garage shop petrol_station barn police_station clinic cottage bungalow terraced_pair flat_block trailer farmhouse large_farmhouse holiday_cabin pub post_office small_school school supermarket motel warehouse fire_station pharmacy hardware_store diner barracks armory mess_hall farm_shed stable hunters_cabin church empty
constexpr int village_homes[] = { 28, 12, 8, 0, 0, 5, 0, 0, 14, 10, 6, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 12 };
static_assert(sizeof(village_homes) / sizeof(int) == int(building::COUNT) + 1);
constexpr int village_landmarks[] = { 0, 0, 5, 30, 25, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 15, 12, 13, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static_assert(sizeof(village_landmarks) / sizeof(int) == int(building::COUNT) + 1);
constexpr int town_homes[] = { 18, 18, 7, 5, 0, 0, 0, 0, 3, 7, 10, 11, 0, 0, 0, 0, 3, 2, 0, 0, 0, 0, 0, 0, 2, 2, 3, 0, 0, 0, 0, 0, 0, 0, 9 };
static_assert(sizeof(town_homes) / sizeof(int) == int(building::COUNT) + 1);
constexpr int town_landmarks[] = { 0, 0, 0, 6, 6, 0, 10, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 10, 8, 8, 8, 9, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0 };
static_assert(sizeof(town_landmarks) / sizeof(int) == int(building::COUNT) + 1);
constexpr int military_homes[] = { 0, 5, 20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 35, 10, 15, 0, 0, 0, 0, 15 };
static_assert(sizeof(military_homes) / sizeof(int) == int(building::COUNT) + 1);
constexpr int military_landmarks[] = { 0, 0, 0, 0, 0, 0, 25, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 25, 30, 20, 0, 0, 0, 0, 0 };
static_assert(sizeof(military_landmarks) / sizeof(int) == int(building::COUNT) + 1);
constexpr int farm_landmarks[] = { 10, 0, 0, 0, 0, 35, 0, 0, 0, 0, 0, 0, 0, 25, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 5, 0, 0, 0 };
static_assert(sizeof(farm_landmarks) / sizeof(int) == int(building::COUNT) + 1);
constexpr int farm_homes[] = { 0, 0, 0, 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 20, 10, 0, 0, 55 };
static_assert(sizeof(farm_homes) / sizeof(int) == int(building::COUNT) + 1);
constexpr int cabin_landmarks[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 35, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 55, 0, 0 };
static_assert(sizeof(cabin_landmarks) / sizeof(int) == int(building::COUNT) + 1);

// Cabins in the wild (milestone 14): this share of forest and woods cells holds one, on a lot beside the cell's
// centre (whatever the clearing), with a small grassy clearing round the centre.
constexpr int cabin_cell_percent = 3;

// Towns: this share of the lots beyond the four by the crossroads hold a landmark too (milestone 14).
constexpr int town_outer_landmark_percent = 35;
constexpr int cabin_clearing = 12;

// Wells (milestone 13): every farm and this share of villages have one, on a corner of the crossroads (the lot
// corner a building never covers: the yard row or the margin), which corner from the seed.
constexpr int village_well_percent = 50;

// Wilderness finds (spec: density rule, scatter tables): each block of tiles has one spot, used this often, for a
// tent, a dead survivor or a supply crate, on open dry ground outside settlements.
// Milestone 14 adds campsites (a tent, a cold campfire and a crate) and, in forest and woods, hunter stands (a
// raised seat over a container; elsewhere the spot stays empty).
constexpr int scatter_block = 12;
constexpr int scatter_percent = 35;
constexpr int scatter_tent = 22;            // of 100: tents, bodies, campsites, hunter stands, the rest crates
constexpr int scatter_body = 15;
constexpr int scatter_campsite = 15;
constexpr int scatter_stand = 18;

// Set pieces (spec: one hand-made set piece per town tier; milestone 14). The village church takes the village's
// landmark lot (building::CHURCH). Every town (all are 2x2 cells, the big-town tier) has a crashed coach on one of
// its inner roads and a police barricade on a road out; the military base a checkpoint on a road out. Distances in
// tiles from the crossroads of their cell, along the road.
constexpr int bus_distance = 6;             // the coach's near end
constexpr int barricade_distance = 18;
constexpr int checkpoint_distance = 16;
constexpr int piece_uniform_radius = 12;    // zombies that appear this close to the barricade or checkpoint
constexpr int piece_uniform_percent = 70;   // wear its uniform this often

// Furniture swaps: a template's piece may become another piece of the same room, picked per tile by the seed
// (percent chances of each option; otherwise the piece stays).
struct furniture_swap
{
    world_gfx from;
    world_gfx options[2];
    uint8_t percent[2];
};

constexpr furniture_swap furniture_swaps[] = {
    { world_gfx::KITCHEN_CABINET, { world_gfx::COUNTER, world_gfx::FRIDGE }, { 25, 15 } },
    { world_gfx::FRIDGE, { world_gfx::KITCHEN_CABINET, world_gfx::CHEST_FREEZER }, { 30, 12 } },  // 13: freezers
    { world_gfx::WARDROBE, { world_gfx::DESK, world_gfx::GUN_SAFE }, { 20, 20 } },   // milestone 12.7: safes
    { world_gfx::BATH_CABINET, { world_gfx::MED_CABINET, world_gfx::EMPTY }, { 30, 0 } },
    { world_gfx::TOOL_SHELF, { world_gfx::CRATE, world_gfx::LOCKER }, { 25, 15 } },
    { world_gfx::SHOP_SHELF, { world_gfx::CRATE, world_gfx::CHEST_FREEZER }, { 15, 15 } },
    { world_gfx::CRATE, { world_gfx::TOOL_SHELF, world_gfx::EMPTY }, { 30, 0 } },
    { world_gfx::DESK, { world_gfx::LOCKER, world_gfx::EMPTY }, { 25, 0 } },
    { world_gfx::TV, { world_gfx::EMPTY, world_gfx::DESK }, { 40, 10 } },    // milestone 13: some living rooms
};

// Chunk generation is spread over frames: one row of 32 tiles per frame (at most about 7% of a frame, inside the
// spec's 10% share). The 3 chunks a step into a new chunk needs take about 100 frames, well before the view reaches
// them at sprint speed.
constexpr int rows_per_frame = 1;

}

#endif
