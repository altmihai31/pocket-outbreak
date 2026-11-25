#include "kx_world.h"

#include "bn_algorithm.h"
#include "bn_assert.h"
#include "bn_math.h"
#include "bn_vector.h"

#include "kx_hash.h"
#include "kx_hordes.h"
#include "kx_macro_map.h"
#include "kx_save.h"
#include "kx_test_map.h"
#include "kx_world_detail.h"
#include "kx_world_gen.h"
#include "kx_world_gen_tuning.h"

#if KX_BENCH
    #include "bn_log.h"
    #include "bn_string_view.h"
#endif

namespace kx::world::detail
{
    BN_DATA_EWRAM_BSS chunk_slot slots[slots_count];
    obstacle obstacles[max_obstacles];
    int obstacle_count = 0;
}

namespace kx::world
{

using namespace detail;

namespace
{
    // Decals (blood, corpses, dropped items) written over generated ground; replayed when a chunk is generated
    // again. The oldest is forgotten when the list is full.
    struct decal_entry
    {
        int16_t tile_x;
        int16_t tile_y;
        uint8_t gfx;
        uint8_t layer;      // GROUND (forgotten when old) or WALLS (kept)
    };

    static_assert(sizeof(decal_entry) * max_decals + 4 <= max_decal_save_bytes);


    // In EWRAM: IWRAM is kept for code and the stack.
    BN_DATA_EWRAM_BSS bn::vector<decal_entry, max_decals> decals;
    uint32_t world_seed = 0;
    bool is_test = false;
    int centre_x = -1000;
    int centre_y = -1000;

    // The chunk being generated over several frames.
    int job_slot = -1;
    gen_state job;

    // ----- test map -----

    [[nodiscard]] char map_char(int x, int y)
    {
        if(x < 0 || y < 0 || x >= test_map_width || y >= test_map_height)
        {
            return 'W';
        }

        return test_map_rows[y][x];
    }

    [[nodiscard]] bool is_tree(char c)
    {
        return c == 'T' || c == 't';
    }

    [[nodiscard]] bool is_fence_or_wall(char c)
    {
        return c == 'F' || c == 'B';
    }

    [[nodiscard]] world_gfx grass_variant(int x, int y)
    {
        return (((x * 7) + (y * 13) + ((x * y) >> 2)) & 3) == 0 ? world_gfx::GRASS_B : world_gfx::GRASS_A;
    }

    void test_tile(int x, int y, uint8_t& ground, uint8_t& walls, uint8_t& canopy)
    {
        char c = map_char(x, y);
        world_gfx ground_gfx = grass_variant(x, y);
        world_gfx wall_gfx = world_gfx::EMPTY;

        switch(c)
        {

        case '.':
        case 't':
            break;

        case '#':
            ground_gfx = world_gfx::ROAD;
            break;

        case '=':
            ground_gfx = world_gfx::ROAD_LINE_H;
            break;

        case '!':
            ground_gfx = world_gfx::ROAD_LINE_V;
            break;

        case '_':
            ground_gfx = world_gfx::PAVEMENT;
            break;

        case 'o':
            ground_gfx = world_gfx::WOOD_FLOOR;
            break;

        case 'd':
            ground_gfx = world_gfx::WOOD_FLOOR;
            wall_gfx = map_char(x, y - 1) == 'B' && map_char(x, y + 1) == 'B' ? world_gfx::DOOR_TOP_OPEN :
                                                                                world_gfx::DOOR_FACE_OPEN;
            break;

        case 'f':
        case 'T':
            ground_gfx = world_gfx::FOREST_FLOOR;
            break;

        case 'm':
            ground_gfx = world_gfx::MUD;
            break;

        case '~':
            ground_gfx = world_gfx::SHALLOW_WATER;
            break;

        case 'W':
            ground_gfx = world_gfx::DEEP_WATER;
            break;

        case 'B':
            ground_gfx = world_gfx::WOOD_FLOOR;
            wall_gfx = map_char(x, y + 1) == 'B' ? world_gfx::WALL_TOP : world_gfx::WALL_FACE;
            break;

        case 'F':
            wall_gfx = is_fence_or_wall(map_char(x - 1, y)) || is_fence_or_wall(map_char(x + 1, y)) ?
                        world_gfx::FENCE_H : world_gfx::FENCE_V;
            break;

        case 'k':
        case 'R':
        case 'D':
        case 'h':
        case 'G':
        case 'Y':
            // Furniture: solid but see-through, on a wood floor.
            ground_gfx = world_gfx::WOOD_FLOOR;
            wall_gfx = c == 'k' ? world_gfx::KITCHEN_CABINET : c == 'R' ? world_gfx::FRIDGE :
                       c == 'D' ? world_gfx::WARDROBE : c == 'h' ? world_gfx::BATH_CABINET :
                       c == 'Y' ? world_gfx::BED : world_gfx::TOOL_SHELF;
            break;

        default:
            BN_ERROR("Invalid test map char: ", c, " at ", x, ", ", y);
            break;
        }

        if(is_tree(c))
        {
            wall_gfx = world_gfx::TREE_TRUNK;
        }

        // Canopy: the lower half of this tile's tree, and the upper half of the tree below.
        bool canopy_bottom = is_tree(c);
        bool canopy_top = is_tree(map_char(x, y + 1));
        world_gfx canopy_gfx = world_gfx::EMPTY;

        if(canopy_bottom && canopy_top)
        {
            canopy_gfx = world_gfx::CANOPY_FULL;
        }
        else if(canopy_bottom)
        {
            canopy_gfx = world_gfx::CANOPY_BOTTOM;
        }
        else if(canopy_top)
        {
            canopy_gfx = world_gfx::CANOPY_TOP;
        }

        ground = uint8_t(ground_gfx);
        walls = uint8_t(wall_gfx);
        canopy = uint8_t(canopy_gfx);
    }

