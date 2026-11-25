#include "kx_base.h"

#include "bn_math.h"

#include "kx_game.h"
#include "kx_hud_tiles.h"
#include "kx_macro_map.h"
#include "kx_save.h"
#include "kx_world.h"

namespace kx
{

using namespace base_tuning;

const piece_info piece_infos[piece_kinds] = {
    { "BARRICADE", "BARR", item_icon::PIECE_BARRICADE, { { item_id::PLANKS, 3 }, { item_id::NAILS, 6 } }, 2,
      item_id::HAMMER, false, 0, 360, true, 20, magazine::NONE },
    { "WOODEN CRATE", "CRATE", item_icon::PIECE_CRATE, { { item_id::PLANKS, 4 }, { item_id::NAILS, 8 } }, 2,
      item_id::HAMMER, false, 1, 300, true, 25, magazine::NONE },
    { "MOVE A BED", "BED", item_icon::PIECE_BED, {}, 0, item_id::NONE, false, 0, 120, false, 0, magazine::NONE },
    { "RAIN COLLECTOR", "RAIN", item_icon::PIECE_RAIN,
      { { item_id::PLANKS, 4 }, { item_id::NAILS, 4 }, { item_id::GARBAGE_BAG, 1 } }, 3, item_id::HAMMER, false, 2,
      300, true, 30, magazine::NONE },
    { "CAMPFIRE", "FIRE", item_icon::PIECE_FIRE, { { item_id::STONES, 3 }, { item_id::STICKS, 3 } }, 2,
      item_id::NONE, true, 0, 120, false, 5, magazine::NONE },
    { "FARM PLOT", "PLOT", item_icon::PIECE_PLOT, {}, 0, item_id::SHOVEL, false, 0, 240, false, 6, magazine::NONE },
    { "GENERATOR", "GEN", item_icon::PIECE_GENERATOR, { { item_id::GENERATOR, 1 } }, 1, item_id::NONE, false, 0, 180,
      false, 0, magazine::GENERATOR },
    { "BOX TRAP", "TRAP", item_icon::PIECE_TRAP, { { item_id::PLANKS, 2 }, { item_id::NAILS, 4 } }, 2,
      item_id::HAMMER, false, 0, 240, true, 10, magazine::TRAPPING },
    { "CAN ALARM LINE", "CANS", item_icon::PIECE_CANS, { { item_id::STICKS, 2 }, { item_id::NAILS, 6 } }, 2,
      item_id::HAMMER, false, 0, 180, true, 8, magazine::NONE },
    { "WOODEN DOOR", "DOOR", item_icon::PIECE_DOOR,
      { { item_id::PLANKS, 4 }, { item_id::HINGE, 1 }, { item_id::DOORKNOB, 1 } }, 3, item_id::HAMMER, false, 3, 480,
      true, 40, magazine::NONE },
    { "LOG WALL", "LOGS", item_icon::PIECE_LOGS, { { item_id::LOGS, 4 }, { item_id::NAILS, 6 } }, 2, item_id::HAMMER,
      false, 2, 360, true, 30, magazine::NONE },
    { "SNARE", "SNARE", item_icon::PIECE_SNARE, { { item_id::ROPE, 1 }, { item_id::PLANKS, 1 } }, 2, item_id::NONE,
      false, 0, 120, false, 6, magazine::NONE },
};

namespace
{
    // A placed piece: 4 bytes (spec: Save layout, base pieces). Position from the safehouse's corner; bits 0-3
    // kind + 1 (0: free), 4-6 what a barricade covers, 7 a reinforced barricade or a campfire that has gone out;
    // value: barricade health, campfire burnout hour (mod 256), rain collector water.
    struct piece
    {
        uint8_t dx;
        uint8_t dy;
        uint8_t bits;
        uint8_t value;

        [[nodiscard]] bool used() const
        {
            return bits & 15;
        }

        [[nodiscard]] piece_kind kind() const
        {
            return piece_kind((bits & 15) - 1);
        }

