#include "kx_world_gen.h"

#include "kx_buildings.h"
#include "kx_hash.h"
#include "kx_macro_map.h"
#include "kx_world_detail.h"

namespace kx::world_gen
{

namespace
{
    [[nodiscard]] constexpr int log2(int value)
    {
        int result = 0;

        while((1 << result) < value)
        {
            ++result;
        }

        return result;
    }

    constexpr uint32_t elevation_seed = 0x454c4556u;
    constexpr uint32_t moisture_seed = 0x4d4f4953u;
    constexpr uint32_t octave_seed_step = 0x3c6ef372u;
}

// Value noise: random 0..255 on a lattice of 2^shift tiles, smoothly interpolated in between. Along one tile row
// within one lattice cell only the horizontal weight changes, so the row's two vertically interpolated edges are
// worked out once (every octave is at least a chunk wide, and chunks start on lattice lines).
octave_row make_octave(uint32_t seed, int cell_x, int tile_y, int shift, int weight)
{
    int cell_y = tile_y >> shift;
    int fy = smooth(((tile_y & ((1 << shift) - 1)) << 8) >> shift);
    int a = int(hash_xy(seed, cell_x, cell_y) & 255);
    int b = int(hash_xy(seed, cell_x + 1, cell_y) & 255);
    int c = int(hash_xy(seed, cell_x, cell_y + 1) & 255);
    int d = int(hash_xy(seed, cell_x + 1, cell_y + 1) & 255);

    octave_row result;
    result.left = a + (((c - a) * fy) >> 8);
    result.right = b + (((d - b) * fy) >> 8);
    result.shift = shift;
    result.weight = weight;
    return result;
}

void make_noise_row(uint32_t seed, int tile_x, int tile_y, noise_row& output)
{
    for(int index = 0; index < elevation_octaves; ++index)
    {
        int shift = log2(elevation_scales[index]);
        output.elevation[index] = make_octave((seed ^ elevation_seed) + uint32_t(index) * octave_seed_step,
                                              tile_x >> shift, tile_y, shift, elevation_weights[index]);
    }

    for(int index = 0; index < moisture_octaves; ++index)
    {
        int shift = log2(moisture_scales[index]);
        output.moisture[index] = make_octave((seed ^ moisture_seed) + uint32_t(index) * octave_seed_step,
                                             tile_x >> shift, tile_y, shift, moisture_weights[index]);
    }
}

int row_elevation(const noise_row& row, int tile_x)
{
    int value = evaluate(row.elevation, tile_x);

    if(tile_x > coast_start_tile)
    {
        value -= ((tile_x - coast_start_tile) * coast_falloff_per_64_tiles) >> 6;
    }

    return value < 0 ? 0 : value;
}

int row_moisture(const noise_row& row, int tile_x)
{
    return evaluate(row.moisture, tile_x);
}

int elevation(uint32_t seed, int tile_x, int tile_y)
{
    noise_row row;
    make_noise_row(seed, tile_x, tile_y, row);
    return row_elevation(row, tile_x);
}

int moisture(uint32_t seed, int tile_x, int tile_y)
{
    noise_row row;
    make_noise_row(seed, tile_x, tile_y, row);
    return row_moisture(row, tile_x);
}

}

namespace kx::world::detail
{

// Helpers only reach IWRAM when marked BN_CODE_IWRAM (unmarked ones run as ARM code from ROM, several times
// slower), so every non-trivial one below is.
namespace
{
    constexpr int cell_centre = macro::cell_tiles / 2;
    constexpr int road_half = world_gen::road_half_width;
    constexpr int lot_size = world_gen::lot_size;
    constexpr int lot_offset = world_gen::lot_first_offset;
    constexpr int empty_lot = int(building::COUNT);
    constexpr int no_lot = -1;

    // Tree kind of a walls graphic (its trunk), for the canopies.
    struct tree_table
    {
        uint8_t values[int(world_gfx::COUNT)];
    };

    [[nodiscard]] constexpr tree_table make_tree_table()
    {
        tree_table result = {};

        for(int kind = 1; kind < int(tree_kind::COUNT); ++kind)
        {
            result.values[world_tree_trunks[kind]] = uint8_t(kind);
        }

        return result;
    }

    constexpr tree_table trees_of = make_tree_table();

    [[nodiscard]] inline uint8_t pick_tree(const world_gen::tree_mix& mix, int roll)
    {
        int kind = roll < mix.oak ? int(tree_kind::OAK) : roll < mix.oak + mix.pine ? int(tree_kind::PINE) :
                                                                                      int(tree_kind::BIRCH);
        return world_tree_trunks[kind];
    }

    [[nodiscard]] inline world_gfx grass_variant(int x, int y)
    {
        return (((x * 7) + (y * 13) + ((x * y) >> 2)) & 3) == 0 ? world_gfx::GRASS_B : world_gfx::GRASS_A;
    }

    // Natural grass: lush on wet ground, dry on high ground, otherwise plain with tufts and flowers scattered through
    // it; the noise makes the lush and dry stretches large, soft-edged patches.
    [[nodiscard]] inline world_gfx natural_grass(int elevation, int moisture, uint32_t hash)
    {
        int roll = int((hash >> 16) & 255);

        if(elevation > world_gen::dry_grass_elevation)
        {
            return roll < world_gen::grass_tufts ? world_gfx::GRASS_B : world_gfx::GRASS_DRY;
        }

        if(moisture > world_gen::lush_grass_moisture)
        {
            return roll < world_gen::grass_flowers ? world_gfx::GRASS_FLOWERS : world_gfx::GRASS_LUSH;
        }

        return roll < world_gen::grass_flowers ? world_gfx::GRASS_FLOWERS :
               roll < world_gen::grass_flowers + world_gen::grass_tufts ? world_gfx::GRASS_B : world_gfx::GRASS_A;
    }