    void test_rows(chunk_slot& slot, gen_state& state, int rows)
    {
        for(int end = state.row + rows; state.row < end && state.row < chunk_tiles; ++state.row)
        {
            int index = state.row * chunk_tiles;
            int tile_y = (slot.chunk_y * chunk_tiles) + state.row;

            for(int x = 0; x < chunk_tiles; ++x, ++index)
            {
                test_tile((slot.chunk_x * chunk_tiles) + x, tile_y, slot.gfx[int(layer::GROUND)][index],
                          slot.gfx[int(layer::WALLS)][index], slot.gfx[int(layer::CANOPY)][index]);
                slot.style[index] = 0;
            }
        }
    }

    // ----- chunk cache -----

    [[nodiscard]] chunk_slot& slot_of(int chunk_x, int chunk_y)
    {
        return slots[(chunk_x % window) + ((chunk_y % window) * window)];
    }

    // Runs rows of a slot's generation; when the last row is done, replays the chunk's decals.
    void run_rows(chunk_slot& slot, gen_state& state, int rows)
    {
        if(is_test)
        {
            test_rows(slot, state, rows);
        }
        else
        {
            generate_rows(slot, state, rows);
        }

        if(state.row >= chunk_tiles)
        {
            for(const decal_entry& decal : decals)
            {
                if((decal.tile_x >> chunk_shift) == slot.chunk_x && (decal.tile_y >> chunk_shift) == slot.chunk_y)
                {
                    slot.gfx[decal.layer][tile_index(decal.tile_x, decal.tile_y)] = decal.gfx;
                }
            }

            slot.ready = true;
        }
    }

    // Generates a slot's chunk to the end now.
    void finish(chunk_slot& slot)
    {
        if(slot.ready)
        {
            return;
        }

        if(job_slot >= 0 && &slots[job_slot] == &slot)
        {
            while(! slot.ready)
            {
                run_rows(slot, job, chunk_tiles);
            }

            job_slot = -1;
        }
        else
        {
            gen_state state;

            while(! slot.ready)
            {
                run_rows(slot, state, chunk_tiles);
            }
        }
    }

    // Assigns the 3x3 chunks around the centre to their slots (new ones start ungenerated).
    void recentre(int chunk_x, int chunk_y)
    {
        centre_x = chunk_x;
        centre_y = chunk_y;

        for(int y = chunk_y - 1; y <= chunk_y + 1; ++y)
        {
            for(int x = chunk_x - 1; x <= chunk_x + 1; ++x)
            {
                if(unsigned(x) >= unsigned(chunks_per_side) || unsigned(y) >= unsigned(chunks_per_side))
                {
                    continue;
                }

                chunk_slot& slot = slot_of(x, y);

                if(slot.chunk_x != x || slot.chunk_y != y)
                {
                    if(job_slot >= 0 && &slots[job_slot] == &slot)
                    {
                        job_slot = -1;
                    }

                    slot.chunk_x = int16_t(x);
                    slot.chunk_y = int16_t(y);
                    slot.ready = false;
                }
            }
        }
    }

    // The pending chunk closest to the centre.
    [[nodiscard]] int next_job()
    {
        int best = -1;
        int best_distance = 0;

        for(int index = 0; index < slots_count; ++index)
        {
            const chunk_slot& slot = slots[index];

            if(! slot.ready && slot.chunk_x >= 0)
            {
                int distance = bn::abs(slot.chunk_x - centre_x) + bn::abs(slot.chunk_y - centre_y);

                if(best < 0 || distance < best_distance)
                {
                    best = index;
                    best_distance = distance;
                }
            }
        }

        return best;
    }

    void reset_cache()
    {
        for(chunk_slot& slot : slots)
        {
            slot.chunk_x = -1;
            slot.chunk_y = -1;
            slot.ready = false;
        }

        decals.clear();
        obstacle_count = 0;
        job_slot = -1;
        centre_x = -1000;
        centre_y = -1000;
    }

