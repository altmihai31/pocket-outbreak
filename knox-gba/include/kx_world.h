#ifndef KX_WORLD_H
#define KX_WORLD_H

#include "bn_common.h"

#include "kx_bench.h"
#include "kx_loot.h"
#include "kx_terrain.h"
#include "kx_buildings.h"
#include "kx_world_gfx_ids.h"

namespace kx::save
{
    class reader;
    class writer;
}

namespace kx::world
{

constexpr int tile_size = 16;
constexpr int tile_shift = 4;

// Spec: World. 4096x4096 tiles in chunks of 32x32 tiles (512x512 px), generated on demand from the seed.
constexpr int width_tiles = 4096;
constexpr int height_tiles = 4096;
constexpr int chunk_tiles = 32;
constexpr int chunk_shift = 5;
constexpr int chunk_px_shift = chunk_shift + tile_shift;

enum class layer : int
{
    GROUND,     // BG3
    WALLS,      // BG2
    CANOPY,     // BG1
    COUNT
};

// A generated world: the macro map is built now, chunks when the player comes near.
void init(uint32_t seed);

// The hand-made test map at the world's top-left corner, deep water around it (the benches' world).
void init_test();

[[nodiscard]] bool test_world();

// Playable size in tiles: the whole world, or the test map in the test world (the camera stops at its edges).
[[nodiscard]] int width();
[[nodiscard]] int height();

[[nodiscard]] inline int width_px()
{
    return width() * tile_size;
}

[[nodiscard]] inline int height_px()
{
    return height() * tile_size;
}

[[nodiscard]] uint32_t seed();

// Where a new survivor starts: the crossroads of a village (test world: the road west of the crossing).
void start_tile(int& tile_x, int& tile_y);

// Generates every chunk around a world pixel now (game start, teleports).
void prepare(int focus_x, int focus_y);

// Once per frame, before the renderer: keeps the 3x3 chunks around the focus (the player) cached, generating new
// ones a few rows per frame; chunks the view (top-left world pixel) needs are finished at once.
void update(int focus_x, int focus_y, int view_left, int view_top);

// True if the tile's chunk is generated.
[[nodiscard]] bool loaded(int tile_x, int tile_y);

// The roofed part of the building over a tile, as a rectangle of tiles; false outside buildings (and on the test map,
// which has no roofs).
[[nodiscard]] bool building_over(int tile_x, int tile_y, int& left, int& top, int& width, int& height);

// Beds: A sleeps (spec: Bed — sleep, skip night).
[[nodiscard]] bool is_bed(int tile_x, int tile_y);

// Loot table of the furniture container on a tile, or loot_table::COUNT.
[[nodiscard]] loot_table container_table(int tile_x, int tile_y);

// Set pieces on the roads of generated worlds (milestone 14; the village church is a building).
enum class road_piece : uint8_t
{
    NONE,
    BUS,            // a crashed coach in each town
    BARRICADE,      // a police barricade on a road out of each town
    CHECKPOINT,     // the military base's checkpoint
};

// The set piece in the tile's macro cell whose middle is within radius tiles (Chebyshev), NONE if none or on the
// test map.
[[nodiscard]] road_piece road_piece_near(int tile_x, int tile_y, int radius);

// Tiles outside the world or not generated yet are solid deep water.
[[nodiscard]] bool solid(int tile_x, int tile_y);
[[nodiscard]] terrain terrain_at(int tile_x, int tile_y);

[[nodiscard]] world_gfx gfx_at(layer value, int tile_x, int tile_y);

[[nodiscard]] inline world_gfx ground_gfx(int tile_x, int tile_y)
{
    return gfx_at(layer::GROUND, tile_x, tile_y);
}

// Corpse decals are mirrored on some tiles: a hash of the tile, so nothing is stored. A dying zombie's sprite takes
// the same side, so the body and the corpse it becomes agree (playtest 1).
[[nodiscard]] inline bool decal_mirrored(int tile_x, int tile_y)
{
    unsigned value = (unsigned(tile_x) * 0x9e3779b1u) ^ (unsigned(tile_y ^ 0x5eed) * 0x85ebca77u);
    return (value ^ (value >> 15)) & (1u << 20);
}

// Replaces a ground graphic (decals). Decals are remembered, so they come back when their chunk is generated
// again; setting a tile back to its plain graphic forgets it. The caller refreshes the renderer.
void set_ground_gfx(int tile_x, int tile_y, world_gfx gfx);

// Replaces a wall-layer graphic for good (base pieces, smashed windows, moved beds): remembered like a decal but
// never forgotten. restore = true puts the generated graphic back (given as gfx) and forgets the change. Returns
// false if there is no room to remember it. The caller refreshes the renderer.
bool set_wall_gfx(int tile_x, int tile_y, world_gfx gfx, bool restore = false);

// Playtest 5: a side door's edge, drawn on the canopy of the tile outside it, follows the door's state. Called for
// each chunk as it is generated and whenever a wall graphic changes; true if an edge changed (the caller refreshes
// the tiles either side of the door).
bool update_door_edge(int tile_x, int tile_y);

[[nodiscard]] inline world_gfx wall_gfx(int tile_x, int tile_y)
{
    return gfx_at(layer::WALLS, tile_x, tile_y);
}

// Doors (milestone 12.7): a door's graphic is its state, in a front wall (FACE) or a side wall (TOP).
enum class door_state : uint8_t
{
    NONE,
    CLOSED,
    OPEN,
    BROKEN
};

[[nodiscard]] constexpr door_state door_of(world_gfx gfx, bool& side_wall)
{
    side_wall = gfx == world_gfx::DOOR_TOP_CLOSED || gfx == world_gfx::DOOR_TOP_OPEN ||
                gfx == world_gfx::DOOR_TOP_BROKEN;

    switch(gfx)
    {

    case world_gfx::DOOR_FACE_CLOSED:
    case world_gfx::DOOR_TOP_CLOSED:
        return door_state::CLOSED;

    case world_gfx::DOOR_FACE_OPEN:
    case world_gfx::DOOR_TOP_OPEN:
        return door_state::OPEN;

    case world_gfx::DOOR_FACE_BROKEN:
    case world_gfx::DOOR_TOP_BROKEN:
        return door_state::BROKEN;

    default:
        return door_state::NONE;
    }
}

[[nodiscard]] constexpr world_gfx door_gfx(door_state state, bool side_wall)
{
    return state == door_state::CLOSED ? (side_wall ? world_gfx::DOOR_TOP_CLOSED : world_gfx::DOOR_FACE_CLOSED) :
           state == door_state::OPEN ? (side_wall ? world_gfx::DOOR_TOP_OPEN : world_gfx::DOOR_FACE_OPEN) :
           state == door_state::BROKEN ? (side_wall ? world_gfx::DOOR_TOP_BROKEN : world_gfx::DOOR_FACE_BROKEN) :
           world_gfx::EMPTY;
}

// Houses and locks (milestone 12.7). A house id names a lot's building (its macro cell and lot): a house key keeps
// the id of the house it opens in its extra field; 0 is none.
namespace lock_tuning
{
    constexpr int locked_percent = 33;          // small and large houses with their outer doors locked
    constexpr int force_frames = 120;           // a crowbar: 2 s, loud, the door breaks
    constexpr int force_noise = 12;
    constexpr int force_wear = 3;
    constexpr int pick_frames = 300;            // a screwdriver: 5 s
    constexpr int burglar_pick_frames = 120;    // 2 s, never fails (spec: picks locked doors quietly)
    constexpr int pick_fail_percent = 40;
    constexpr int pick_fail_per_sneaking = 8;
    constexpr int pick_fail_noise = 4;
    constexpr int sound_interval = 30;
}

// The house whose lot holds a tile; 0 with no building there.
[[nodiscard]] uint16_t house_id_at(int tile_x, int tile_y);

// The settlement (macro::settlement_at index) a house stands in, or -1.
[[nodiscard]] int house_settlement(uint16_t id);

// A small or large house the seed locked.
[[nodiscard]] bool house_locked(uint16_t id);

// A house's first outer door (a door whose tile is not under its roof), in world tiles; side_wall (if given) says
// whether it is in a side wall, from the template (so it works before the chunk is generated).
[[nodiscard]] bool house_door(uint16_t id, int& tile_x, int& tile_y, bool* side_wall = nullptr);

// A closed outer door of a locked house that nobody has opened (a door once opened is remembered, and stays
// unlocked).
[[nodiscard]] bool door_locked(int tile_x, int tile_y);

// True if the wall layer of a tile was changed (a door opened, a window smashed, a piece built).
[[nodiscard]] bool wall_changed(int tile_x, int tile_y);

// How full the list of remembered wall and canopy changes is, in percent (ground decals make way; the rest never do).
[[nodiscard]] int changes_percent();

// The remembered change on a layer of a tile (what the save keeps), or -1 if there is none; works for chunks that
// are not generated.
[[nodiscard]] int remembered_gfx(layer which, int tile_x, int tile_y);

// True if a building's roof covers a tile (from the seed, so before its chunk is generated); never on the test map.
[[nodiscard]] bool under_roof(int tile_x, int tile_y);

// Gun safes (milestone 12.7 stage 4): every police station has one, and a fifth of houses (a wardrobe swapped
// for it). Its tile; its 3-digit code (from the seed, never stored); the police station or a house.
[[nodiscard]] bool house_safe(uint16_t id, int& tile_x, int& tile_y);
[[nodiscard]] int safe_code(uint16_t id);
[[nodiscard]] bool house_is_police(uint16_t id);

// A building holding a gun safe is the military base's armory (milestone 14: its safes get their own notes).
[[nodiscard]] bool house_is_armory(uint16_t id);

// A building with a gun safe in the settlement holding a tile, for a note found there; 0 if none.
[[nodiscard]] uint16_t safe_for(int tile_x, int tile_y, uint32_t roll);

// The settlement holding a tile, or -1.
[[nodiscard]] int settlement_at_tile(int tile_x, int tile_y);

// A settlement's stash (a note tells of it): a dry tile in the woods off one of its corners, and which way it is
// from the settlement (0 north-east, 1 south-east, 2 south-west, 3 north-west). False if none was found.
[[nodiscard]] bool stash_tile(int settlement, int& tile_x, int& tile_y, int& direction);

// A farm cell's yard (animals plan stage 4): the outer door of its barn, stable or farm shed (else its farmhouse),
// or just in front of it when it opens on a gap; in world tiles, from the seed. False if the cell is no farm or has
// no building (and always on the test map).
[[nodiscard]] bool farm_yard(int cell_x, int cell_y, int& tile_x, int& tile_y);

// The buildings of a settlement (house ids and their kinds), up to capacity; how many.
int settlement_buildings(int settlement, uint16_t* ids, building* kinds, int capacity);

// A locked house in the settlement holding a tile, other than one standing on it, for a key found there; roll
// picks which. 0 if the settlement has none (or the tile is in none).
[[nodiscard]] uint16_t key_for(int tile_x, int tile_y, uint32_t roll);

// Chopping trees (milestone 12.7 stage 5): the tree on a tile (0 none, else its tree_kind); chopping it leaves a
// stump and takes its canopy off this tile and the one above, remembered like a wall change. False if no tree.
[[nodiscard]] int tree_at(int tile_x, int tile_y);
bool chop_tree(int tile_x, int tile_y);

namespace chop_tuning
{
    constexpr int frames = 240;                 // hold A with the axe: 4 s
    constexpr int noise = 8;                    // the tree falling; each blow is half that
    constexpr int axe_wear = 2;
    constexpr int logs = 2;
    constexpr int axe_xp = 3;
}

// Solid, but zombies break through (windows and barricades).
[[nodiscard]] bool breakable(int tile_x, int tile_y);

// Decals remembered at most (the oldest is forgotten), and their size in the save.
constexpr int max_decals = 512;
constexpr int max_decal_save_bytes = 4 + (max_decals * 6);

// Save block: the remembered decals (replayed when their chunks are generated).
void save_decals(save::writer& out);
void load_decals(save::reader& in);

#if KX_BENCH
    // Logs the macro map (with_macro) and the chunk cache as view `view` (KXMACRO / KXCHUNK lines for
    // tools/render_world_dump.py).
    void dump(int view, bool with_macro);
#endif

// Solid things that are not tiles (parked cars): boxes in world pixels, inclusive. box_blocked() and solid() count
// them, and the zombies' flow field goes around them. The list replaces the last one.
struct obstacle
{
    uint16_t left;
    uint16_t top;
    uint16_t right;
    uint16_t bottom;
};

constexpr int max_obstacles = 8;

void set_obstacles(const obstacle* list, int count);

// True if the feet box [left, right] x [top, bottom] (inclusive pixels) touches a solid tile or an obstacle.
// swimmer: deep water doesn't block (milestone 13: the survivor swims; zombies and vehicles don't).
[[nodiscard]] BN_CODE_IWRAM bool box_blocked(int left, int top, int right, int bottom, bool swimmer = false);

// True if no sight-blocking tile (walls, tree trunks) lies between the two world pixels.
[[nodiscard]] BN_CODE_IWRAM bool line_of_sight(int from_x, int from_y, int to_x, int to_y);

}

#endif