    [[nodiscard]] inline bool settled(macro::zone value)
    {
        return value == macro::zone::VILLAGE || value == macro::zone::TOWN || value == macro::zone::MILITARY;
    }

    // Road graphic of a tile from its macro cell's road mask, or EMPTY. Roads run along the cell's centre lines
    // from the centre to each side in the mask; the centre line is dashed except where roads cross. vertical_only
    // tells a bridge which way its planks run.
    [[nodiscard]] BN_CODE_IWRAM world_gfx road_gfx(int roads, bool pavements, int local_x, int local_y,
                                                   bool& vertical_only)
    {
        int offset_x = local_x - cell_centre;
        int offset_y = local_y - cell_centre;
        bool vertical = (offset_x >= -road_half && offset_x <= road_half) &&
                        (((roads & macro::road_north) && offset_y <= road_half) ||
                         ((roads & macro::road_south) && offset_y >= -road_half));
        bool horizontal = (offset_y >= -road_half && offset_y <= road_half) &&
                          (((roads & macro::road_west) && offset_x <= road_half) ||
                           ((roads & macro::road_east) && offset_x >= -road_half));
        vertical_only = vertical && ! horizontal;

        if(vertical || horizontal)
        {
            if(vertical_only && offset_x == 0 && ! (local_y & 1))
            {
                return world_gfx::ROAD_LINE_V;
            }

            if(horizontal && ! vertical && offset_y == 0 && ! (local_x & 1))
            {
                return world_gfx::ROAD_LINE_H;
            }

            return world_gfx::ROAD;
        }

        if(pavements)
        {
            int edge = road_half + 1;
            bool beside_vertical = (offset_x == -edge || offset_x == edge) &&
                                   (((roads & macro::road_north) && offset_y <= edge) ||
                                    ((roads & macro::road_south) && offset_y >= -edge));
            bool beside_horizontal = (offset_y == -edge || offset_y == edge) &&
                                     (((roads & macro::road_west) && offset_x <= edge) ||
                                      ((roads & macro::road_east) && offset_x >= -edge));

            if(beside_vertical || beside_horizontal)
            {
                return world_gfx::PAVEMENT;
            }
        }

        return world_gfx::EMPTY;
    }

    // A set piece on one of a cell's roads (milestone 14): which, along which road, and which way from the
    // crossroads.
    struct road_place
    {
        road_piece kind;
        bool vertical;          // on the north-south road
        int sign;               // +1 towards the east or south side, -1 the other way
    };

    // What a chunk row needs from its macro cell (a chunk row never crosses one).
    struct cell_info
    {
        int cell_x;
        int cell_y;
        macro::zone zone;
        int roads;
        int quarter;
        int clearing;
        bool pavements;
        bool cabin;             // a wild cell with a cabin (milestone 14)
        bool lots;              // has building lots: settlements, farms and cabin cells
        int pieces;             // set pieces on its roads (milestone 14), up to 2
        road_place piece[2];
    };

    // The building on a lot, worked out once per lot and row.
    struct lot_info
    {
        int key = no_lot - 1;       // which lot of the cell; no_lot - 1: nothing cached yet
        int left = 0;               // the building's top-left tile in the cell
        int top = 0;
        int width = 0;              // 0: no building
        int height = 0;
        const uint8_t* ground = nullptr;
        const uint8_t* walls = nullptr;
        const uint8_t* roofs = nullptr;
        const building_shape* shape = nullptr;
        uint8_t style = 0;          // chunk_slot::style
        uint8_t kind = uint8_t(building::COUNT);
    };

    // Distance (squared, tiles) from a settlement's centre: a 1-cell settlement is centred on its cell, a 2x2 one on
    // the corner its four cells share.
    [[nodiscard]] int settlement_distance_squared(int quarter, int local_x, int local_y)
    {
        int centre_x = cell_centre;
        int centre_y = cell_centre;

        if(quarter >= 0)
        {
            centre_x = (quarter & 1) ? 0 : macro::cell_tiles;
            centre_y = (quarter & 2) ? 0 : macro::cell_tiles;
        }

        int dx = local_x - centre_x;
        int dy = local_y - centre_y;
        return (dx * dx) + (dy * dy);
    }

    [[nodiscard]] int clearing_radius(macro::zone zone, bool cabin)
    {
        return cabin ? world_gen::cabin_clearing :
               zone == macro::zone::TOWN ? world_gen::town_clearing :
               zone == macro::zone::MILITARY ? world_gen::military_clearing :
               zone == macro::zone::FARM ? world_gen::farm_area : world_gen::village_clearing;
    }

