#ifndef KX_BASE_H
#define KX_BASE_H

#include "kx_items.h"
#include "kx_recipes.h"

namespace kx
{

class game;

namespace save
{
    class reader;
    class writer;
}

// Base building (spec: Base building, farming and cooking): existing buildings are fortified, not built from
// nothing. Up to 3 safehouses with 64 placed pieces each; every piece is a background tile (a remembered wall
// graphic, kx_world) plus a 4-byte record here, so a whole base costs no sprites.
enum class piece_kind : uint8_t
{
    BARRICADE,
    CRATE,
    BED,                // moved, not built
    RAIN_COLLECTOR,
    CAMPFIRE,
    PLOT,               // a farm plot (kx_farm keeps it, not a safehouse)
    GENERATOR,          // kx_power keeps it
    TRAP,               // a box trap (kx_traps keeps it)
    CAN_LINE,           // a can alarm line: rattles when crossed
    WOODEN_DOOR,        // milestone 12.7: a door in an empty doorway; zombies break it (health in the record)
    LOG_WALL,           // a wall of logs outdoors; zombies break it like a barricade
    SNARE,              // milestone 13: a rabbit snare (kx_traps keeps it, with the box traps)
    COUNT
};

constexpr int piece_kinds = int(piece_kind::COUNT);

struct material
{
    item_id item;
    uint8_t amount;
};

struct piece_info
{
    const char* name;
    const char* short_name;         // build bar label (mockup 06)
    item_icon icon;
    material materials[3];
    uint8_t material_count;
    item_id tool;                   // needs this carried: HAMMER, SHOVEL or NONE
    bool matches;                   // needs matches carried (not used up)
    uint8_t carpentry;              // Carpentry level needed
    uint16_t build_frames;          // holding A
    bool loud;                      // hammering: noise every second
    uint8_t xp;                     // Carpentry XP when done (Farming for a plot)
    magazine needs;                 // a magazine read first (generators, traps)
};

// What a barricade covers (restored when it breaks): a doorway or a window, in a front wall (FACE) or a side wall
// seen from above (TOP).
enum class covered : uint8_t
{
    DOOR_FACE,
    DOOR_TOP,
    WINDOW_FACE,
    WINDOW_TOP,
    BROKEN_FACE,
    BROKEN_TOP,
    HUNG_DOOR_FACE,     // a doorway with a door (milestone 12.7): the door is broken once the barricade is
    HUNG_DOOR_TOP
};

[[nodiscard]] constexpr bool top_wall(covered value)
{
    return int(value) & 1;
}

namespace base_tuning
{
    constexpr int max_safehouses = 3;
    constexpr int max_pieces = 64;
    constexpr int safehouse_margin = 10;            // tiles around a building that belong to its safehouse
    constexpr int outdoor_radius = 10;              // a safehouse started outdoors covers this far around
    constexpr int reach = 3;                        // build mode: tiles from the player the cursor goes

    constexpr int barricade_health = 100;           // spec: health 100, reinforced 250 (Carpentry 3)
    constexpr int reinforced_health = 250;
    constexpr int reinforce_level = 3;
    constexpr int window_health = 30;               // a window takes a few zombie hits
    constexpr int zombie_damage = 10;               // per zombie hit on a barricade or window
    constexpr int zombie_hit_frames = 45;
    constexpr int bash_reach = 22;                  // px from the obstacle's centre a zombie hits it from
    constexpr int bang_noise = 6;                   // tiles: a zombie banging on boards
    constexpr int glass_noise = 12;                 // spec: breaking glass 12
    constexpr int door_health = 120;                // milestone 12.7: a closed door takes 12 zombie hits
    constexpr int wooden_door_health = 200;         // a built one 20
    constexpr int log_wall_health = 150;            // a log wall 15
    constexpr int door_break_noise = 8;
    constexpr int door_close_noise = 2;
    constexpr int hammer_noise = 12;                // spec: hammering is loud (noise radius 12)