    [[nodiscard]] int to_chunk(int pixel)
    {
        return bn::clamp(pixel >> chunk_px_shift, 0, chunks_per_side - 1);
    }
}

void init(uint32_t seed)
{
    world_seed = seed;
    is_test = false;
    reset_cache();
    macro::generate(seed);
    hordes::seed_counts(seed);
}

void init_test()
{
    for(int y = 0; y < test_map_height; ++y)
    {
        BN_ASSERT(test_map_rows[y][test_map_width] == 0, "Invalid test map row width: ", y);
    }

    world_seed = test_map_seed;
    is_test = true;
    reset_cache();
    macro::generate_empty();
}

bool test_world()
{
    return is_test;
}

int width()
{
    return is_test ? test_map_width : width_tiles;
}

int height()
{
    return is_test ? test_map_height : height_tiles;
}

uint32_t seed()
{
    return world_seed;
}

void start_tile(int& tile_x, int& tile_y)
{
    if(is_test || ! macro::settlement_count())
    {
        // Test map: on the east-west road, west of the crossing.
        tile_x = 24;
        tile_y = 21;
        return;
    }

    // The crossroads at the centre of a village cell.
    const macro::settlement& start = macro::settlement_at(macro::start_settlement());
    tile_x = (start.cell_x * macro::cell_tiles) + (macro::cell_tiles / 2);
    tile_y = (start.cell_y * macro::cell_tiles) + (macro::cell_tiles / 2);
}

void prepare(int focus_x, int focus_y)
{
    recentre(to_chunk(focus_x), to_chunk(focus_y));

    for(chunk_slot& slot : slots)
    {
        if(slot.chunk_x >= 0)
        {
            finish(slot);
        }
    }
}

void update(int focus_x, int focus_y, int view_left, int view_top)
{
    int chunk_x = to_chunk(focus_x);
    int chunk_y = to_chunk(focus_y);

    if(chunk_x != centre_x || chunk_y != centre_y)
    {
        recentre(chunk_x, chunk_y);
    }

    // What the renderer draws this frame (plus its spare column and row) must exist now.
    constexpr int view_width = 240 + 16;
    constexpr int view_height = 160 + 16;
    int first_x = (view_left - 8) >> chunk_px_shift;
    int last_x = (view_left + view_width) >> chunk_px_shift;
    int first_y = (view_top - 8) >> chunk_px_shift;
    int last_y = (view_top + view_height) >> chunk_px_shift;

    for(int y = first_y; y <= last_y; ++y)
    {
        for(int x = first_x; x <= last_x; ++x)
        {
            if(unsigned(x) < unsigned(chunks_per_side) && unsigned(y) < unsigned(chunks_per_side))
            {
                chunk_slot& slot = slot_of(x, y);

                if(slot.chunk_x == x && slot.chunk_y == y)
                {
                    #if KX_BENCH
                        if(! slot.ready)
                        {
                            BN_LOG("KXSYNC chunk ", x, " ", y, " centre ", centre_x, " ", centre_y);
                        }
                    #endif

                    finish(slot);
                }
            }
        }
    }

    // The rest a few rows per frame.
    if(job_slot < 0)
    {
        job_slot = next_job();
        job = gen_state();
    }

    if(job_slot >= 0)
    {
        run_rows(slots[job_slot], job, world_gen::rows_per_frame);

        if(slots[job_slot].ready)
        {
            job_slot = -1;
        }
    }
}

bool loaded(int tile_x, int tile_y)
{
    return ready_slot(tile_x >> chunk_shift, tile_y >> chunk_shift);
}

world_gfx gfx_at(layer value, int tile_x, int tile_y)
{
    const chunk_slot* slot = ready_slot(tile_x >> chunk_shift, tile_y >> chunk_shift);

    if(! slot)
    {
        return value == layer::GROUND ? world_gfx::DEEP_WATER : world_gfx::EMPTY;
    }

    return world_gfx(slot->gfx[int(value)][tile_index(tile_x, tile_y)]);
}

namespace
{
    // The building a tile belongs to (milestone 14: some rooms loot by their building), COUNT if none.
    [[nodiscard]] building building_kind(int tile_x, int tile_y)
    {
        lot_view lot;
        return ! is_test && lot_at(tile_x, tile_y, lot) ? lot.kind : building::COUNT;
    }
}

road_piece road_piece_near(int tile_x, int tile_y, int radius)
{
    if(is_test)
    {
        return road_piece::NONE;
    }

    road_piece_view pieces[2];
    int count = road_pieces(tile_x >> macro::cell_shift, tile_y >> macro::cell_shift, pieces);

    for(int index = 0; index < count; ++index)
    {
        int dx = bn::abs(pieces[index].tile_x - tile_x);
        int dy = bn::abs(pieces[index].tile_y - tile_y);

        if(dx <= radius && dy <= radius)
        {
            return pieces[index].kind;
        }
    }

    return road_piece::NONE;
}

loot_table container_table(int tile_x, int tile_y)
{
    world_gfx furniture = gfx_at(layer::WALLS, tile_x, tile_y);

    // Lockers, shop shelves and counters loot by the building they stand in (milestone 14).
    if(furniture == world_gfx::LOCKER || furniture == world_gfx::SHOP_SHELF || furniture == world_gfx::COUNTER)
    {
        switch(building_kind(tile_x, tile_y))
        {

        case building::FIRE_STATION:
            return furniture == world_gfx::LOCKER ? loot_table::FIRE_STATION : loot_table::COUNTER;

        case building::BARRACKS:
        case building::ARMORY:
        case building::MESS_HALL:
            return furniture == world_gfx::LOCKER ? loot_table::FOOTLOCKER : loot_table::KITCHEN;

        case building::SCHOOL:
        case building::SMALL_SCHOOL:
            return loot_table::SCHOOL;

        case building::PHARMACY:
            return furniture == world_gfx::SHOP_SHELF ? loot_table::MEDICINE : loot_table::COUNTER;

        case building::HARDWARE_STORE:
            return furniture == world_gfx::SHOP_SHELF ? loot_table::GARAGE : loot_table::COUNTER;

        case building::PUB:
        case building::DINER:
            return loot_table::PUB;

        case building::POST_OFFICE:
            return loot_table::OFFICE;

        default:
            break;
        }
    }

    switch(furniture)
    {

    case world_gfx::KITCHEN_CABINET:
        return loot_table::KITCHEN;

    case world_gfx::FRIDGE:
        return loot_table::FRIDGE;

    case world_gfx::WARDROBE:
        return loot_table::BEDROOM;

    case world_gfx::BATH_CABINET:
        return loot_table::BATHROOM;

    case world_gfx::TOOL_SHELF:
        return loot_table::GARAGE;

    case world_gfx::SHOP_SHELF:
        return loot_table::SHOP_SHELF;

    case world_gfx::COUNTER:
        return loot_table::COUNTER;

    case world_gfx::LOCKER:
        return loot_table::POLICE_LOCKER;

    case world_gfx::MED_CABINET:
        return loot_table::MEDICINE;

    case world_gfx::CRATE:
        // The checkpoint's crate (milestone 14) is a military one.
        return road_piece_near(tile_x, tile_y, 4) == road_piece::CHECKPOINT ? loot_table::MILITARY :
                                                                             loot_table::CRATE;

    case world_gfx::DESK:
        return loot_table::DESK;

    case world_gfx::TENT:
        return loot_table::CAMP;

    case world_gfx::BODY:
        return loot_table::BODY;

    case world_gfx::GUN_SAFE:
        return loot_table::GUN_SAFE;

    case world_gfx::STASH:
        return loot_table::STASH;

    case world_gfx::CHEST_FREEZER:
        return loot_table::FREEZER;

    // Milestone 14.
    case world_gfx::SCHOOL_DESK:
        return loot_table::SCHOOL;

    case world_gfx::FILING_CABINET:
        return loot_table::OFFICE;

    case world_gfx::RACK:
        return loot_table::WAREHOUSE;

    case world_gfx::GUN_RACK:
        return building_kind(tile_x, tile_y) == building::HUNTERS_CABIN ? loot_table::HUNTER : loot_table::ARMORY;

    case world_gfx::VENDING:
        return loot_table::VENDING;

    case world_gfx::CHECKOUT:
        return loot_table::COUNTER;

    // Milestone 14 stage 4: the coach (its door and a middle window), the hunter stand.
    case world_gfx::BUS_01:
    case world_gfx::BUS_20:
        return loot_table::BUS;

    case world_gfx::HUNTER_STAND:
        return loot_table::HUNTER_STAND;

    default:
        return loot_table::COUNT;
    }
}

bool building_over(int tile_x, int tile_y, int& left, int& top, int& width, int& height)
{
    if(is_test || ! is_roof(gfx_at(layer::CANOPY, tile_x, tile_y)))
    {
        return false;
    }

    return building_at(tile_x, tile_y, left, top, width, height);
}

bool is_bed(int tile_x, int tile_y)
{
    world_gfx value = gfx_at(layer::WALLS, tile_x, tile_y);
    return value == world_gfx::BED || value == world_gfx::BUNK_BED;
}

bool solid(int tile_x, int tile_y)
{
    return (tile_flags(tile_x, tile_y) & solid_flag) || obstacle_on_tile(tile_x, tile_y);
}

void set_obstacles(const obstacle* list, int count)
{
    obstacle_count = bn::min(count, max_obstacles);

    for(int index = 0; index < obstacle_count; ++index)
    {
        obstacles[index] = list[index];
    }
}

terrain terrain_at(int tile_x, int tile_y)
{
    if(! loaded(tile_x, tile_y))
    {
        return terrain::DEEP_WATER;
    }

    return terrain(tile_flags(tile_x, tile_y) & terrain_mask);
}

void save_decals(save::writer& out)
{
    out.put(decals.size());

    for(const decal_entry& value : decals)
    {
        out.put(value);
    }
}

void load_decals(save::reader& in)
{
    decals.clear();
    int count = bn::min(in.get<int>(), max_decals);

    for(int index = 0; index < count; ++index)
    {
        decal_entry value;
        in.get(value);
        decals.push_back(value);
    }
}

void set_ground_gfx(int tile_x, int tile_y, world_gfx gfx)
{
    if(unsigned(tile_x) >= unsigned(width_tiles) || unsigned(tile_y) >= unsigned(height_tiles))
    {
        return;
    }

    for(auto it = decals.begin(); it != decals.end(); ++it)
    {
        if(it->tile_x == tile_x && it->tile_y == tile_y && it->layer == uint8_t(layer::GROUND))
        {
            decals.erase(it);
            break;
        }
    }

    if(world_gfx_base[int(gfx)] != uint8_t(gfx))
    {
        // Full: the oldest ground decal makes way (wall changes are kept).
        if(decals.full())
        {
            for(auto it = decals.begin(); it != decals.end(); ++it)
            {
                if(it->layer == uint8_t(layer::GROUND))
                {
                    decals.erase(it);
                    break;
                }
            }
        }

        if(! decals.full())
        {
            decals.push_back(decal_entry{ int16_t(tile_x), int16_t(tile_y), uint8_t(gfx), uint8_t(layer::GROUND) });
        }
    }

    const chunk_slot* slot = ready_slot(tile_x >> chunk_shift, tile_y >> chunk_shift);

    if(slot)
    {
        slots[slot - slots].gfx[int(layer::GROUND)][tile_index(tile_x, tile_y)] = uint8_t(gfx);
    }
}

namespace
{
    // A remembered change on the walls or the canopy layer (kept, never forgotten as old decals are).
    bool set_kept_gfx(layer which, int tile_x, int tile_y, world_gfx gfx, bool restore)
    {
        if(unsigned(tile_x) >= unsigned(width_tiles) || unsigned(tile_y) >= unsigned(height_tiles))
        {
            return false;
        }

        decal_entry* existing = nullptr;

        for(decal_entry& value : decals)
        {
            if(value.tile_x == tile_x && value.tile_y == tile_y && value.layer == uint8_t(which))
            {
                existing = &value;
                break;
            }
        }

        if(restore)
        {
            if(existing)
            {
                decals.erase(decals.begin() + (existing - decals.data()));
            }
        }
        else if(existing)
        {
            existing->gfx = uint8_t(gfx);
        }
        else
        {
            if(decals.full())
            {
                for(auto it = decals.begin(); it != decals.end(); ++it)
                {
                    if(it->layer == uint8_t(layer::GROUND))
                    {
                        decals.erase(it);
                        break;
                    }
                }
            }

            if(decals.full())
            {
                return false;
            }

            decals.push_back(decal_entry{ int16_t(tile_x), int16_t(tile_y), uint8_t(gfx), uint8_t(which) });
        }

        const chunk_slot* slot = ready_slot(tile_x >> chunk_shift, tile_y >> chunk_shift);

        if(slot)
        {
            slots[slot - slots].gfx[int(which)][tile_index(tile_x, tile_y)] = uint8_t(gfx);
        }

        return true;
    }
}

bool set_wall_gfx(int tile_x, int tile_y, world_gfx gfx, bool restore)
{
    return set_kept_gfx(layer::WALLS, tile_x, tile_y, gfx, restore);
}

int tree_at(int tile_x, int tile_y)
{
    world_gfx wall = wall_gfx(tile_x, tile_y);

    for(int kind = 1; kind < int(tree_kind::COUNT); ++kind)
    {
        if(world_tree_trunks[kind] == uint8_t(wall))
        {
            return kind;
        }
    }

    return 0;
}

bool chop_tree(int tile_x, int tile_y)
{
    if(! tree_at(tile_x, tile_y) || ! set_wall_gfx(tile_x, tile_y, world_gfx::STUMP))
    {
        return false;
    }

    // A tile's canopy is the lower half of its own tree and the upper half of the one below it (as generated):
    // this tile keeps only the top of the tree below, the one above loses its lower half.
    auto canopy = [](int x, int y, int own, int below)
    {
        if(! is_roof(gfx_at(layer::CANOPY, x, y)))
        {
            static_cast<void>(set_kept_gfx(layer::CANOPY, x, y, world_gfx(world_tree_canopies[own][below]), false));
        }
    };

    canopy(tile_x, tile_y, 0, tree_at(tile_x, tile_y + 1));
    canopy(tile_x, tile_y - 1, tree_at(tile_x, tile_y - 1), 0);
    return true;
}

namespace
{
    // A house id: the macro cell (12 bits), then the lot packed into 4 bits (its quarter of the cell, 2 bits, and
    // its depth along x and y, 1 bit each: world_gen::lots_deep is 2).
    static_assert(world_gen::lots_deep <= 2 && macro::cells <= 64);