    [[nodiscard]] const int* building_weights(macro::zone zone, bool landmark)
    {
        switch(zone)
        {

        case macro::zone::WILDERNESS:
            return world_gen::cabin_landmarks;

        case macro::zone::TOWN:
            return landmark ? world_gen::town_landmarks : world_gen::town_homes;

        case macro::zone::MILITARY:
            return landmark ? world_gen::military_landmarks : world_gen::military_homes;

        case macro::zone::FARM:
            return landmark ? world_gen::farm_landmarks : world_gen::farm_homes;

        default:
            return landmark ? world_gen::village_landmarks : world_gen::village_homes;
        }
    }

    // Which lot a tile of the cell is on: side of the centre lines (bits 0-1), then depth along x and y (bits 2-5);
    // no_lot on the roads and beyond the last lot.
    [[nodiscard]] int lot_key(int local_x, int local_y)
    {
        int offset_x = local_x - cell_centre;
        int offset_y = local_y - cell_centre;
        int depth_x = (offset_x < 0 ? -offset_x : offset_x) - lot_offset;
        int depth_y = (offset_y < 0 ? -offset_y : offset_y) - lot_offset;

        if(depth_x < 0 || depth_y < 0 || depth_x >= lot_size * world_gen::lots_deep ||
           depth_y >= lot_size * world_gen::lots_deep)
        {
            return no_lot;
        }

        return (offset_x > 0) | ((offset_y > 0) << 1) | ((depth_x / lot_size) << 2) | ((depth_y / lot_size) << 4);
    }

    // Picks and places a lot's building: inside the clearing, on dry land, its door towards the road beside it.
    BN_CODE_IWRAM void plan_lot(uint32_t seed, const cell_info& cell, int key, lot_info& lot)
    {
        lot.key = key;
        lot.width = 0;

        bool east = key & 1;
        bool south = key & 2;
        int lot_x = (key >> 2) & 3;
        int lot_y = (key >> 4) & 3;
        int left = east ? cell_centre + lot_offset + (lot_x * lot_size) :
                          cell_centre - lot_offset - (lot_x * lot_size) - (lot_size - 1);
        int top = south ? cell_centre + lot_offset + (lot_y * lot_size) :
                          cell_centre - lot_offset - (lot_y * lot_size) - (lot_size - 1);
        int centre_x = left + (lot_size / 2);
        int centre_y = top + (lot_size / 2);
        uint32_t roll = hash_xy(seed ^ 0x4c4f5453u, (cell.cell_x * 64) + key, cell.cell_y);

        // The landmark lot: one of the four next to the crossroads; a farm has no other.
        // Milestone 14: towns and the military base have landmarks on all four lots by the crossroads, villages on
        // two (more kinds of landmark to go round), farms and cabins on one.
        int landmark_key = int(hash_xy(seed ^ 0x4c4e444du, cell.cell_x, cell.cell_y) & 3);
        bool inner = (key >> 2) == 0;
        bool town_extra = cell.zone == macro::zone::TOWN &&
                          int((roll >> 20) % 100) < world_gen::town_outer_landmark_percent;
        bool landmark = cell.zone == macro::zone::TOWN || cell.zone == macro::zone::MILITARY ? inner || town_extra :
                        cell.zone == macro::zone::VILLAGE ? inner && (key == landmark_key || key == (landmark_key ^ 3)) :
                                                            key == landmark_key;

        // A farm has its landmark and maybe one more building on the inner lots; a cabin cell only the cabin.
        if((cell.zone == macro::zone::FARM && ! landmark && (key >> 2) != 0) ||
           (cell.zone == macro::zone::WILDERNESS && ! landmark))
        {
            return;
        }

        int radius = cell.clearing - world_gen::lot_clearing_margin;

        if(! cell.cabin && settlement_distance_squared(cell.quarter, centre_x, centre_y) >= radius * radius)
        {
            return;
        }

        int tile_x = (cell.cell_x * macro::cell_tiles) + centre_x;
        int tile_y = (cell.cell_y * macro::cell_tiles) + centre_y;

        if(world_gen::elevation(seed, tile_x, tile_y) <= world_gen::lot_dry_elevation)
        {
            return;
        }

        const int* weights = building_weights(cell.zone, landmark);
        int pick = int(roll % 100);
        int chosen = 0;

        while(chosen < empty_lot && pick >= weights[chosen])
        {
            pick -= weights[chosen];
            ++chosen;
        }

        // The village set piece (milestone 14): the church on its first landmark lot.
        if(cell.zone == macro::zone::VILLAGE && key == landmark_key)
        {
            chosen = int(building::CHURCH);
        }

        if(chosen >= empty_lot)
        {
            return;
        }

        // The door faces the road along the lot's inner side (or its other inner side), else the cell's centre.
        bool road_beside_x = lot_x == 0 && (cell.roads & (south ? macro::road_south : macro::road_north));
        bool road_beside_y = lot_y == 0 && (cell.roads & (east ? macro::road_east : macro::road_west));
        bool face_x = road_beside_x || (! road_beside_y && lot_x == 0);
        building_facing facing = face_x ? (east ? building_facing::WEST : building_facing::EAST) :
                                          (south ? building_facing::NORTH : building_facing::SOUTH);
        // One of its 8 layouts (mirrored or not), and its roof, wall and floor colours.
        int layout = int(facing) + (((roll >> 8) & 1) ? int(building_facing::COUNT) : 0);
        const building_shape& shape = building_shapes[chosen][layout];
        lot.kind = uint8_t(chosen);
        lot.style = uint8_t((roll >> 12) & ((1 << (style_bits * 3)) - 1));
        lot.width = shape.width;
        lot.height = shape.height;
        lot.ground = building_ground + shape.offset;
        lot.walls = building_walls + shape.offset;
        lot.roofs = building_roofs + shape.offset;
        lot.shape = &shape;

        // A tile of yard between the road side and the front; centred along the road.
        lot.left = facing == building_facing::WEST ? left + 1 :
                   facing == building_facing::EAST ? left + lot_size - 1 - shape.width :
                                                     left + ((lot_size - shape.width) / 2);
        lot.top = facing == building_facing::NORTH ? top + 1 :
                  facing == building_facing::SOUTH ? top + lot_size - 1 - shape.height :
                                                     top + ((lot_size - shape.height) / 2);
    }

