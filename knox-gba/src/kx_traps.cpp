#include "kx_traps.h"

#include "kx_hash.h"
#include "kx_save.h"
#include "kx_world.h"

namespace kx
{

using namespace trap_tuning;

namespace
{
    // A trap: 8 bytes. bits 0 used, 1 caught, 2 a snare (milestone 13).
    struct trap
    {
        int16_t x;
        int16_t y;
        uint16_t set_day;           // nights after this day are rolled
        uint8_t bits;
        uint8_t unused;

        [[nodiscard]] bool used() const
        {
            return bits & 1;
        }
    };

    static_assert(sizeof(trap) == 8);

    BN_DATA_EWRAM_BSS trap trap_list[max_traps];
}

namespace traps
{

void reset()
{
    for(trap& value : trap_list)
    {
        value = trap();
    }
}

int count()
{
    int result = 0;

    for(const trap& value : trap_list)
    {
        result += value.used();
    }

    return result;
}

bool add(int tile_x, int tile_y, int today, bool snare)
{
    for(trap& value : trap_list)
    {
        if(! value.used())
        {
            value = trap();
            value.x = int16_t(tile_x);
            value.y = int16_t(tile_y);
            value.set_day = uint16_t(today);
            value.bits = uint8_t(snare ? 1 | 4 : 1);
            return true;
        }
    }

    return false;
}

int at(int tile_x, int tile_y)
{
    for(int index = 0; index < max_traps; ++index)
    {
        const trap& value = trap_list[index];

        if(value.used() && value.x == tile_x && value.y == tile_y)
        {
            return index;
        }
    }

    return -1;
}

bool caught(int index, int today, int foraging)
{
    trap& value = trap_list[index];

    if(value.bits & 2)
    {
        return true;
    }

    int chance = catch_percent + (foraging * ((value.bits & 4) ? snare_percent_per_level : percent_per_level));

    for(int day = value.set_day + 1; day <= today; ++day)
    {
        uint32_t roll = hash_xy(world::seed() ^ (uint32_t(day) * 0x2545f491u) ^ 0x54524150u, value.x, value.y);

        if(int(roll % 100) < chance)
        {
            value.bits = uint8_t(value.bits | 2);
            return true;
        }
    }

    // Nothing yet: the nights rolled so far stay rolled.
    value.set_day = uint16_t(today);
    return false;
}

void reset_trap(int index, int today)
{
    trap& value = trap_list[index];
    value.bits = uint8_t(value.bits & 4) | 1;
    value.set_day = uint16_t(today);
}

bool snare(int index)
{
    return trap_list[index].bits & 4;
}

bool caught_bird(int index)
{
    const trap& value = trap_list[index];
    return ! (value.bits & 4) &&
           (hash_xy(world::seed() ^ (uint32_t(value.set_day) * 0x9e3779b9u) ^ 0x42495244u, value.x, value.y) & 1);
}

void save(save::writer& out)
{
    out.put(trap_list);
}

void load(save::reader& in)
{
    in.get(trap_list);
}

}

}