    [[nodiscard]] uint16_t make_house_id(const lot_view& lot)
    {
        int packed = (lot.key & 3) | (((lot.key >> 2) & 1) << 2) | (((lot.key >> 4) & 1) << 3);
        return uint16_t((((lot.cell_y * macro::cells) + lot.cell_x) << 4) | packed);
    }

    [[nodiscard]] bool house_lot(uint16_t id, lot_view& lot)
    {
        if(! id)
        {
            return false;
        }

        int packed = id & 15;
        int cell = id >> 4;
        int key = (packed & 3) | (((packed >> 2) & 1) << 2) | (((packed >> 3) & 1) << 4);
        return lot_planned(cell % macro::cells, cell / macro::cells, key, lot);
    }

    [[nodiscard]] bool lot_locked(const lot_view& lot)
    {
        if(! home(lot.kind))
        {
            return false;
        }

        return int(hash_xy(seed() ^ 0x4c4f434bu, (lot.cell_x * 64) + lot.key, lot.cell_y) % 100) <
               lock_tuning::locked_percent;
    }

    // A door of the template beside its open ground (an unroofed tile or the lot's edge): an outer door. Playtest 1:
    // outer doorways are under the roof now, so a door with no roof over it no longer marks one.
    [[nodiscard]] bool outer_door(const lot_view& lot, int x, int y)
    {
        bool side_wall;

        if(door_of(world_gfx(lot.walls[(y * lot.width) + x]), side_wall) == door_state::NONE)
        {
            return false;
        }

        constexpr int steps[4][2] = { { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 } };

        for(const auto& step : steps)
        {
            int nx = x + step[0];
            int ny = y + step[1];

            if(unsigned(nx) >= unsigned(lot.width) || unsigned(ny) >= unsigned(lot.height) ||
               ! lot.roofs[(ny * lot.width) + nx])
            {
                return true;
            }
        }

        return false;
    }