    // The wilderness find of a tile's block, worked out once per block and row.
    struct scatter_info
    {
        int block_x = -1;
        int block_y = -1;
        int spot_x = 0;             // world tile of the block's spot
        int spot_y = 0;
        uint8_t walls = 0;          // what lies there, or EMPTY
        bool campsite = false;      // a campsite: the tent on the spot, a campfire and a crate below it
    };

    [[nodiscard]] BN_CODE_IWRAM uint8_t scatter_at(uint32_t seed, int tile_x, int tile_y, scatter_info& scatter)
    {
        constexpr int block = world_gen::scatter_block;
        int block_x = tile_x / block;
        int block_y = tile_y / block;

        if(block_x != scatter.block_x || block_y != scatter.block_y)
        {
            uint32_t hash = hash_xy(seed ^ 0x46494e44u, block_x, block_y);
            int kind = int((hash >> 16) % 100);
            scatter.block_x = block_x;
            scatter.block_y = block_y;
            scatter.spot_x = (block_x * block) + int(hash % block);
            scatter.spot_y = (block_y * block) + int((hash >> 8) % block);
            constexpr int body_below = world_gen::scatter_tent + world_gen::scatter_body;
            constexpr int campsite_below = body_below + world_gen::scatter_campsite;
            constexpr int stand_below = campsite_below + world_gen::scatter_stand;
            scatter.campsite = kind >= body_below && kind < campsite_below;
            scatter.walls = int((hash >> 24) % 100) >= world_gen::scatter_percent ? uint8_t(world_gfx::EMPTY) :
                            kind < world_gen::scatter_tent ? uint8_t(world_gfx::TENT) :
                            kind < body_below ? uint8_t(world_gfx::BODY) :
                            kind < campsite_below ? uint8_t(world_gfx::TENT) :
                            kind < stand_below ? uint8_t(world_gfx::HUNTER_STAND) : uint8_t(world_gfx::CRATE);

            // A campsite keeps its three tiles inside the block.
            if(scatter.campsite)
            {
                scatter.spot_x = bn::clamp(scatter.spot_x, (block_x * block) + 1, (block_x * block) + block - 2);
                scatter.spot_y = bn::min(scatter.spot_y, (block_y * block) + block - 3);
            }
        }

        if(tile_x == scatter.spot_x && tile_y == scatter.spot_y)
        {
            return scatter.walls;
        }

        if(scatter.campsite && scatter.walls && tile_y == scatter.spot_y + 2)
        {
            return tile_x == scatter.spot_x + 1 ? uint8_t(world_gfx::CAMPFIRE) :
                   tile_x == scatter.spot_x - 1 ? uint8_t(world_gfx::CRATE) : uint8_t(world_gfx::EMPTY);
        }

        return uint8_t(world_gfx::EMPTY);
    }

    // The walls graphic of a set piece on a tile of a cell, or EMPTY; on its road tiles (ground given) it may
    // stain the road with blood or a corpse.
    // Kept out of line: few tiles get here, and inlined it would keep classify_tile out of its row loop.
    [[nodiscard]] [[gnu::noinline]] BN_CODE_IWRAM uint8_t piece_at(const cell_info& cell, int local_x, int local_y,
                                                                   uint8_t* ground)
    {
        for(int index = 0; index < cell.pieces; ++index)
        {
            const road_place& place = cell.piece[index];
            int along = place.vertical ? local_y : local_x;
            int across = place.vertical ? local_x : local_y;
            int lane = (across - cell_centre) * place.sign;
            int decal = -1;
            uint8_t result = 0;

            switch(place.kind)
            {

            case road_piece::BUS:
            {
                // Drawn front to the east whichever way it lies; two lanes, the door on the lower row.
                int left = place.sign > 0 ? cell_centre + world_gen::bus_distance :
                                            cell_centre - world_gen::bus_distance - 3;
                int top = place.sign > 0 ? cell_centre : cell_centre - 1;
                int column = local_x - left;
                int row = local_y - top;

                if(unsigned(column) < 4 && unsigned(row) < 2)
                {
                    result = uint8_t(int(world_gfx::BUS_00) + (row * 4) + column);
                }
                else if(column == 2 && row == 2)
                {
                    result = uint8_t(world_gfx::BODY);
                }
                else if(column == 4 && row == 0)
                {
                    decal = int(world_decal::BLOOD_A);
                }
                else if(column == -2 && row == 1)
                {
                    decal = int(world_decal::CORPSE);
                }

                break;
            }

            case road_piece::BARRICADE:
            {
                // Two staggered rows of road blocks (a chicane), sandbags along one side with a police locker.
                int step = ((along - cell_centre) * place.sign) - world_gen::barricade_distance;

                if((step == 0 && lane >= -2 && lane <= 0) || (step == 3 && lane >= 0 && lane <= 2))
                {
                    result = uint8_t(world_gfx::ROADBLOCK);
                }
                else if(lane == -3 && step >= -1 && step <= 4)
                {
                    result = uint8_t(world_gfx::SANDBAGS);
                }
                else if(lane == -3 && step == 5)
                {
                    result = uint8_t(world_gfx::LOCKER);
                }
                else if(lane == -2 && step == 6)
                {
                    result = uint8_t(world_gfx::BODY);
                }
                else if(lane == 1 && step == 1)
                {
                    decal = int(world_decal::CORPSE);
                }
                else if(lane == -1 && step == 2)
                {
                    decal = int(world_decal::BLOOD_A);
                }

                break;
            }

            case road_piece::CHECKPOINT:
            {
                // A barrier over two lanes, sandbags out to both sides, a watchtower and a military crate.
                int step = ((along - cell_centre) * place.sign) - world_gen::checkpoint_distance;
                int side = lane < 0 ? -lane : lane;

                if(step == 0 && (lane == -1 || lane == 0))
                {
                    result = uint8_t(world_gfx::BARRIER);
                }
                else if(step == 0 && side >= 3 && side <= 5)
                {
                    result = uint8_t(world_gfx::SANDBAGS);
                }
                else if(step == -2 && lane == 4)
                {
                    result = uint8_t(world_gfx::WATCHTOWER);
                }
                else if(step == -2 && lane == -4)
                {
                    result = uint8_t(world_gfx::CRATE);
                }

                break;
            }

            default:
                break;
            }

            if(result)
            {
                return result;
            }

            if(decal >= 0 && ground)
            {
                if(uint8_t stained = world_gfx_decals[*ground][decal])
                {
                    *ground = stained;
                }
            }
        }

        return uint8_t(world_gfx::EMPTY);
    }

