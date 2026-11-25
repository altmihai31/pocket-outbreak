#include "kx_survivors.h"

#include "bn_math.h"

#include "kx_game.h"
#include "kx_hash.h"
#include "kx_horde_tuning.h"
#include "kx_macro_map.h"
#include "kx_save.h"
#include "kx_world.h"

namespace kx::survivors
{

namespace
{
    enum class state : uint8_t
    {
        NONE,
        RISEN,          // out in the world as a zombie (not materialised)
        WALKING,        // materialised near the player
        PUT_DOWN        // killed: the gear lies on its body
    };

    struct past_survivor
    {
        int16_t tile_x = 0;
        int16_t tile_y = 0;
        state current = state::NONE;
        uint8_t order = 0;                                  // which death (the oldest is replaced first)
        item_id worn[int(equip_slot::COUNT)] = {};
    };

    BN_DATA_EWRAM_BSS past_survivor survivors_list[max_survivors];  // EWRAM: IWRAM is kept for code and the stack
    int death_count = 0;

    // Zombies of past survivors appear out of sight, like the hordes' (kx_horde_tuning.h).
    constexpr int screen_half_columns = 8;
    constexpr int screen_half_rows = 6;

    [[nodiscard]] int gear_index(game& owner, int id)
    {
        return owner.containers().at(gear_tile_x, id);
    }