    [[nodiscard]] int settlement_of_cell(int cell_x, int cell_y)
    {
        for(int index = 0; index < macro::settlement_count(); ++index)
        {
            const macro::settlement& value = macro::settlement_at(index);

            if(cell_x >= value.cell_x && cell_x < value.cell_x + value.size && cell_y >= value.cell_y &&
               cell_y < value.cell_y + value.size)
            {
                return index;
            }
        }

        return -1;
    }
}

uint16_t house_id_at(int tile_x, int tile_y)
{
    lot_view lot;
    return lot_at(tile_x, tile_y, lot) ? make_house_id(lot) : 0;
}

int house_settlement(uint16_t id)
{
    lot_view lot;
    return house_lot(id, lot) ? settlement_of_cell(lot.cell_x, lot.cell_y) : -1;
}

bool house_locked(uint16_t id)
{
    lot_view lot;
    return house_lot(id, lot) && lot_locked(lot);
}

bool house_door(uint16_t id, int& tile_x, int& tile_y, bool* side_wall)
{
    lot_view lot;

    if(! house_lot(id, lot))
    {
        return false;
    }

    for(int y = 0; y < lot.height; ++y)
    {
        for(int x = 0; x < lot.width; ++x)
        {
            if(outer_door(lot, x, y))
            {
                tile_x = lot.left + x;
                tile_y = lot.top + y;

                if(side_wall)
                {
                    static_cast<void>(door_of(world_gfx(lot.walls[(y * lot.width) + x]), *side_wall));
                }

                return true;
            }
        }
    }

    return false;
}

int changes_percent()
{
    int walls = 0;

    for(const decal_entry& entry : decals)
    {
        walls += entry.layer != uint8_t(layer::GROUND);
    }

    return (walls * 100) / max_decals;
}

bool wall_changed(int tile_x, int tile_y)
{
    for(const decal_entry& value : decals)
    {
        if(value.tile_x == tile_x && value.tile_y == tile_y && value.layer == uint8_t(layer::WALLS))
        {
            return true;
        }
    }

    return false;
}

int remembered_gfx(layer which, int tile_x, int tile_y)
{
    for(const decal_entry& value : decals)
    {
        if(value.tile_x == tile_x && value.tile_y == tile_y && value.layer == uint8_t(which))
        {
            return value.gfx;
        }
    }

    return -1;
}

bool under_roof(int tile_x, int tile_y)
{
    int left;
    int top;
    int width;
    int height;
    return ! is_test && building_at(tile_x, tile_y, left, top, width, height);
}

bool door_locked(int tile_x, int tile_y)
{
    bool side_wall;

    if(door_of(wall_gfx(tile_x, tile_y), side_wall) != door_state::CLOSED || wall_changed(tile_x, tile_y))
    {
        return false;
    }

    lot_view lot;

    if(! lot_at(tile_x, tile_y, lot) || ! lot_locked(lot))
    {
        return false;
    }

    int x = tile_x - lot.left;
    int y = tile_y - lot.top;
    return unsigned(x) < unsigned(lot.width) && unsigned(y) < unsigned(lot.height) && outer_door(lot, x, y);
}

namespace
{
    // What a template's furniture piece became on a tile (as world generation swaps it: kx_world_gen_tuning.h).
    [[nodiscard]] world_gfx swapped(world_gfx piece, int tile_x, int tile_y)
    {
        for(const world_gen::furniture_swap& swap : world_gen::furniture_swaps)
        {
            if(swap.from == piece)
            {
                int roll = int(hash_xy(seed() ^ 0x46555251u, tile_x, tile_y) % 100);

                if(roll < swap.percent[0])
                {
                    return swap.options[0];
                }

                return roll < swap.percent[0] + swap.percent[1] ? swap.options[1] : piece;
            }
        }

        return piece;
    }
}

bool house_safe(uint16_t id, int& tile_x, int& tile_y)
{
    lot_view lot;

    if(! house_lot(id, lot))
    {
        return false;
    }

    for(int y = 0; y < lot.height; ++y)
    {
        for(int x = 0; x < lot.width; ++x)
        {
            if(swapped(world_gfx(lot.walls[(y * lot.width) + x]), lot.left + x, lot.top + y) == world_gfx::GUN_SAFE)
            {
                tile_x = lot.left + x;
                tile_y = lot.top + y;
                return true;
            }
        }
    }

    return false;
}

int safe_code(uint16_t id)
{
    return int(hash_xy(seed() ^ 0x53414645u, id, 7) % 1000);
}

bool house_is_armory(uint16_t id)
{
    lot_view lot;
    return house_lot(id, lot) && lot.kind == building::ARMORY;
}

bool house_is_police(uint16_t id)
{
    lot_view lot;
    return house_lot(id, lot) && lot.kind == building::POLICE_STATION;
}

int settlement_at_tile(int tile_x, int tile_y)
{
    return settlement_of_cell(tile_x >> macro::cell_shift, tile_y >> macro::cell_shift);
}

uint16_t safe_for(int tile_x, int tile_y, uint32_t roll)
{
    constexpr int capacity = 64;
    uint16_t ids[capacity];
    building kinds[capacity];
    int count = settlement_buildings(settlement_at_tile(tile_x, tile_y), ids, kinds, capacity);
    int safes = 0;

    for(int index = 0; index < count; ++index)
    {
        int x;
        int y;

        if(house_safe(ids[index], x, y))
        {
            ids[safes++] = ids[index];
        }
    }

    return safes ? ids[roll % unsigned(safes)] : 0;
}

bool stash_tile(int settlement, int& tile_x, int& tile_y, int& direction)
{
    if(settlement < 0 || settlement >= macro::settlement_count())
    {
        return false;
    }

    // Off a corner of the settlement, past its clearing, on dry land; the seed picks the corner to try first.
    const macro::settlement& value = macro::settlement_at(settlement);
    int centre_x = (value.cell_x * macro::cell_tiles) + ((value.size * macro::cell_tiles) / 2);
    int centre_y = (value.cell_y * macro::cell_tiles) + ((value.size * macro::cell_tiles) / 2);
    int clearing = value.kind == macro::zone::TOWN ? world_gen::town_clearing : world_gen::village_clearing;
    int reach = ((clearing + 8) * 7) / 10;
    int first = int(hash_xy(seed() ^ 0x53544153u, settlement, 3) & 3);
    constexpr int corner_x[] = { 1, 1, -1, -1 };
    constexpr int corner_y[] = { -1, 1, 1, -1 };

    for(int attempt = 0; attempt < 4; ++attempt)
    {
        int corner = (first + attempt) & 3;

        for(int step = 0; step < 6; ++step)
        {
            int x = centre_x + (corner_x[corner] * (reach + (step * 2)));
            int y = centre_y + (corner_y[corner] * (reach + (step * 2)));

            if(world_gen::elevation(seed(), x, y) > world_gen::lot_dry_elevation + 4)
            {
                tile_x = x;
                tile_y = y;
                direction = corner;
                return true;
            }
        }
    }

    return false;
}

bool farm_yard(int cell_x, int cell_y, int& tile_x, int& tile_y)
{
    if(is_test || cell_x < 0 || cell_y < 0 || cell_x >= macro::cells || cell_y >= macro::cells ||
       macro::zone_at(cell_x, cell_y) != macro::zone::FARM)
    {
        return false;
    }

    // A farm builds only on the four lots by its crossroads.
    int best = -1;
    int best_rank = 0;
    lot_view best_lot;

    for(int key = 0; key < 4; ++key)
    {
        lot_view lot;

        if(! lot_planned(cell_x, cell_y, key, lot))
        {
            continue;
        }

        int rank = lot.kind == building::BARN || lot.kind == building::STABLE ? 3 :
                   lot.kind == building::FARM_SHED ? 2 : 1;

        if(rank > best_rank)
        {
            best = key;
            best_rank = rank;
            best_lot = lot;
        }
    }

    if(best < 0)
    {
        return false;
    }

    // A barn or a shed opens on a gap, not a door: then the yard is just in front of it.
    if(! house_door(make_house_id(best_lot), tile_x, tile_y))
    {
        tile_x = best_lot.left + (best_lot.width / 2);
        tile_y = best_lot.top + best_lot.height;
    }

    return true;
}

int settlement_buildings(int settlement, uint16_t* ids, building* kinds, int capacity)
{
    if(settlement < 0 || settlement >= macro::settlement_count())
    {
        return 0;
    }

    const macro::settlement& value = macro::settlement_at(settlement);
    int count = 0;

    for(int cell_y = value.cell_y; cell_y < value.cell_y + value.size; ++cell_y)
    {
        for(int cell_x = value.cell_x; cell_x < value.cell_x + value.size; ++cell_x)
        {
            for(int packed = 0; packed < 16 && count < capacity; ++packed)
            {
                int key = (packed & 3) | (((packed >> 2) & 1) << 2) | (((packed >> 3) & 1) << 4);
                lot_view lot;

                if(lot_planned(cell_x, cell_y, key, lot))
                {
                    ids[count] = make_house_id(lot);
                    kinds[count] = lot.kind;
                    ++count;
                }
            }
        }
    }

    return count;
}

uint16_t key_for(int tile_x, int tile_y, uint32_t roll)
{
    int settlement = settlement_of_cell(tile_x >> macro::cell_shift, tile_y >> macro::cell_shift);

    if(settlement < 0)
    {
        return 0;
    }

    // Every locked house in the settlement's cells but the one the key was found in.
    const macro::settlement& value = macro::settlement_at(settlement);
    uint16_t here = house_id_at(tile_x, tile_y);
    constexpr int max_candidates = 64;
    uint16_t candidates[max_candidates];
    int count = 0;

    for(int cell_y = value.cell_y; cell_y < value.cell_y + value.size; ++cell_y)
    {
        for(int cell_x = value.cell_x; cell_x < value.cell_x + value.size; ++cell_x)
        {
            for(int packed = 0; packed < 16 && count < max_candidates; ++packed)
            {
                int key = (packed & 3) | (((packed >> 2) & 1) << 2) | (((packed >> 3) & 1) << 4);
                lot_view lot;

                if(lot_planned(cell_x, cell_y, key, lot) && lot_locked(lot) && make_house_id(lot) != here)
                {
                    candidates[count++] = make_house_id(lot);
                }
            }
        }
    }

    return count ? candidates[roll % unsigned(count)] : 0;
}

bool breakable(int tile_x, int tile_y)
{
    return tile_flags(tile_x, tile_y) & breakable_flag;
}

#if KX_BENCH
    void dump(int view, bool with_macro)
    {
        constexpr char digits[] = "0123456789ABCDEF";
        BN_LOG("KXMACRO seed ", world_seed);

        // 16 cells (4 hex digits each) per line.
        for(int cell_y = 0; cell_y < macro::cells && with_macro; ++cell_y)
        {
            for(int part = 0; part < macro::cells / 16; ++part)
            {
                char text[16 * 4 + 1];

                for(int index = 0; index < 16; ++index)
                {
                    uint16_t value = macro::detail::cell((part * 16) + index, cell_y);

                    for(int nibble = 0; nibble < 4; ++nibble)
                    {
                        text[(index * 4) + nibble] = digits[(value >> ((3 - nibble) * 4)) & 15];
                    }
                }

                text[16 * 4] = 0;
                BN_LOG("KXMACRO ", cell_y, " ", part, " ", bn::string_view(text));
            }
        }

        BN_LOG("KXCHUNK view ", view);

        for(int index = 0; index < slots_count; ++index)
        {
            const chunk_slot& slot = slots[index];

            if(! slot.ready)
            {
                continue;
            }

            BN_LOG("KXCHUNK slot ", index, " ", slot.chunk_x, " ", slot.chunk_y);

            // The three layers, then the building styles as a fourth.
            for(int value_layer = 0; value_layer <= int(layer::COUNT); ++value_layer)
            {
                for(int row = 0; row < chunk_tiles; ++row)
                {
                    char text[chunk_tiles * 2 + 1];
                    const uint8_t* gfx = (value_layer < int(layer::COUNT) ? slot.gfx[value_layer] : slot.style) +
                                         (row * chunk_tiles);

                    for(int x = 0; x < chunk_tiles; ++x)
                    {
                        text[x * 2] = digits[gfx[x] >> 4];
                        text[(x * 2) + 1] = digits[gfx[x] & 15];
                    }

                    text[chunk_tiles * 2] = 0;
                    BN_LOG("KXCHUNK ", index, " ", value_layer, " ", row, " ", bn::string_view(text));
                }
            }
        }

        BN_LOG("KXCHUNK end");
    }
#endif

}