    // A farm plot's ground: crops, plowed soil or fallow grass, the same for the whole plot; grass on the lanes.
    [[nodiscard]] BN_CODE_IWRAM world_gfx farm_gfx(uint32_t seed, int tile_x, int tile_y)
    {
        constexpr int step_x = world_gen::plot_width + 1;
        constexpr int step_y = world_gen::plot_height + 1;
        int local_x = tile_x & (macro::cell_tiles - 1);
        int local_y = tile_y & (macro::cell_tiles - 1);

        if(local_x % step_x == world_gen::plot_width || local_y % step_y == world_gen::plot_height)
        {
            return grass_variant(tile_x, tile_y);
        }

        int roll = int(hash_xy(seed ^ 0x504c4f54u, (tile_x - local_x) + (local_x / step_x),
                               (tile_y - local_y) + (local_y / step_y)) % 100);
        return roll < world_gen::crops_percent ? world_gfx::CROPS :
               roll < world_gen::crops_percent + world_gen::soil_percent ? world_gfx::SOIL :
                                                                         grass_variant(tile_x, tile_y);
    }

    // A farm's or village's well (milestone 13): on one corner of the crossroads, just past the pavements.
    [[nodiscard]] BN_CODE_IWRAM bool well_at(uint32_t seed, const cell_info& cell, int local_x, int local_y)
    {
        if(cell.zone != macro::zone::FARM && cell.zone != macro::zone::VILLAGE)
        {
            return false;
        }

        uint32_t roll = hash_xy(seed ^ 0x57454c4cu, cell.cell_x, cell.cell_y);

        if(cell.zone == macro::zone::VILLAGE && int(roll % 100) >= world_gen::village_well_percent)
        {
            return false;
        }

        int corner = int((roll >> 8) & 3);
        int well_x = cell_centre + ((corner & 1) ? lot_offset : -lot_offset);
        int well_y = cell_centre + ((corner & 2) ? lot_offset : -lot_offset);
        return local_x == well_x && local_y == well_y;
    }

    // A template's furniture piece, or another one for the same room (per tile, from the seed).
    [[nodiscard]] BN_CODE_IWRAM uint8_t swap_furniture(uint8_t piece, uint32_t seed, int tile_x, int tile_y)
    {
        for(const world_gen::furniture_swap& swap : world_gen::furniture_swaps)
        {
            if(uint8_t(swap.from) == piece)
            {
                int roll = int(hash_xy(seed ^ 0x46555251u, tile_x, tile_y) % 100);

                if(roll < swap.percent[0])
                {
                    return uint8_t(swap.options[0]);
                }

                return roll < swap.percent[0] + swap.percent[1] ? uint8_t(swap.options[1]) : piece;
            }
        }

        return piece;
    }