        [[nodiscard]] covered covers() const
        {
            return covered((bits >> 4) & 7);
        }

        [[nodiscard]] bool reinforced() const
        {
            return bits & 0x80;
        }
    };

    static_assert(sizeof(piece) == 4);

    struct safehouse
    {
        int16_t left = 0;
        int16_t top = 0;
        uint8_t width = 0;          // 0: unused
        uint8_t height = 0;
        uint8_t count = 0;
        uint8_t unused = 0;
        int32_t last_visit = 0;     // in-game minute
        piece pieces[max_pieces] = {};

        [[nodiscard]] bool contains(int tile_x, int tile_y, int margin = 0) const
        {
            return width && tile_x >= left - margin && tile_y >= top - margin && tile_x < left + width + margin &&
                   tile_y < top + height + margin;
        }
    };

    BN_DATA_EWRAM_BSS safehouse safehouses[max_safehouses];     // EWRAM: IWRAM is kept for code and the stack

    [[nodiscard]] int safehouse_at(int tile_x, int tile_y)
    {
        for(int index = 0; index < max_safehouses; ++index)
        {
            if(safehouses[index].contains(tile_x, tile_y))
            {
                return index;
            }
        }

        return -1;
    }

    [[nodiscard]] piece* piece_at(int tile_x, int tile_y)
    {
        int index = safehouse_at(tile_x, tile_y);

        if(index < 0)
        {
            return nullptr;
        }

        safehouse& value = safehouses[index];

        for(piece& item : value.pieces)
        {
            if(item.used() && value.left + item.dx == tile_x && value.top + item.dy == tile_y)
            {
                return &item;
            }
        }

        return nullptr;
    }

    // The area a new safehouse covers: the building the tile is in (with a margin), or around the tile.
    void new_area(int tile_x, int tile_y, safehouse& value)
    {
        int left;
        int top;
        int width;
        int height;

        if(world::building_over(tile_x, tile_y, left, top, width, height))
        {
            left -= safehouse_margin;
            top -= safehouse_margin;
            width += safehouse_margin * 2;
            height += safehouse_margin * 2;
        }
        else
        {
            left = tile_x - outdoor_radius;
            top = tile_y - outdoor_radius;
            width = height = (outdoor_radius * 2) + 1;
        }

        value.left = int16_t(left);
        value.top = int16_t(top);
        value.width = uint8_t(bn::min(width, 255));
        value.height = uint8_t(bn::min(height, 255));
    }

    constexpr uint8_t fire_out_bit = 0x80;

    // The burnout hour is kept mod 256, so a fire is marked out once it has gone (expire_fires, every hour):
    // otherwise an old fire would look lit again for a few hours every 256.
    [[nodiscard]] bool burning(const piece& value, int now_hour)
    {
        int left = (int(value.value) - now_hour) & 0xFF;
        return ! (value.bits & fire_out_bit) && left > 0 && left <= max_fuel_hours;
    }