    // The gear becomes a body on the ground at a tile (or the nearest one free of containers).
    void lay_down(game& owner, int id, int tile_x, int tile_y)
    {
        container_store& containers = owner.containers();
        int index = gear_index(owner, id);

        if(index < 0)
        {
            return;
        }

        for(int radius = 0; radius <= 2; ++radius)
        {
            for(int y = tile_y - radius; y <= tile_y + radius; ++y)
            {
                for(int x = tile_x - radius; x <= tile_x + radius; ++x)
                {
                    if(containers.at(x, y) < 0 && ! world::solid(x, y))
                    {
                        containers.relocate(index, x, y, container_kind::CORPSE, loot_table::BODY);
                        return;
                    }
                }
            }
        }
    }
}

void reset()
{
    for(past_survivor& value : survivors_list)
    {
        value = past_survivor();
    }

    death_count = 0;
}

void bury(game& owner)
{
    if(world::test_world())
    {
        return;
    }

    // A free entry, else the oldest: its gear stays on the ground as a body where its zombie was last.
    int id = -1;

    for(int index = 0; index < max_survivors && id < 0; ++index)
    {
        if(survivors_list[index].current == state::NONE)
        {
            id = index;
        }
    }

    if(id < 0)
    {
        id = 0;

        for(int index = 1; index < max_survivors; ++index)
        {
            if(survivors_list[index].order < survivors_list[id].order)
            {
                id = index;
            }
        }

        if(survivors_list[id].current != state::PUT_DOWN)
        {
            lay_down(owner, id, survivors_list[id].tile_x, survivors_list[id].tile_y);
        }
    }

    const player& hero = owner.hero();
    const inventory& items = hero.items();
    past_survivor& value = survivors_list[id];
    value = past_survivor();
    value.tile_x = int16_t(hero.position().x().integer() >> world::tile_shift);
    value.tile_y = int16_t((hero.position().y().integer() - 1) >> world::tile_shift);
    value.current = state::RISEN;
    value.order = uint8_t(bn::min(++death_count, 255));

    for(int slot = 0; slot < int(equip_slot::COUNT); ++slot)
    {
        value.worn[slot] = items.equipped(equip_slot(slot)).id;
    }

    // The gear: what was worn and held first, then the bag. What does not fit stays in a sack where they fell.
    container_store& containers = owner.containers();
    int index = containers.add(container_kind::SURVIVOR, loot_table::BODY, gear_tile_x, id);
    item_stack leftovers[inventory::slots + int(equip_slot::COUNT)];
    int leftover_count = 0;

    auto keep = [&](item_stack stack)
    {
        if(stack.empty())
        {
            return;
        }

        if(index < 0 || ! containers.get(index).add(stack))
        {
            leftovers[leftover_count++] = stack;
        }
    };

    for(int slot = 0; slot < int(equip_slot::COUNT); ++slot)
    {
        keep(items.equipped(equip_slot(slot)));
    }

    for(int slot = 0; slot < inventory::slots; ++slot)
    {
        keep(items.slot(slot));
    }

    for(int leftover = 0; leftover < leftover_count; ++leftover)
    {
        static_cast<void>(owner.drop_item(leftovers[leftover]));
    }
}

void update(game& owner)
{
    if(world::test_world())
    {
        return;
    }

    int player_x = owner.hero().position().x().integer() >> world::tile_shift;
    int player_y = owner.hero().position().y().integer() >> world::tile_shift;

    for(int id = 0; id < max_survivors; ++id)
    {
        past_survivor& value = survivors_list[id];

        if(value.current != state::RISEN)
        {
            continue;
        }

        int dx = bn::abs(value.tile_x - player_x);
        int dy = bn::abs(value.tile_y - player_y);
        bool near = bn::max(dx, dy) <= horde::active_tiles;
        bool hidden = dx > screen_half_columns || dy > screen_half_rows;

        if(near && hidden && world::loaded(value.tile_x, value.tile_y) && ! world::solid(value.tile_x, value.tile_y) &&
           owner.zombies().spawn_survivor(bn::fixed_point((value.tile_x * world::tile_size) + (world::tile_size / 2),
                                                          (value.tile_y * world::tile_size) + 12), id, value.worn,
                                          owner.random()))
        {
            value.current = state::WALKING;
        }
    }
}

void left(int id, const bn::fixed_point& position)
{
    past_survivor& value = survivors_list[id];
    value.tile_x = int16_t(position.x().integer() >> world::tile_shift);
    value.tile_y = int16_t((position.y().integer() - 1) >> world::tile_shift);
    value.current = state::RISEN;
}

void seen_at(int id, const bn::fixed_point& position)
{
    past_survivor& value = survivors_list[id];
    value.tile_x = int16_t(position.x().integer() >> world::tile_shift);
    value.tile_y = int16_t((position.y().integer() - 1) >> world::tile_shift);
}

void killed(game& owner, int id, const bn::fixed_point& position)
{
    past_survivor& value = survivors_list[id];
    value.tile_x = int16_t(position.x().integer() >> world::tile_shift);
    value.tile_y = int16_t((position.y().integer() - 1) >> world::tile_shift);
    value.current = state::PUT_DOWN;
    lay_down(owner, id, value.tile_x, value.tile_y);
}

const item_id* worn(int id)
{
    return survivors_list[id].worn;
}

void position(int id, int& tile_x, int& tile_y)
{
    tile_x = survivors_list[id].tile_x;
    tile_y = survivors_list[id].tile_y;
}

int deaths()
{
    return death_count;
}

void start_tile(int& tile_x, int& tile_y)
{
    // The villages (the spec's low-tier towns); one picked by the seed and the number of deaths.
    int villages[macro::max_settlements];
    int village_count = 0;

    for(int index = 0; index < macro::settlement_count(); ++index)
    {
        if(macro::settlement_at(index).kind == macro::zone::VILLAGE)
        {
            villages[village_count++] = index;
        }
    }

    if(! village_count)
    {
        world::start_tile(tile_x, tile_y);
        return;
    }

    uint32_t roll = hash32(world::seed() ^ (uint32_t(death_count) * 0x9e3779b9u));
    const macro::settlement& start = macro::settlement_at(villages[roll % uint32_t(village_count)]);
    tile_x = (start.cell_x * macro::cell_tiles) + (macro::cell_tiles / 2);
    tile_y = (start.cell_y * macro::cell_tiles) + (macro::cell_tiles / 2);
}

void save(save::writer& out)
{
    // A zombie walking near the player is saved as risen where it stands (it is not in the horde counts).
    out.put(death_count);
    out.put(survivors_list);
}

void load(save::reader& in)
{
    in.get(death_count);
    in.get(survivors_list);

    for(past_survivor& value : survivors_list)
    {
        if(value.current == state::WALKING)
        {
            value.current = state::RISEN;
        }
    }
}

}