    // Ground and walls graphic of one tile: roads (bridges over water) first, then buildings, then the natural
    // ground from the terrain noise, cleared around settlements and planted on farms, with its tree, rock or bush.
    BN_CODE_IWRAM void classify_tile(uint32_t seed, const world_gen::noise_row& noise, const cell_info& cell,
                                     lot_info& lot, scatter_info& scatter, int tile_x, int tile_y, uint8_t& ground,
                                     uint8_t& walls, uint8_t& roof, uint8_t& style)
    {
        int local_x = tile_x & (macro::cell_tiles - 1);
        int local_y = tile_y & (macro::cell_tiles - 1);
        macro::zone zone = cell.zone;
        int roads = cell.roads;
        int elevation = world_gen::row_elevation(noise, tile_x);
        bool water = elevation < world_gen::shallow_water_below;
        walls = uint8_t(world_gfx::EMPTY);
        roof = uint8_t(world_gfx::EMPTY);
        style = 0;

        if(roads)
        {
            bool vertical_only;
            world_gfx road = road_gfx(roads, cell.pavements, local_x, local_y, vertical_only);

            if(road == world_gfx::PAVEMENT && water)
            {
                road = world_gfx::EMPTY;
            }
            else if(road != world_gfx::EMPTY && road != world_gfx::PAVEMENT && water)
            {
                road = vertical_only ? world_gfx::BRIDGE_V : world_gfx::BRIDGE_H;
            }

            if(road != world_gfx::EMPTY)
            {
                ground = uint8_t(road);

                if(cell.pieces)
                {
                    walls = piece_at(cell, local_x, local_y, &ground);
                }

                return;
            }
        }

        bool settled_cell = cell.lots;
        uint8_t outside = 0;            // a template's outdoor furniture (graves) or a set piece off the road

        if(settled_cell)
        {
            int key = lot_key(local_x, local_y);

            if(key != lot.key)
            {
                if(key == no_lot)
                {
                    lot.key = no_lot;
                    lot.width = 0;
                }
                else
                {
                    plan_lot(seed, cell, key, lot);
                }
            }

            int x = local_x - lot.left;
            int y = local_y - lot.top;

            if(unsigned(x) < unsigned(lot.width) && unsigned(y) < unsigned(lot.height))
            {
                int index = (y * lot.width) + x;

                if(uint8_t building_tile = lot.ground[index])
                {
                    ground = building_tile;
                    walls = swap_furniture(lot.walls[index], seed, tile_x, tile_y);
                    roof = lot.roofs[index];
                    style = lot.style;
                    return;
                }

                outside = lot.walls[index];
            }
        }

        if(! outside && cell.pieces)
        {
            outside = piece_at(cell, local_x, local_y, nullptr);
        }

        if(elevation < world_gen::deep_water_below)
        {
            ground = uint8_t(world_gfx::DEEP_WATER);
            return;
        }

        if(water)
        {
            ground = uint8_t(world_gfx::SHALLOW_WATER);
            return;
        }

        // Moisture matters on dry, low ground, and for the edges of clearings.
        int moisture = settled_cell || (elevation >= world_gen::coast_below && elevation <= world_gen::hills_above) ?
                           world_gen::row_moisture(noise, tile_x) : 0;
        world_gen::ground_kind kind = world_gen::classify(elevation, moisture);

        uint32_t hash = hash_xy(seed ^ 0x54524545u, tile_x, tile_y);
        uint8_t grass = uint8_t(natural_grass(elevation, moisture, hash));

        // Settlements and farms: a round area with a wobbly edge.
        if(settled_cell)
        {
            int radius = cell.clearing + ((moisture - 128) >> world_gen::clearing_wobble_shift);

            if(settlement_distance_squared(cell.quarter, local_x, local_y) < radius * radius)
            {
                ground = kind == world_gen::ground_kind::COAST ? uint8_t(world_gfx::SAND) :
                         zone == macro::zone::FARM ? uint8_t(farm_gfx(seed, tile_x, tile_y)) : grass;

                if(outside)
                {
                    walls = outside;
                }
                else if(well_at(seed, cell, local_x, local_y))
                {
                    walls = uint8_t(world_gfx::WELL);
                }

                return;
            }
        }

        int roll = int(hash & 255);
        int kind_roll = int((hash >> 8) & 255);
        int trees = 0;
        int rocks = 0;
        int bushes = 0;
        const world_gen::tree_mix* mix = &world_gen::open_mix;

        switch(kind)
        {

        case world_gen::ground_kind::FOREST:
            ground = uint8_t(moisture > world_gen::forest_mud_moisture && (roll & 3) ? world_gfx::MUD :
                                                                                      world_gfx::FOREST_FLOOR);
            trees = world_gen::forest_trees;
            bushes = world_gen::forest_bushes;
            mix = elevation > world_gen::high_forest_elevation ? &world_gen::high_forest_mix : &world_gen::forest_mix;
            break;

        case world_gen::ground_kind::WOODS:
            ground = grass;
            trees = world_gen::woods_trees;
            bushes = world_gen::woods_bushes;
            mix = &world_gen::woods_mix;
            break;

        case world_gen::ground_kind::HILLS:
            ground = grass;
            trees = world_gen::hills_trees;
            rocks = world_gen::hills_rocks;
            bushes = world_gen::hills_bushes;
            mix = &world_gen::hills_mix;
            break;

        case world_gen::ground_kind::COAST:
            ground = uint8_t(world_gfx::SAND);
            break;

        default:
            ground = grass;
            trees = world_gen::open_trees;
            bushes = world_gen::open_bushes;
            break;
        }

        if(outside)
        {
            walls = outside;
            return;
        }

        // A wilderness find takes the place of whatever would grow there (hunter stands only among trees).
        if(uint8_t find = scatter_at(seed, tile_x, tile_y, scatter))
        {
            if(find != uint8_t(world_gfx::HUNTER_STAND) || kind == world_gen::ground_kind::FOREST ||
               kind == world_gen::ground_kind::WOODS)
            {
                walls = find;
                return;
            }
        }

        walls = roll < trees ? pick_tree(*mix, kind_roll) :
                roll < trees + rocks ? uint8_t(world_gfx::ROCK) :
                roll < trees + rocks + bushes ? uint8_t(world_gfx::BUSH) : uint8_t(world_gfx::EMPTY);
    }

