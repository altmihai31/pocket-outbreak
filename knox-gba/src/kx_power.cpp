#include "kx_power.h"

#include "bn_math.h"

#include "kx_containers.h"
#include "kx_game.h"
#include "kx_save.h"
#include "kx_timeline.h"
#include "kx_world.h"

namespace kx
{

using namespace power_tuning;

namespace
{
    constexpr int minutes_per_day = 24 * 60;

    // A generator: 16 bytes. bits 0 used, 1 on.
    struct generator
    {
        int16_t x;
        int16_t y;
        int16_t fuel;               // minutes of fuel when last worked out
        int16_t fridge_credit;      // powered fridge minutes not yet a whole day
        int32_t since;              // the in-game minute it was last worked out
        uint8_t bits;
        uint8_t unused[3];

        [[nodiscard]] bool used() const
        {
            return bits & 1;
        }

        [[nodiscard]] bool on() const
        {
            return bits & 2;
        }
    };

    static_assert(sizeof(generator) == 16);

    BN_DATA_EWRAM_BSS generator generators[max_generators];

    // Burns the fuel since it was last worked out; returns the minutes it ran.
    int settle(generator& value, int now_minute)
    {
        int ran = 0;

        if(value.on() && now_minute > value.since)
        {
            ran = bn::min(now_minute - int(value.since), int(value.fuel));
            value.fuel = int16_t(value.fuel - ran);

            // Fridges near it are credited at the fridge spoilage rate when the player is next close by.
            int credit = value.fridge_credit + ((ran * (100 - timeline_tuning::fridge_age_percent)) / 100);
            value.fridge_credit = int16_t(bn::min(credit, 30000));

            if(value.fuel <= 0)
            {
                value.bits = uint8_t(value.bits & ~2);
            }
        }

        value.since = bn::max(int(value.since), now_minute);
        return ran;
    }

    // Food in opened fridges near a generator keeps longer: each whole day of running (counted at the fridge
    // spoilage rate) moves their stored freshness day on by one.
    void credit_fridges(game& owner, generator& value)
    {
        while(value.fridge_credit >= minutes_per_day)
        {
            value.fridge_credit = int16_t(value.fridge_credit - minutes_per_day);
            container_store& store = owner.containers();
            int today = owner.day();

            for(int index = 0; index < store.count(); ++index)
            {
                const container_record& record = store.record(index);

                if(record.table != loot_table::FRIDGE || record.page == container_record::no_page ||
                   bn::abs(record.tile_x - value.x) > reach_tiles || bn::abs(record.tile_y - value.y) > reach_tiles)
                {
                    continue;
                }

                for(item_stack& stack : store.get(index).items)
                {
                    if(! stack.empty() && info(stack.id).fresh_days > 0 && stack.extra < today)
                    {
                        ++stack.extra;
                    }
                }
            }
        }
    }
}

namespace power
{

void reset()
{
    for(generator& value : generators)
    {
        value = generator();
    }
}

int count()
{
    int result = 0;

    for(const generator& value : generators)
    {
        result += value.used();
    }

    return result;
}

bool add(int tile_x, int tile_y, int now_minute)
{
    for(generator& value : generators)
    {
        if(! value.used())
        {
            value = generator();
            value.x = int16_t(tile_x);
            value.y = int16_t(tile_y);
            value.since = now_minute;
            value.bits = 1;
            return true;
        }
    }

    return false;
}

int at(int tile_x, int tile_y)
{
    for(int index = 0; index < max_generators; ++index)
    {
        const generator& value = generators[index];

        if(value.used() && value.x == tile_x && value.y == tile_y)
        {
            return index;
        }
    }

    return -1;
}

int fuel(int index, int now_minute)
{
    generator& value = generators[index];
    static_cast<void>(settle(value, now_minute));
    return value.fuel;
}

bool running(int index, int now_minute)
{
    generator& value = generators[index];
    static_cast<void>(settle(value, now_minute));
    return value.on();
}

bool switch_on(int index, bool on, int now_minute)
{
    generator& value = generators[index];
    static_cast<void>(settle(value, now_minute));

    if(on && value.fuel <= 0)
    {
        return false;
    }

    value.bits = uint8_t(on ? (value.bits | 2) : (value.bits & ~2));
    return true;
}

int refuel(int index, int litres, int now_minute)
{
    generator& value = generators[index];
    static_cast<void>(settle(value, now_minute));
    int room = (tank_minutes - value.fuel) / minutes_per_litre;
    int taken = bn::clamp(litres, 0, room);
    value.fuel = int16_t(value.fuel + (taken * minutes_per_litre));
    return taken;
}

bool powered(int tile_x, int tile_y, int now_minute)
{
    for(generator& value : generators)
    {
        if(value.used() && bn::abs(value.x - tile_x) <= reach_tiles && bn::abs(value.y - tile_y) <= reach_tiles)
        {
            static_cast<void>(settle(value, now_minute));

            if(value.on())
            {
                return true;
            }
        }
    }

    return false;
}

void update(game& owner, int frame)
{
    int player_x = owner.hero().position().x().integer() >> world::tile_shift;
    int player_y = owner.hero().position().y().integer() >> world::tile_shift;
    int now = owner.world_minute();

    for(generator& value : generators)
    {
        if(! value.used() || bn::abs(value.x - player_x) > update_tiles || bn::abs(value.y - player_y) > update_tiles)
        {
            continue;
        }

        // Worked out once a second (fridges are credited a whole day at a time, so the loop over the containers
        // is rare).
        if(frame % 60 == 0)
        {
            static_cast<void>(settle(value, now));
            credit_fridges(owner, value);
        }

        if(value.on() && frame % noise_frames == 0)
        {
            bn::fixed_point centre((value.x * world::tile_size) + (world::tile_size / 2),
                                   (value.y * world::tile_size) + (world::tile_size / 2));
            owner.noise(centre, noise_tiles);
        }
    }
}

void save(save::writer& out)
{
    out.put(generators);
}

void load(save::reader& in)
{
    in.get(generators);
}

}

}
