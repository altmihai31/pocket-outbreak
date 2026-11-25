#include "kx_containers.h"

#include "bn_algorithm.h"

#include "kx_character.h"
#include "kx_hash.h"
#include "kx_guns.h"
#include "kx_light.h"
#include "kx_inventory.h"
#include "kx_save.h"
#include "kx_sandbox.h"
#include "kx_timeline.h"
#include "kx_macro_map.h"
#include "kx_world.h"

namespace kx
{

namespace
{
    constexpr int chunk_tiles = world::chunk_tiles;
}

bool container::empty() const
{
    for(const item_stack& stack : items)
    {
        if(! stack.empty())
        {
            return false;
        }
    }

    return true;
}

bool container::add(item_stack& stack)
{
    const item_info& item = info(stack.id);

    if(item.stack > 1 && item.durability == 0)
    {
        for(item_stack& existing : items)
        {
            if(existing.id == stack.id && existing.amount < item.stack)
            {
                int moved = bn::min(item.stack - existing.amount, int(stack.amount));
                existing.amount = uint8_t(existing.amount + moved);
                stack.amount = uint8_t(stack.amount - moved);

                if(stack.amount == 0)
                {
                    stack = item_stack();
                    return true;
                }
            }
        }
    }

    for(item_stack& existing : items)
    {
        if(existing.empty())
        {
            existing = stack;
            stack = item_stack();
            return true;
        }
    }

    return false;
}

void container_store::init()
{
    _records.clear();
    _page_of.clear();
    _pinned = -1;

    for(int16_t& owner : _page_owners)
    {
        owner = -1;
    }
}

int container_store::furniture(int tile_x, int tile_y)
{
    int index = at(tile_x, tile_y);

    if(index >= 0)
    {
        return index;
    }

    loot_table table = world::container_table(tile_x, tile_y);
    return table == loot_table::COUNT ? -1 : add(container_kind::FURNITURE, table, tile_x, tile_y);
}

int container_store::at(int tile_x, int tile_y) const
{
    for(int index = 0, limit = _records.size(); index < limit; ++index)
    {
        const container_record& value = _records[index];

        if(value.tile_x == tile_x && value.tile_y == tile_y)
        {
            return index;
        }
    }

    return -1;
}

int container_store::add(container_kind kind, loot_table table, int tile_x, int tile_y)
{
    container_record value;
    value.tile_x = int16_t(tile_x);
    value.tile_y = int16_t(tile_y);
    value.table = table;
    value.kind = kind;
    value.opened = kind == container_kind::FLOOR || kind == container_kind::SURVIVOR || kind == container_kind::CRATE;

    if(! _records.full())
    {
        _records.push_back(value);
        _page_of.push_back(-1);
        return _records.size() - 1;
    }

    // Full: replace (in place, so other indices stay valid) unopened furniture, which is registered again with
    // the same loot when next faced, or else an emptied corpse or floor pile.
    int victim = -1;

    for(int index = 0, limit = _records.size(); index < limit; ++index)
    {
        const container_record& existing = _records[index];

        if(existing.kind == container_kind::FURNITURE && ! existing.opened)
        {
            victim = index;
            break;
        }

        if(victim < 0 && (existing.kind == container_kind::CORPSE || existing.kind == container_kind::FLOOR ||
                          existing.kind == container_kind::VEHICLE) &&
           existing.opened &&
           (_page_of[index] < 0 || _pages[_page_of[index]].empty()))
        {
            victim = index;
        }
    }

    if(victim >= 0)
    {
        if(_page_of[victim] >= 0)
        {
            _free_page(_page_of[victim]);
        }

        _records[victim] = value;
    }

    return victim;
}

void container_store::relocate(int index, int tile_x, int tile_y, container_kind kind, loot_table table)
{
    container_record& value = _records[index];
    value.tile_x = int16_t(tile_x);
    value.tile_y = int16_t(tile_y);
    value.kind = kind;
    value.table = table;

    if(_page_of[index] >= 0)
    {
        container& page = _pages[_page_of[index]];
        page.tile_x = value.tile_x;
        page.tile_y = value.tile_y;
        page.kind = kind;
        page.table = table;
    }
}

void container_store::_free_page(int page)
{
    int owner = _page_owners[page];

    if(owner >= 0)
    {
        _set_page(owner, -1);
    }

    _page_owners[page] = -1;
}

void container_store::_set_page(int index, int page)
{
    _page_of[index] = int16_t(page);
    _records[index].page = page >= 0 ? 0 : container_record::no_page;

    if(page >= 0)
    {
        _page_owners[page] = int16_t(index);
    }
}

int container_store::_used_stacks() const
{
    int result = 0;

    for(int page = 0; page < max_pages; ++page)
    {
        if(_page_owners[page] >= 0)
        {
            for(const item_stack& stack : _pages[page].items)
            {
                result += ! stack.empty();
            }
        }
    }

    return result;
}

int container_store::room() const
{
    return bn::max(max_stacks - _used_stacks(), 0);
}

int container_store::_find_page()
{
    // A free page, else one whose container has been emptied, else (losing what is left in it) the oldest page
    // of a zombie corpse, then of furniture. Floor piles, the player's own drops, are never taken.
    int empty_page = -1;
    int corpse_page = -1;
    int furniture_page = -1;
    int corpse_owner = max_containers;
    int furniture_owner = max_containers;

    for(int page = 0; page < max_pages; ++page)
    {
        int owner = _page_owners[page];

        if(owner < 0)
        {
            return page;
        }

        if(owner == _pinned)
        {
            continue;
        }

        if(empty_page < 0 && _pages[page].empty())
        {
            empty_page = page;
        }
        else if(_records[owner].kind == container_kind::CORPSE && owner < corpse_owner)
        {
            corpse_page = page;
            corpse_owner = owner;
        }
        else if(_records[owner].kind == container_kind::FURNITURE && owner < furniture_owner)
        {
            furniture_page = page;
            furniture_owner = owner;
        }
    }

    int page = empty_page >= 0 ? empty_page : corpse_page >= 0 ? corpse_page : furniture_page;

    if(page >= 0)
    {
        _free_page(page);
    }

    return page;
}

container& container_store::get(int index)
{
    container_record& value = _records[index];
    _pinned = index;

    if(_page_of[index] < 0)
    {
        int page = _find_page();
        container& target = page >= 0 ? _pages[page] : _overflow;
        target = container();
        target.tile_x = value.tile_x;
        target.tile_y = value.tile_y;
        target.table = value.table;
        target.kind = value.kind;
        target.opened = value.opened;

        if(page < 0)
        {
            return target;
        }

        _set_page(index, page);
    }

    return _pages[_page_of[index]];
}

namespace
{
    // The zone's loot amount in percent, and how much more weapons and ammunition weigh there.
    [[nodiscard]] int zone_loot_percent(int tile_x, int tile_y, int& weapon_weight)
    {
        int cell_x = tile_x >> macro::cell_shift;
        int cell_y = tile_y >> macro::cell_shift;
        weapon_weight = 1;

        switch(macro::zone_at(cell_x, cell_y))
        {

        case macro::zone::VILLAGE:
            return zone_loot::village_percent;

        case macro::zone::TOWN:
            return macro::settlement_quarter(cell_x, cell_y) >= 0 ? zone_loot::city_percent : zone_loot::town_percent;

        case macro::zone::FARM:
            return zone_loot::farm_percent;

        case macro::zone::MILITARY:
            weapon_weight = zone_loot::military_weapon_weight;
            return zone_loot::military_percent;

        default:
            return zone_loot::wild_percent;
        }
    }
}

int container_store::open(int index, int today, int extra_items)
{
    container& value = get(index);

    if(! value.opened)
    {
        value.opened = true;
        _records[index].opened = true;

        // Spec: roll with hash(seed, chunk, container index); the position within the chunk is the index.
        int chunk_x = value.tile_x / chunk_tiles;
        int chunk_y = value.tile_y / chunk_tiles;
        int local = ((value.tile_y % chunk_tiles) * chunk_tiles) + (value.tile_x % chunk_tiles);
        uint32_t seed = hash32(world::seed() ^ hash32(uint32_t((chunk_y << 16) | chunk_x)) ^
                               hash32(uint32_t(local) * 2654435761u + uint32_t(value.kind)));

        // Foraging finds change from day to day.
        if(value.kind == container_kind::FORAGE)
        {
            seed = hash32(seed ^ (uint32_t(today) * 0x9e3779b9u));
        }
        roll_stream rng(seed);

        const loot_table_info& table = info(value.table);
        int span = table.max_items - table.min_items + 1;
        int count = table.min_items + rng.next(span) + extra_items;
        int weapon_weight = 1;
        // The debug menu's amount, times the sandbox's loot setting (milestone 13).
        int loot_percent = (tuning(inventory_param::LOOT_PERCENT).integer() *
                            sandbox::tuning::loot_percent[sandbox::value(sandbox::setting::LOOT)]) / 100;
        int zone_percent = world::test_world() ? 100 : zone_loot_percent(value.tile_x, value.tile_y, weapon_weight);

        if(zone_percent != 100)
        {
            // Rounded at random, so half of one item is one item half the time.
            count = ((count * zone_percent) + rng.next(100)) / 100;
        }

        count = bn::min((count * loot_percent) / 100, int(container::capacity));

        // The save's stack budget (milestone 14): a world this full finds less.
        count = bn::min(count, room());

        auto weight_of = [weapon_weight](const loot_entry& entry)
        {
            const item_info& item = info(entry.item);
            bool armed = item.category == item_category::WEAPON ||
                         is_ammo(entry.item);
            return armed ? entry.weight * weapon_weight : int(entry.weight);
        };

        int total_weight = 0;

        for(const loot_entry& entry : table.entries)
        {
            total_weight += weight_of(entry);
        }

        for(int item = 0; item < count && total_weight > 0; ++item)
        {
            int roll = rng.next(total_weight);
            const loot_entry* chosen = &table.entries[0];

            for(const loot_entry& entry : table.entries)
            {
                int weight = weight_of(entry);

                if(roll < weight)
                {
                    chosen = &entry;
                    break;
                }

                roll -= weight;
            }

            item_stack stack = make_stack(chosen->item, chosen->min_amount +
                                          rng.next(chosen->max_amount - chosen->min_amount + 1));
            const item_info& rolled = info(stack.id);

            // Found weapons and clothes are worn: 40-100% condition.
            if(rolled.durability > 0)
            {
                stack.amount = uint8_t(bn::max(1, (rolled.durability * (40 + rng.next(61))) / 100));
            }

            // Found guns hold anything from nothing to a full load.
            if(is_gun(stack.id))
            {
                set_gun_loaded(stack, rng.next(info(gun_of(stack.id)).capacity + 1));
            }

            // A note (milestone 12.7): a safe's code in this town, this town's stash, or a letter naming a place.
            if(is_note(stack.id))
            {
                int pick = rng.next(100);
                int town = world::settlement_at_tile(value.tile_x, value.tile_y);
                uint16_t safe = pick < 45 ? world::safe_for(value.tile_x, value.tile_y, uint32_t(rng.next(1 << 15))) : 0;
                int stash_x;
                int stash_y;
                int way;

                if(safe)
                {
                    stack.id = item_id::NOTE_SAFE;
                    stack.extra = safe;
                }
                else if(pick < 75 && town >= 0 && world::stash_tile(town, stash_x, stash_y, way))
                {
                    stack.id = item_id::NOTE_STASH;
                    stack.extra = uint16_t(town);
                }
                else
                {
                    if(town < 0)
                    {
                        town = rng.next(bn::max(macro::settlement_count(), 1));
                    }

                    stack.id = item_id::NOTE_LETTER;
                    stack.extra = uint16_t((rng.next(256) << 8) | (town & 0xFF));
                }
            }

            // A house key opens a locked house in the same town (milestone 12.7); 0 if the town has none.
            if(stack.id == item_id::HOUSE_KEY)
            {
                stack.extra = world::key_for(value.tile_x, value.tile_y, uint32_t(rng.next(1 << 15)));
            }

            // Found flashlights have some of their battery left.
            if(stack.id == item_id::FLASHLIGHT)
            {
                int full = tuning(light_param::BATTERY_MINUTES);
                int lowest = tuning(light_param::FOUND_CHARGE_MIN);
                int percent = lowest + rng.next(100 - lowest + 1);
                light::set_flashlight(stack, false, bn::max((full * percent) / 100, 1));
            }

            // Skill books: an early volume more often than a late one.
            if(rolled.category == item_category::BOOK)
            {
                int pick = rng.next(100);
                int volume = 1;

                while(volume < book_volumes && pick >= character_tuning::volume_weights[volume - 1])
                {
                    pick -= character_tuning::volume_weights[volume - 1];
                    ++volume;
                }

                stack.extra = uint16_t(volume);
            }

            // A car key starts the cars parked around where it turns up (kx_vehicles): it remembers the 16x16-tile
            // block it was found in, or its own car's when it was in the glove box. 0 would fit any car.
            if(stack.id == item_id::CAR_KEY)
            {
                constexpr int blocks_per_side = world::width_tiles >> 4;

                if(value.kind == container_kind::VEHICLE)
                {
                    // Off-map: the car's block is in the position (the high bit in the column), unless it was
                    // spawned.
                    int high = (-10 - value.tile_x) >> 1;
                    stack.extra = value.tile_x <= -14 ? 0 : uint16_t(1 + (value.tile_y | (high << 15)));
                }
                else if(value.tile_x >= 0 && value.tile_y >= 0)
                {
                    stack.extra = uint16_t(1 + ((value.tile_y >> 4) * blocks_per_side) + (value.tile_x >> 4));
                }
            }

            // Perishable food was put there some days ago (spec: freshness = now - spawn day, worked out lazily).
            if(rolled.fresh_days > 0)
            {
                // In a fridge, the days it had power count for a quarter (spec: slower spoilage while powered).
                int age = rng.next(rolled.fresh_days * 2);

                if(value.table == loot_table::FRIDGE || value.table == loot_table::FREEZER)
                {
                    // A freezer (milestone 13) keeps food from ageing at all while it has power.
                    int powered = bn::clamp(bn::min(today, timeline::power_off_day()) - (today - age), 0, age);
                    int kept = value.table == loot_table::FREEZER ? 100 : 100 - timeline_tuning::fridge_age_percent;
                    age -= (powered * kept) / 100;
                }

                stack.extra = uint16_t(bn::max(today - age, 0));
            }

            value.add(stack);
        }
    }

    int result = 0;

    for(const item_stack& stack : value.items)
    {
        result += ! stack.empty();
    }

    return result;
}

void container_store::remove(int index)
{
    if(index < 0 || index >= _records.size())
    {
        return;
    }

    if(_page_of[index] >= 0)
    {
        _free_page(_page_of[index]);
    }

    _records.erase(_records.begin() + index);
    _page_of.erase(_page_of.begin() + index);

    for(int16_t& owner : _page_owners)
    {
        if(owner > index)
        {
            --owner;
        }
    }

    _pinned = -1;
}

void container_store::remove_if_empty_floor(int index)
{
    if(index < 0 || index >= _records.size())
    {
        return;
    }

    container_record& value = _records[index];
    bool has_items = _page_of[index] >= 0 && ! _pages[_page_of[index]].empty();

    if(value.kind != container_kind::FLOOR || has_items)
    {
        return;
    }

    if(_page_of[index] >= 0)
    {
        _free_page(_page_of[index]);
    }

    _records.erase(_records.begin() + index);
    _page_of.erase(_page_of.begin() + index);

    // Later records moved down by one.
    for(int16_t& owner : _page_owners)
    {
        if(owner > index)
        {
            --owner;
        }
    }

    _pinned = -1;
}

int container_store::stored_stacks(int& checksum) const
{
    int result = 0;
    checksum = 0;

    for(int index = 0, limit = _records.size(); index < limit; ++index)
    {
        if(_page_of[index] >= 0)
        {
            for(int slot = 0; slot < container::capacity; ++slot)
            {
                const item_stack& stack = _pages[_page_of[index]].items[slot];

                if(! stack.empty())
                {
                    ++result;
                    checksum += (index + 1) * (slot + 1) * ((int(stack.id) + 1) * 256 + stack.amount + stack.extra);
                }
            }
        }
    }

    return result;
}

int container_store::holding() const
{
    int result = 0;

    for(int index = 0, limit = _records.size(); index < limit; ++index)
    {
        result += _page_of[index] >= 0 && ! _pages[_page_of[index]].empty();
    }

    return result;
}

int container_store::fill_percent() const
{
    int used_pages = 0;

    for(int page = 0; page < max_pages; ++page)
    {
        used_pages += _page_owners[page] >= 0 && ! _pages[page].empty();
    }

    return bn::max(bn::max((_records.size() * 100) / max_containers, (used_pages * 100) / max_pages),
                   (_used_stacks() * 100) / max_stacks);
}

void container_store::save(save::writer& out) const
{
    // The records (page: 1 if items follow, else none), then for each container holding items a mask of its
    // filled slots and those stacks.
    out.put(_records.size());

    for(int index = 0, limit = _records.size(); index < limit; ++index)
    {
        container_record saved = _records[index];
        bool has_items = _page_of[index] >= 0 && ! _pages[_page_of[index]].empty();
        saved.page = has_items ? 1 : container_record::no_page;
        out.put(saved);
    }

    for(int index = 0, limit = _records.size(); index < limit; ++index)
    {
        if(_page_of[index] >= 0 && ! _pages[_page_of[index]].empty())
        {
            const container& page = _pages[_page_of[index]];
            uint16_t mask = 0;

            for(int slot = 0; slot < container::capacity; ++slot)
            {
                mask |= page.items[slot].empty() ? 0 : uint16_t(1 << slot);
            }

            out.put(mask);

            for(const item_stack& stack : page.items)
            {
                if(! stack.empty())
                {
                    out.put(stack);
                }
            }
        }
    }
}

void container_store::load(save::reader& in)
{
    init();
    int count = bn::min(in.get<int>(), max_containers);

    for(int index = 0; index < count; ++index)
    {
        _records.push_back(in.get<container_record>());
        _page_of.push_back(-1);
    }

    int next_page = 0;

    for(int index = 0; index < count; ++index)
    {
        container_record& value = _records[index];

        if(value.page == container_record::no_page)
        {
            continue;
        }

        uint16_t mask = in.get<uint16_t>();
        container page;
        page.tile_x = value.tile_x;
        page.tile_y = value.tile_y;
        page.table = value.table;
        page.kind = value.kind;
        page.opened = value.opened;

        for(int slot = 0; slot < container::capacity; ++slot)
        {
            if(mask & (1 << slot))
            {
                in.get(page.items[slot]);
            }
        }

        if(next_page < max_pages)
        {
            _pages[next_page] = page;
            _set_page(index, next_page++);
        }
        else
        {
            value.page = container_record::no_page;
        }
    }
}

}