    // The set pieces of a 2x2 settlement that lie in this cell (milestone 14): a town's coach on an east-west road
    // towards its centre and its police barricade on a road out; the military base's checkpoint on a road out. The
    // seed picks the cells, trying each quarter in turn until one has the road.
    void place_pieces(uint32_t seed, cell_info& cell)
    {
        cell.pieces = 0;

        if(cell.quarter < 0 || (cell.zone != macro::zone::TOWN && cell.zone != macro::zone::MILITARY))
        {
            return;
        }

        int base_x = cell.cell_x - (cell.quarter & 1);
        int base_y = cell.cell_y - (cell.quarter >> 1);
        uint32_t hash = hash_xy(seed ^ 0x50494543u, base_x, base_y);

        if(cell.zone == macro::zone::TOWN)
        {
            for(int step = 0; step < 4; ++step)
            {
                int quarter = int(hash + unsigned(step)) & 3;
                int inner = (quarter & 1) ? macro::road_west : macro::road_east;

                if(macro::roads_at(base_x + (quarter & 1), base_y + (quarter >> 1)) & inner)
                {
                    if(quarter == cell.quarter)
                    {
                        cell.piece[cell.pieces++] = { road_piece::BUS, false, inner == macro::road_east ? 1 : -1 };
                    }

                    break;
                }
            }
        }

        for(int step = 0; step < 4; ++step)
        {
            int quarter = int((hash >> 4) + unsigned(step)) & 3;
            int vertical_out = (quarter & 2) ? macro::road_south : macro::road_north;
            int horizontal_out = (quarter & 1) ? macro::road_east : macro::road_west;
            int roads = macro::roads_at(base_x + (quarter & 1), base_y + (quarter >> 1));
            int out = roads & (vertical_out | horizontal_out);

            if(out)
            {
                if(quarter == cell.quarter)
                {
                    int side = (out & vertical_out) && (! (out & horizontal_out) || ((hash >> 8) & 1)) ?
                                   vertical_out : horizontal_out;
                    cell.piece[cell.pieces++] = {
                        cell.zone == macro::zone::TOWN ? road_piece::BARRICADE : road_piece::CHECKPOINT,
                        side == vertical_out, (side & (macro::road_south | macro::road_east)) ? 1 : -1 };
                }

                break;
            }
        }
    }

    [[nodiscard]] cell_info make_cell_info(int tile_x, int tile_y)
    {
        cell_info cell;
        cell.cell_x = tile_x >> macro::cell_shift;
        cell.cell_y = tile_y >> macro::cell_shift;
        cell.zone = macro::zone_at(cell.cell_x, cell.cell_y);
        cell.roads = macro::roads_at(cell.cell_x, cell.cell_y);
        cell.quarter = macro::settlement_quarter(cell.cell_x, cell.cell_y);
        macro::land land = macro::land_at(cell.cell_x, cell.cell_y);
        cell.cabin = cell.zone == macro::zone::WILDERNESS &&
                     (land == macro::land::FOREST || land == macro::land::WOODS) &&
                     int(hash_xy(world::seed() ^ 0x4341424eu, cell.cell_x, cell.cell_y) % 100) <
                         world_gen::cabin_cell_percent;
        cell.lots = cell.zone != macro::zone::WILDERNESS || cell.cabin;
        cell.clearing = clearing_radius(cell.zone, cell.cabin);
        cell.pavements = settled(cell.zone);
        place_pieces(world::seed(), cell);
        return cell;
    }

    // The last row's cell (milestone 14): a chunk's rows share their macro cell, and make_cell_info runs from ROM.
    // Kept while the seed and the cell's macro data match, so a new world never sees an old one's.
    struct cell_cache
    {
        uint32_t seed = 0;
        int cell_x = -1;
        int cell_y = -1;
        uint16_t data = 0;
        cell_info info;
    };

    cell_cache last_cell;