    [[nodiscard]] world_gfx covered_gfx(covered value)
    {
        switch(value)
        {

        case covered::WINDOW_FACE:
            return world_gfx::WINDOW_FACE;

        case covered::WINDOW_TOP:
            return world_gfx::WINDOW_TOP;

        case covered::BROKEN_FACE:
            return world_gfx::BROKEN_FACE;

        case covered::BROKEN_TOP:
            return world_gfx::BROKEN_TOP;

        case covered::HUNG_DOOR_FACE:
            return world_gfx::DOOR_FACE_BROKEN;

        case covered::HUNG_DOOR_TOP:
            return world_gfx::DOOR_TOP_BROKEN;

        default:
            return world_gfx::EMPTY;
        }
    }
}

namespace base
{

void reset()
{
    for(safehouse& value : safehouses)
    {
        value = safehouse();
    }
}

int pieces_percent()
{
    int result = 0;

    for(const safehouse& value : safehouses)
    {
        result = bn::max(result, (int(value.count) * 100) / max_pieces);
    }

    return result;
}

bool room_at(int tile_x, int tile_y, int& used)
{
    int index = safehouse_at(tile_x, tile_y);

    if(index >= 0)
    {
        used = safehouses[index].count;
        return used < max_pieces;
    }

    used = 0;

    for(const safehouse& value : safehouses)
    {
        if(! value.width)
        {
            return true;
        }
    }

    return false;
}

int pieces_at(int tile_x, int tile_y)
{
    int index = safehouse_at(tile_x, tile_y);
    return index < 0 ? -1 : safehouses[index].count;
}

bool add(piece_kind kind, int tile_x, int tile_y, covered covers, int value, int now_minute)
{
    int index = safehouse_at(tile_x, tile_y);

    if(index < 0)
    {
        for(int candidate = 0; candidate < max_safehouses && index < 0; ++candidate)
        {
            if(! safehouses[candidate].width)
            {
                index = candidate;
                new_area(tile_x, tile_y, safehouses[index]);
            }
        }
    }

    if(index < 0 || safehouses[index].count >= max_pieces)
    {
        return false;
    }

    safehouse& house = safehouses[index];
    house.last_visit = now_minute;

    for(piece& item : house.pieces)
    {
        if(! item.used())
        {
            item.dx = uint8_t(tile_x - house.left);
            item.dy = uint8_t(tile_y - house.top);
            item.bits = uint8_t((int(kind) + 1) | (int(covers) << 4));
            item.value = uint8_t(value);
            ++house.count;
            return true;
        }
    }

    return false;
}

bool find(int tile_x, int tile_y, piece_kind& kind)
{
    if(const piece* item = piece_at(tile_x, tile_y))
    {
        kind = item->kind();
        return true;
    }

    return false;
}

namespace
{
    [[nodiscard]] bool breaks(piece_kind kind)
    {
        return kind == piece_kind::BARRICADE || kind == piece_kind::WOODEN_DOOR || kind == piece_kind::LOG_WALL;
    }
}

int barricade_health(int tile_x, int tile_y, int& full)
{
    const piece* item = piece_at(tile_x, tile_y);

    if(! item || ! breaks(item->kind()))
    {
        full = 0;
        return 0;
    }

    full = item->kind() == piece_kind::WOODEN_DOOR ? base_tuning::wooden_door_health :
           item->kind() == piece_kind::LOG_WALL ? base_tuning::log_wall_health :
           item->reinforced() ? reinforced_health : base_tuning::barricade_health;
    return item->value;
}

void repair_barricade(int tile_x, int tile_y, bool reinforced)
{
    if(piece* item = piece_at(tile_x, tile_y))
    {
        if(reinforced)
        {
            item->bits = uint8_t(item->bits | 0x80);
        }

        item->value = uint8_t(item->reinforced() ? reinforced_health : base_tuning::barricade_health);

        // Mended: the boards look whole again (playtest 8).
        world_gfx wall = world::wall_gfx(tile_x, tile_y);

        if(item->kind() == piece_kind::BARRICADE && world::is_barricade(wall))
        {
            static_cast<void>(world::set_wall_gfx(tile_x, tile_y,
                                                  world::barricade_gfx(world::barricade_in_side_wall(wall), 1, 1)));
        }
    }
}

bool damage_barricade(game& owner, int tile_x, int tile_y, int amount)
{
    piece* item = piece_at(tile_x, tile_y);

    if(! item || ! breaks(item->kind()))
    {
        return false;
    }

    if(item->value > amount)
    {
        int before = item->value;
        item->value = uint8_t(item->value - amount);

        // Playtest 8: a barricade shows its wear, and says when it is about to give.
        if(item->kind() == piece_kind::BARRICADE)
        {
            int full = item->reinforced() ? reinforced_health : base_tuning::barricade_health;
            world_gfx wall = world::wall_gfx(tile_x, tile_y);
            world_gfx look = world::barricade_gfx(world::barricade_in_side_wall(wall), item->value, full);

            if(look != wall && world::is_barricade(wall))
            {
                static_cast<void>(world::set_wall_gfx(tile_x, tile_y, look));
                owner.refresh_tile(tile_x, tile_y);
            }

            if(before * 100 >= full * base_tuning::failing_percent &&
               item->value * 100 < full * base_tuning::failing_percent)
            {
                owner.toast("BARRICADE FAILING", hud_tiles::bank_danger, 120);
            }
        }

        return false;
    }

    // Broken: the doorway or window it covered is back; a wooden door is left broken in its doorway; a log wall is
    // gone.
    covered covers = item->covers();
    piece_kind kind = item->kind();
    item->bits = 0;
    --safehouses[safehouse_at(tile_x, tile_y)].count;

    if(kind != piece_kind::BARRICADE)
    {
        world_gfx left = kind == piece_kind::WOODEN_DOOR ?
                         world::door_gfx(world::door_state::BROKEN, covers == covered::DOOR_TOP) : world_gfx::EMPTY;
        static_cast<void>(world::set_wall_gfx(tile_x, tile_y, left, kind == piece_kind::LOG_WALL));
        owner.refresh_tile(tile_x, tile_y);
        return true;
    }

    world_gfx under = covered_gfx(covers);
    // A smashed window, or the door behind the boards (broken with them), is remembered; the rest are generated.
    bool generated = covers != covered::BROKEN_FACE && covers != covered::BROKEN_TOP &&
                     covers != covered::HUNG_DOOR_FACE && covers != covered::HUNG_DOOR_TOP;
    static_cast<void>(world::set_wall_gfx(tile_x, tile_y, under, generated));
    owner.refresh_tile(tile_x, tile_y);
    return true;
}

bool fire_lit(int tile_x, int tile_y, int now_hour)
{
    const piece* item = piece_at(tile_x, tile_y);
    return item && item->kind() == piece_kind::CAMPFIRE && burning(*item, now_hour);
}

bool fire_near(int tile_x, int tile_y, int tiles, int now_hour)
{
    for(const safehouse& house : safehouses)
    {
        if(! house.contains(tile_x, tile_y, tiles))
        {
            continue;
        }

        for(const piece& item : house.pieces)
        {
            if(item.used() && item.kind() == piece_kind::CAMPFIRE && burning(item, now_hour) &&
               bn::abs(house.left + item.dx - tile_x) <= tiles && bn::abs(house.top + item.dy - tile_y) <= tiles)
            {
                return true;
            }
        }
    }

    return false;
}

void add_fuel(int tile_x, int tile_y, int hours, int now_hour)
{
    piece* item = piece_at(tile_x, tile_y);

    if(! item || item->kind() != piece_kind::CAMPFIRE)
    {
        return;
    }

    int left = burning(*item, now_hour) ? (int(item->value) - now_hour) & 0xFF : 0;
    item->value = uint8_t(now_hour + bn::min(left + hours, max_fuel_hours));
    item->bits = uint8_t(item->bits & ~fire_out_bit);
}

bool safehouse_centre(int index, int& tile_x, int& tile_y)
{
    const safehouse& house = safehouses[index];

    if(! house.width)
    {
        return false;
    }

    tile_x = house.left + (house.width / 2);
    tile_y = house.top + (house.height / 2);
    return true;
}

void expire_fires(int now_hour)
{
    for(safehouse& house : safehouses)
    {
        if(! house.width)
        {
            continue;
        }

        for(piece& item : house.pieces)
        {
            if(item.used() && item.kind() == piece_kind::CAMPFIRE && ! burning(item, now_hour))
            {
                item.bits = uint8_t(item.bits | fire_out_bit);
            }
        }
    }
}

int rain_water(int tile_x, int tile_y)
{
    const piece* item = piece_at(tile_x, tile_y);
    return item && item->kind() == piece_kind::RAIN_COLLECTOR ? item->value : -1;
}

bool take_rain_water(int tile_x, int tile_y)
{
    piece* item = piece_at(tile_x, tile_y);

    if(! item || item->kind() != piece_kind::RAIN_COLLECTOR || ! item->value)
    {
        return false;
    }

    --item->value;
    return true;
}

void rain(int amount)
{
    for(safehouse& house : safehouses)
    {
        for(piece& item : house.pieces)
        {
            if(item.used() && item.kind() == piece_kind::RAIN_COLLECTOR)
            {
                item.value = uint8_t(bn::min(item.value + amount, rain_capacity));
            }
        }
    }
}

int can_lines_in(int left_tile, int top_tile, int right_tile, int bottom_tile, int* tiles_x, int* tiles_y,
                 int capacity)
{
    int found = 0;

    for(const safehouse& house : safehouses)
    {
        if(! house.width || house.left > right_tile || house.top > bottom_tile ||
           house.left + house.width < left_tile || house.top + house.height < top_tile)
        {
            continue;
        }

        for(const piece& item : house.pieces)
        {
            int x = house.left + item.dx;
            int y = house.top + item.dy;

            if(found < capacity && item.used() && item.kind() == piece_kind::CAN_LINE && x >= left_tile &&
               x <= right_tile && y >= top_tile && y <= bottom_tile)
            {
                tiles_x[found] = x;
                tiles_y[found] = y;
                ++found;
            }
        }
    }

    return found;
}

int lit_fires_in(int left_tile, int top_tile, int right_tile, int bottom_tile, int now_hour, int* tiles_x,
                 int* tiles_y, int capacity)
{
    int found = 0;

    for(const safehouse& house : safehouses)
    {
        if(! house.width || house.left > right_tile || house.top > bottom_tile ||
           house.left + house.width < left_tile || house.top + house.height < top_tile)
        {
            continue;
        }

        for(const piece& item : house.pieces)
        {
            int x = house.left + item.dx;
            int y = house.top + item.dy;

            if(found < capacity && item.used() && item.kind() == piece_kind::CAMPFIRE && burning(item, now_hour) &&
               x >= left_tile && x <= right_tile && y >= top_tile && y <= bottom_tile)
            {
                tiles_x[found] = x;
                tiles_y[found] = y;
                ++found;
            }
        }
    }

    return found;
}

void update(game& owner)
{
    int player_x = owner.hero().position().x().integer() >> world::tile_shift;
    int player_y = owner.hero().position().y().integer() >> world::tile_shift;
    int now = owner.world_minute();

    for(int index = 0; index < max_safehouses; ++index)
    {
        safehouse& house = safehouses[index];

        if(! house.contains(player_x, player_y, visit_tiles))
        {
            continue;
        }

        int hours = (now - house.last_visit) / 60;
        house.last_visit = now;

        if(hours <= 0)
        {
            continue;
        }

        // The hordes that passed while the player was away: attacks on the weakest barricade.
        int cell_x = (house.left + (house.width / 2)) >> macro::cell_shift;
        int cell_y = (house.top + (house.height / 2)) >> macro::cell_shift;
        int attacks = bn::min((hours * macro::horde_count(cell_x, cell_y)) / zombie_hours_per_attack, max_attacks);
        int broken = 0;
        int damaged = 0;

        for(int attack = 0; attack < attacks; ++attack)
        {
            piece* weakest = nullptr;

            for(piece& item : house.pieces)
            {
                if(item.used() && breaks(item.kind()) && (! weakest || item.value < weakest->value))
                {
                    weakest = &item;
                }
            }

            if(! weakest)
            {
                break;
            }

            int damage = attack_damage_min + owner.random().get_int(attack_damage_max - attack_damage_min + 1);
            damaged += damage;

            if(damage_barricade(owner, house.left + weakest->dx, house.top + weakest->dy, damage))
            {
                ++broken;
            }
        }

        if(broken)
        {
            owner.toast("ZOMBIES BROKE IN WHILE AWAY", hud_tiles::bank_danger, 180);
        }
        else if(damaged)
        {
            owner.toast("YOUR BARRICADES WERE ATTACKED", hud_tiles::bank_danger, 150);
        }
    }
}

void save(save::writer& out)
{
    out.put(safehouses);
}

void load(save::reader& in)
{
    in.get(safehouses);
}

}

}
