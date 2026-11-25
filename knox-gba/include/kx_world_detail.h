#ifndef KX_WORLD_DETAIL_H
#define KX_WORLD_DETAIL_H

#include "kx_buildings.h"
#include "kx_world.h"

// World internals shared by kx_world.cpp, the chunk generator and the IWRAM halves (collision, streaming). Not for
// use outside the world module.
namespace kx::world::detail
{

constexpr int chunk_area = chunk_tiles * chunk_tiles;
constexpr int chunks_per_side = width_tiles / chunk_tiles;
constexpr int window = 3;                       // spec: a 3x3 chunk cache around the player
constexpr int slots_count = window * window;

constexpr uint8_t solid_flag = 0x80;
constexpr uint8_t opaque_flag = 0x40;
constexpr uint8_t breakable_flag = 0x20;        // windows and barricades: solid, but zombies break through
constexpr uint8_t terrain_mask = 0x0F;

// A cached chunk: the graphic of every tile on each layer (3 KB). Terrain and collision come from the graphics
// (gfx_flags), so they need no storage of their own.
struct chunk_slot
{
    int16_t chunk_x = -1;
    int16_t chunk_y = -1;
    bool ready = false;
    uint8_t gfx[int(layer::COUNT)][chunk_area];

    // Building colour variants per tile (0 outside buildings): bits 0-1 roof, 2-3 wall, 4-5 floor (palette banks in
    // world_paint_banks), applied to the graphics world_gfx_paints marks.
    uint8_t style[chunk_area];
};

constexpr int style_bits = 2;
constexpr int style_mask = 3;

// Chunk (x, y) lives in slot (x mod 3, y mod 3), so moving one chunk only replaces the row or column left behind.
extern chunk_slot slots[slots_count];

// Low bits: terrain; 0x80: solid; 0x40: blocks sight. A tile's flags are its ground's and its wall's, or'ed.
struct gfx_flag_table
{
    uint8_t values[int(world_gfx::COUNT)];
};

[[nodiscard]] constexpr uint8_t plain_gfx_flags(world_gfx gfx)
{
    switch(gfx)
    {

    case world_gfx::GRASS_A:
    case world_gfx::GRASS_B:
    case world_gfx::GRASS_LUSH:
    case world_gfx::GRASS_FLOWERS:
    case world_gfx::GRASS_DRY:
        return uint8_t(terrain::GRASS);

    case world_gfx::ROAD:
    case world_gfx::ROAD_LINE_H:
    case world_gfx::ROAD_LINE_V:
        return uint8_t(terrain::ROAD);

    case world_gfx::PAVEMENT:
        return uint8_t(terrain::PAVEMENT);

    case world_gfx::FOREST_FLOOR:
        return uint8_t(terrain::FOREST);

    case world_gfx::MUD:
        return uint8_t(terrain::MUD);

    case world_gfx::SHALLOW_WATER:
        return uint8_t(terrain::SHALLOW_WATER);

    case world_gfx::DEEP_WATER:
        return uint8_t(terrain::DEEP_WATER) | solid_flag;

    case world_gfx::WOOD_FLOOR:
    case world_gfx::TILE_FLOOR:
    case world_gfx::CONCRETE:
    case world_gfx::BRIDGE_H:
    case world_gfx::BRIDGE_V:
        return uint8_t(terrain::FLOOR);

    case world_gfx::SAND:
        return uint8_t(terrain::SAND);

    case world_gfx::SOIL:
        return uint8_t(terrain::SOIL);

    case world_gfx::CROPS:
        return uint8_t(terrain::CROPS);

    case world_gfx::BARRICADE_FACE:
    case world_gfx::BARRICADE_TOP:
        return solid_flag | opaque_flag | breakable_flag;

    case world_gfx::BROKEN_FACE:
    case world_gfx::BROKEN_TOP:
        return uint8_t(terrain::FLOOR);

    // Log walls (milestone 12.7): a wall zombies break through.
    case world_gfx::LOG_WALL_H:
    case world_gfx::LOG_WALL_V:
        return solid_flag | opaque_flag | breakable_flag;

    // Doors (milestone 12.7): a closed one is a wall zombies break through; open or broken, a floor.
    case world_gfx::DOOR_FACE_CLOSED:
    case world_gfx::DOOR_TOP_CLOSED:
        return solid_flag | opaque_flag | breakable_flag;

    case world_gfx::DOOR_FACE_OPEN:
    case world_gfx::DOOR_TOP_OPEN:
    case world_gfx::DOOR_FACE_BROKEN:
    case world_gfx::DOOR_TOP_BROKEN:
        return uint8_t(terrain::FLOOR);

    case world_gfx::WALL_FACE:
    case world_gfx::WALL_TOP:
    case world_gfx::TREE_TRUNK:
    case world_gfx::PINE_TRUNK:
    case world_gfx::BIRCH_TRUNK:
        return solid_flag | opaque_flag;

    case world_gfx::WINDOW_FACE:
    case world_gfx::WINDOW_TOP:
        return solid_flag | breakable_flag;

    case world_gfx::FENCE_H:
    case world_gfx::FENCE_V:
    case world_gfx::ROCK:
    case world_gfx::BUSH:
    case world_gfx::RAIN_COLLECTOR:
    case world_gfx::CAMPFIRE:
    case world_gfx::GENERATOR:
    case world_gfx::TRAP:
    case world_gfx::SHOP_SHELF:
    case world_gfx::COUNTER:
    case world_gfx::LOCKER:
    case world_gfx::MED_CABINET:
    case world_gfx::CRATE:
    case world_gfx::DESK:
    case world_gfx::PUMP:
    case world_gfx::TENT:
    case world_gfx::GUN_SAFE:
    case world_gfx::STASH:
    case world_gfx::BODY:
    case world_gfx::KITCHEN_CABINET:
    case world_gfx::FRIDGE:
    case world_gfx::WARDROBE:
    case world_gfx::BATH_CABINET:
    case world_gfx::TOOL_SHELF:
    case world_gfx::BED:
    case world_gfx::WELL:
    case world_gfx::CHEST_FREEZER:
    case world_gfx::TV:
    // Milestone 14: furniture, the set pieces and the hunter stand.
    case world_gfx::PEW:
    case world_gfx::ALTAR:
    case world_gfx::SCHOOL_DESK:
    case world_gfx::BLACKBOARD:
    case world_gfx::BUNK_BED:
    case world_gfx::RACK:
    case world_gfx::GUN_RACK:
    case world_gfx::TABLE:
    case world_gfx::FILING_CABINET:
    case world_gfx::HAY_BALE:
    case world_gfx::STOVE:
    case world_gfx::BATH:
    case world_gfx::CHECKOUT:
    case world_gfx::VENDING:
    case world_gfx::TOILET:
    case world_gfx::BUS_00:
    case world_gfx::BUS_10:
    case world_gfx::BUS_20:
    case world_gfx::BUS_30:
    case world_gfx::BUS_01:
    case world_gfx::BUS_11:
    case world_gfx::BUS_21:
    case world_gfx::BUS_31:
    case world_gfx::ROADBLOCK:
    case world_gfx::SANDBAGS:
    case world_gfx::BARRIER:
    case world_gfx::WATCHTOWER:
    case world_gfx::GRAVE:
    case world_gfx::HUNTER_STAND:
        return solid_flag;

    default:
        return 0;
    }
}

[[nodiscard]] constexpr gfx_flag_table make_gfx_flags()
{
    gfx_flag_table result = {};

    for(int index = 0; index < int(world_gfx::COUNT); ++index)
    {
        // Decals keep their ground's flags.
        result.values[index] = plain_gfx_flags(world_gfx(world_gfx_base[index]));
    }

    return result;
}

inline constexpr gfx_flag_table gfx_flags = make_gfx_flags();

// The slot holding a chunk, if it is generated.
[[nodiscard]] inline const chunk_slot* ready_slot(int chunk_x, int chunk_y)
{
    if(unsigned(chunk_x) >= unsigned(chunks_per_side) || unsigned(chunk_y) >= unsigned(chunks_per_side))
    {
        return nullptr;
    }

    const chunk_slot& slot = slots[(chunk_x % window) + ((chunk_y % window) * window)];
    return slot.ready && slot.chunk_x == chunk_x && slot.chunk_y == chunk_y ? &slot : nullptr;
}

[[nodiscard]] constexpr int tile_index(int tile_x, int tile_y)
{
    return ((tile_y & (chunk_tiles - 1)) * chunk_tiles) + (tile_x & (chunk_tiles - 1));
}

// Tiles outside the world or in chunks not generated yet are solid and block sight.
[[nodiscard]] inline int tile_flags(int tile_x, int tile_y)
{
    const chunk_slot* slot = ready_slot(tile_x >> chunk_shift, tile_y >> chunk_shift);

    if(! slot)
    {
        return solid_flag | opaque_flag;
    }

    int index = tile_index(tile_x, tile_y);
    return gfx_flags.values[slot->gfx[int(layer::GROUND)][index]] |
           gfx_flags.values[slot->gfx[int(layer::WALLS)][index]];
}

// Deep water with nothing solid on it, in a generated chunk: a swimmer can go there (milestone 13).
[[nodiscard]] inline bool swimmable(int tile_x, int tile_y)
{
    const chunk_slot* slot = ready_slot(tile_x >> chunk_shift, tile_y >> chunk_shift);

    if(! slot)
    {
        return false;
    }

    int index = tile_index(tile_x, tile_y);
    return slot->gfx[int(layer::GROUND)][index] == uint8_t(world_gfx::DEEP_WATER) &&
           ! (gfx_flags.values[slot->gfx[int(layer::WALLS)][index]] & solid_flag);
}

// Obstacles (kx_world.h): few, and usually none, so the loops below cost next to nothing.
extern obstacle obstacles[max_obstacles];
extern int obstacle_count;

// True if an obstacle covers the centre of a tile.
[[nodiscard]] inline bool obstacle_on_tile(int tile_x, int tile_y)
{
    int x = (tile_x << tile_shift) + (tile_size / 2);
    int y = (tile_y << tile_shift) + (tile_size / 2);

    for(int index = 0; index < obstacle_count; ++index)
    {
        const obstacle& value = obstacles[index];

        if(x >= value.left && x <= value.right && y >= value.top && y <= value.bottom)
        {
            return true;
        }
    }

    return false;
}

// Chunk generation, a row at a time. Each row needs the trees of the row below it (for the canopies), so the
// state keeps the classified current and next rows.
struct gen_state
{
    int row = 0;
    bool primed = false;            // the first row is classified on a step of its own
    uint8_t ground[2][chunk_tiles];
    uint8_t walls[2][chunk_tiles];  // what stands on the tile (walls layer graphic): a wall, furniture, a tree...
    uint8_t roofs[2][chunk_tiles];  // a building's roof over it (canopy layer graphic), or EMPTY
    uint8_t styles[2][chunk_tiles]; // its building's colour variants (chunk_slot::style)
};

// Generated worlds (kx_world_gen.bn_iwram.cpp).
BN_CODE_IWRAM void generate_rows(chunk_slot& slot, gen_state& state, int rows);

// A lot's building (milestone 12.7: houses and their locks): which lot of which macro cell, the template (its
// rectangle in world tiles, its wall and roof graphics row-major). False with no building there.
struct lot_view
{
    int cell_x = 0;
    int cell_y = 0;
    int key = 0;                    // the lot within the cell (lot_key)
    building kind = building::COUNT;
    int left = 0;
    int top = 0;
    int width = 0;
    int height = 0;
    const uint8_t* walls = nullptr;
    const uint8_t* roofs = nullptr;
};

[[nodiscard]] bool lot_at(int tile_x, int tile_y, lot_view& out);

// The set pieces on a macro cell's roads (milestone 14), with the tile at their middle; returns how many.
struct road_piece_view
{
    road_piece kind = road_piece::NONE;
    int tile_x = 0;
    int tile_y = 0;
};

[[nodiscard]] int road_pieces(int cell_x, int cell_y, road_piece_view (&out)[2]);
[[nodiscard]] bool lot_planned(int cell_x, int cell_y, int key, lot_view& out);

// Homes (locks, keys and notes go with them): milestone 14 added cottages, bungalows, terraces, flats, trailers,
// farmhouses and cabins to the two houses.
[[nodiscard]] constexpr bool home(building kind)
{
    switch(kind)
    {

    case building::SMALL_HOUSE:
    case building::LARGE_HOUSE:
    case building::COTTAGE:
    case building::BUNGALOW:
    case building::TERRACED_PAIR:
    case building::FLAT_BLOCK:
    case building::TRAILER:
    case building::FARMHOUSE:
    case building::LARGE_FARMHOUSE:
    case building::HOLIDAY_CABIN:
    case building::HUNTERS_CABIN:
        return true;

    default:
        return false;
    }
}

// The roofed part of the building over a tile of a generated world, as a rectangle of tiles.
[[nodiscard]] BN_CODE_IWRAM bool building_at(int tile_x, int tile_y, int& left, int& top, int& width,
                                             int& height);

[[nodiscard]] constexpr bool is_roof(world_gfx gfx)
{
    // The roof graphics are generated together, red then grey (tools/assets/world.py, ROOF_PARTS); the eaves over
    // front doors came later and sit at the end of the tileset (playtest 1).
    return (gfx >= world_gfx::ROOF_RED_UPPER && gfx <= world_gfx::ROOF_GREY_WEATHER) ||
           gfx == world_gfx::ROOF_RED_EAVE_DOOR || gfx == world_gfx::ROOF_GREY_EAVE_DOOR;
}

}

#endif