    BN_CODE_IWRAM void classify_row(uint32_t seed, int chunk_x, int chunk_y, int row, uint8_t* ground, uint8_t* walls,
                                    uint8_t* roofs, uint8_t* styles)
    {
        int tile_x = chunk_x * chunk_tiles;
        int tile_y = (chunk_y * chunk_tiles) + row;
        world_gen::noise_row noise;
        world_gen::make_noise_row(seed, tile_x, tile_y, noise);

        int cell_x = tile_x >> macro::cell_shift;
        int cell_y = tile_y >> macro::cell_shift;
        uint16_t data = macro::detail::cell(cell_x, cell_y);

        if(last_cell.cell_x != cell_x || last_cell.cell_y != cell_y || last_cell.seed != seed ||
           last_cell.data != data)
        {
            last_cell.info = make_cell_info(tile_x, tile_y);
            last_cell.seed = seed;
            last_cell.cell_x = cell_x;
            last_cell.cell_y = cell_y;
            last_cell.data = data;
        }

        const cell_info& cell = last_cell.info;
        lot_info lot;
        scatter_info scatter;

        for(int x = 0; x < chunk_tiles; ++x)
        {
            classify_tile(seed, noise, cell, lot, scatter, tile_x + x, tile_y, ground[x], walls[x], roofs[x],
                          styles[x]);
        }
    }
}

namespace
{
    bool view_of(const cell_info& cell, int key, lot_view& out)
    {
        lot_info lot;
        plan_lot(world::seed(), cell, key, lot);

        if(! lot.width)
        {
            return false;
        }

        out.cell_x = cell.cell_x;
        out.cell_y = cell.cell_y;
        out.key = key;
        out.kind = building(lot.kind);
        out.left = (cell.cell_x * macro::cell_tiles) + lot.left;
        out.top = (cell.cell_y * macro::cell_tiles) + lot.top;
        out.width = lot.width;
        out.height = lot.height;
        out.walls = lot.walls;
        out.roofs = lot.roofs;
        return true;
    }
}

int road_pieces(int cell_x, int cell_y, road_piece_view (&out)[2])
{
    cell_info cell = make_cell_info(cell_x * macro::cell_tiles, cell_y * macro::cell_tiles);

    for(int index = 0; index < cell.pieces; ++index)
    {
        const road_place& place = cell.piece[index];
        int distance = place.kind == road_piece::BUS ? world_gen::bus_distance + 1 :
                       place.kind == road_piece::BARRICADE ? world_gen::barricade_distance + 2 :
                                                             world_gen::checkpoint_distance;
        int along = cell_centre + (place.sign * distance);
        out[index].kind = place.kind;
        out[index].tile_x = (cell_x * macro::cell_tiles) + (place.vertical ? cell_centre : along);
        out[index].tile_y = (cell_y * macro::cell_tiles) + (place.vertical ? along : cell_centre);
    }

    return cell.pieces;
}

// Not marked BN_CODE_IWRAM: only doors and keys look lots up, now and then, so these run from ROM.
bool lot_at(int tile_x, int tile_y, lot_view& out)
{
    cell_info cell = make_cell_info(tile_x, tile_y);

    if(! cell.lots)
    {
        return false;
    }

    int key = lot_key(tile_x & (macro::cell_tiles - 1), tile_y & (macro::cell_tiles - 1));
    return key != no_lot && view_of(cell, key, out);
}

bool lot_planned(int cell_x, int cell_y, int key, lot_view& out)
{
    if(unsigned(cell_x) >= unsigned(macro::cells) || unsigned(cell_y) >= unsigned(macro::cells))
    {
        return false;
    }

    cell_info cell = make_cell_info(cell_x * macro::cell_tiles, cell_y * macro::cell_tiles);
    return cell.lots && view_of(cell, key, out);
}

bool building_at(int tile_x, int tile_y, int& left, int& top, int& width, int& height)
{
    cell_info cell = make_cell_info(tile_x, tile_y);

    if(! cell.lots)
    {
        return false;
    }

    int local_x = tile_x & (macro::cell_tiles - 1);
    int local_y = tile_y & (macro::cell_tiles - 1);
    int key = lot_key(local_x, local_y);

    if(key == no_lot)
    {
        return false;
    }

    lot_info lot;
    plan_lot(world::seed(), cell, key, lot);

    int x = local_x - lot.left;
    int y = local_y - lot.top;

    if(unsigned(x) >= unsigned(lot.width) || unsigned(y) >= unsigned(lot.height) ||
       ! lot.roofs[(y * lot.width) + x])
    {
        return false;
    }

    // Just the roofed part, so trees on the template's open ground (forecourts, front steps) keep their canopies.
    left = (tile_x - local_x) + lot.left + lot.shape->roof_left;
    top = (tile_y - local_y) + lot.top + lot.shape->roof_top;
    width = lot.shape->roof_width;
    height = lot.shape->roof_height;
    return true;
}

void generate_rows(chunk_slot& slot, gen_state& state, int rows)
{
    uint32_t seed = world::seed();
    int chunk_x = slot.chunk_x;
    int chunk_y = slot.chunk_y;

    // Each step classifies one row, so every frame costs the same.
    if(! state.primed && rows > 0)
    {
        classify_row(seed, chunk_x, chunk_y, 0, state.ground[0], state.walls[0], state.roofs[0], state.styles[0]);
        state.primed = true;
        --rows;
    }

    for(int end = state.row + rows; state.row < end && state.row < chunk_tiles; ++state.row)
    {
        int row = state.row;
        int current = row & 1;
        int next = current ^ 1;

        // The row below (the next chunk's first row for the last one) decides the lower canopies.
        classify_row(seed, chunk_x, chunk_y, row + 1, state.ground[next], state.walls[next], state.roofs[next],
                     state.styles[next]);

        uint8_t* ground = slot.gfx[int(layer::GROUND)] + (row * chunk_tiles);
        uint8_t* walls = slot.gfx[int(layer::WALLS)] + (row * chunk_tiles);
        uint8_t* canopy = slot.gfx[int(layer::CANOPY)] + (row * chunk_tiles);
        uint8_t* style = slot.style + (row * chunk_tiles);

        for(int x = 0; x < chunk_tiles; ++x)
        {
            uint8_t wall = state.walls[current][x];
            style[x] = state.styles[current][x];
            ground[x] = state.ground[current][x];
            walls[x] = wall;
            uint8_t roof = state.roofs[current][x];
            canopy[x] = roof ? roof : world_tree_canopies[trees_of.values[wall]][trees_of.values[state.walls[next][x]]];
        }
    }
}

}