    constexpr int campfire_hours = 6;               // a new fire burns this long
    constexpr int fuel_hours = 2;                   // per plank or stick bundle added
    constexpr int max_fuel_hours = 12;
    constexpr int warmth_tiles = 3;                 // warms and dries within this
    constexpr int warmth_insulation = 6;
    constexpr int drying_percent = 300;
    constexpr int light_tiles = 4;                  // light to read by
    constexpr int can_noise = 10;                   // tiles: a can alarm line rattling
    constexpr int can_rattle_frames = 90;           // a line rattles at most this often

    constexpr int crate_capacity = 4000;            // spec: 40 kg (10 g units)
    constexpr int rain_capacity = 40;               // drinks a rain collector holds

    // Horde passes while the player was away (spec: rolled on approach): an attack per this many zombie-hours
    // in the safehouse's macro cell, each one taking this much off the weakest barricade.
    constexpr int zombie_hours_per_attack = 40;
    constexpr int max_attacks = 30;
    constexpr int attack_damage_min = 15;
    constexpr int attack_damage_max = 40;
    constexpr int visit_tiles = 24;                 // within this of a safehouse counts as a visit
}

extern const piece_info piece_infos[piece_kinds];

[[nodiscard]] inline const piece_info& info(piece_kind value)
{
    return piece_infos[int(value)];
}

namespace base
{
    // A new world: no safehouses.
    void reset();

    // Room for a piece on a tile: the safehouse it would join (or a new one); false if all 3 safehouses are
    // elsewhere or its 64 pieces are used. used / capacity describe that safehouse (used 0 for a new one).
    [[nodiscard]] bool room_at(int tile_x, int tile_y, int& used);

    // The fullest safehouse's share of its piece limit, in percent.
    [[nodiscard]] int pieces_percent();

    // Pieces in the safehouse a tile belongs to, or -1 if it is in none.
    [[nodiscard]] int pieces_at(int tile_x, int tile_y);

    // Adds a piece (its tile graphic is the caller's).
    bool add(piece_kind kind, int tile_x, int tile_y, covered covers, int value, int now_minute);

    // The piece on a tile, if any.
    [[nodiscard]] bool find(int tile_x, int tile_y, piece_kind& kind);

    // Barricades, and since milestone 12.7 wooden doors and log walls (pieces zombies break): health, and its full
    // health (0 if there is none).
    [[nodiscard]] int barricade_health(int tile_x, int tile_y, int& full);

    // Mends or reinforces a barricade to full health.
    void repair_barricade(int tile_x, int tile_y, bool reinforced);

    // Damage to a barricade; when it breaks, the record goes and what it covered comes back (returns true).
    bool damage_barricade(game& owner, int tile_x, int tile_y, int amount);

    // Campfires: burning, a lit one within a distance, and adding fuel (or relighting).
    [[nodiscard]] bool fire_lit(int tile_x, int tile_y, int now_hour);
    [[nodiscard]] bool fire_near(int tile_x, int tile_y, int tiles, int now_hour);

    // The middle of a safehouse (tiles), for the map; false for an unused one (index 0..max_safehouses-1).
    [[nodiscard]] bool safehouse_centre(int index, int& tile_x, int& tile_y);

    // Marks campfires that have burnt out, so they stay out (their burnout hour is kept mod 256). Every hour.
    void expire_fires(int now_hour);
    void add_fuel(int tile_x, int tile_y, int hours, int now_hour);

    // Rain collectors: drinks of water in one, taking one, and rain filling every collector (spec: fills in rain).
    [[nodiscard]] int rain_water(int tile_x, int tile_y);
    bool take_rain_water(int tile_x, int tile_y);
    void rain(int amount);

    // Can alarm lines on screen, up to `capacity`.
    int can_lines_in(int left_tile, int top_tile, int right_tile, int bottom_tile, int* tiles_x, int* tiles_y,
                     int capacity);

    // Lit campfires on screen (for the flame sprites), up to `capacity`.
    int lit_fires_in(int left_tile, int top_tile, int right_tile, int bottom_tile, int now_hour, int* tiles_x,
                     int* tiles_y, int capacity);

    // Once a second: on approaching a safehouse after some time away, the horde passes are rolled against its
    // barricades.
    void update(game& owner);

    void save(save::writer& out);
    void load(save::reader& in);
}

}

#endif
