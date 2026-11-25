#include "kx_sandbox.h"

namespace kx::sandbox
{

namespace
{
    settings chosen;

    constexpr const char* names[setting_count] = { "ZOMBIE SPEED", "ZOMBIES", "LOOT", "SHUTOFF", "DAY LENGTH" };
    constexpr int counts[setting_count] = { 3, 3, 3, 4, 3 };

    constexpr const char* values[setting_count][4] = {
        { "SHAMBLERS", "NORMAL", "FAST", nullptr },
        { "FEW", "NORMAL", "MANY", nullptr },
        { "RARE", "NORMAL", "PLENTY", nullptr },
        { "WEEK 1", "NORMAL", "MONTH 2", "NEVER" },
        { "30 MIN", "60 MIN", "90 MIN", nullptr },
    };

    constexpr const char* details[setting_count][4] = {
        { "NO SPRINTERS, FEWER FAST ONES", "SOME FAST ONES, A FEW SPRINTERS", "MANY FAST ONES, 1 IN 10 SPRINTS",
          nullptr },
        { "HALF AS MANY ZOMBIES", "THE USUAL NUMBERS", "HALF AS MANY AGAIN", nullptr },
        { "HALF THE LOOT IN EVERY CONTAINER", "THE USUAL AMOUNTS", "HALF AS MUCH AGAIN", nullptr },
        { "WATER AND POWER GO IN WEEK 1", "WATER BY DAY 10-20, POWER 14-30", "THEY LAST INTO THE SECOND MONTH",
          "WATER AND POWER NEVER GO" },
        { "A DAY LASTS 30 REAL MINUTES", "A DAY LASTS AN HOUR", "A DAY LASTS 90 MINUTES", nullptr },
    };
}

const settings& current()
{
    return chosen;
}

void set(const settings& value)
{
    chosen = value;

    for(int index = 0; index < setting_count; ++index)
    {
        if(chosen.values[index] >= counts[index])
        {
            chosen.values[index] = 1;
        }
    }
}

void reset()
{
    chosen = settings();
}

int value_count(setting which)
{
    return counts[int(which)];
}

const char* name(setting which)
{
    return names[int(which)];
}

const char* value_name(setting which, int index)
{
    return values[int(which)][index];
}

const char* detail(setting which, int index)
{
    return details[int(which)][index];
}

}
