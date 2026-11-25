#include "kx_farm.h"

#include "bn_math.h"

#include "kx_game.h"
#include "kx_save.h"
#include "kx_weather.h"
#include "kx_world.h"

namespace kx
{

using namespace farm_tuning;

namespace
{
    // A plot: 10 bytes. bits 0-1 crop, 2 used, 3 rotten, 4 the crop's third bit (milestone 14: tomatoes and onions;
    // older saves have it clear); growth counts growing days (and, once ripe, days left unharvested). Days are
    // 1-based in-game days.
    struct plot
    {
        int16_t x;
        int16_t y;
        uint16_t updated;           // the day it was last worked out to
        uint16_t watered;           // the last day it was watered or rained on
        uint8_t bits;
        uint8_t growth;

        [[nodiscard]] bool used() const
        {
            return bits & 4;
        }

        [[nodiscard]] crop planted() const
        {
            return crop((bits & 3) | ((bits >> 2) & 4));
        }

        [[nodiscard]] bool rotten() const
        {
            return bits & 8;
        }
    };

    static_assert(sizeof(plot) == 10);

    BN_DATA_EWRAM_BSS plot plots[max_plots];

    // Replays the days since the plot was last worked out: rain waters it, winter holds it, dry soil or a ripe crop
    // left too long rots it.
    void settle(plot& value, int today)
    {
        crop planted = value.planted();

        if(planted == crop::NONE || value.rotten() || today <= value.updated)
        {
            value.updated = uint16_t(bn::max(int(value.updated), today));
            return;
        }

        int days = info(planted).days;

        for(int day = value.updated + 1; day <= today; ++day)
        {
            if(day >= weather_tuning::winter_day)
            {
                continue;
            }

            if(weather::rain_day(day))
            {
                value.watered = uint16_t(day);
            }

            int dry = day - value.watered;

            if(value.growth >= days)
            {
                // Ripe: it keeps a while, then rots in the ground.
                if(value.growth >= days + ripe_keep_days)
                {
                    value.bits = uint8_t(value.bits | 8);
                    break;
                }

                ++value.growth;
            }
            else if(dry >= rot_dry_days)
            {
                value.bits = uint8_t(value.bits | 8);
                break;
            }
            else if(dry < wet_days)
            {
                ++value.growth;
            }
        }

        value.updated = uint16_t(today);
    }

    [[nodiscard]] plot_stage stage_of(const plot& value)
    {
        crop planted = value.planted();

        if(value.rotten())
        {
            return plot_stage::ROTTEN;
        }

        if(planted == crop::NONE)
        {
            return plot_stage::EMPTY;
        }

        int days = info(planted).days;

        if(value.growth >= days)
        {
            return plot_stage::RIPE;
        }

        if(value.growth == 0)
        {
            return plot_stage::SEEDED;
        }

        return value.growth * 2 < days ? plot_stage::SPROUT : plot_stage::GROWING;
    }
}

namespace farm
{

void reset()
{
    for(plot& value : plots)
    {
        value = plot();
    }
}

int count()
{
    int result = 0;

    for(const plot& value : plots)
    {
        result += value.used();
    }

    return result;
}

bool add(int tile_x, int tile_y, int today)
{
    for(plot& value : plots)
    {
        if(! value.used())
        {
            value = plot();
            value.x = int16_t(tile_x);
            value.y = int16_t(tile_y);
            value.updated = uint16_t(today);
            value.bits = 4;
            return true;
        }
    }

    return false;
}

int at(int tile_x, int tile_y)
{
    for(int index = 0; index < max_plots; ++index)
    {
        const plot& value = plots[index];

        if(value.used() && value.x == tile_x && value.y == tile_y)
        {
            return index;
        }
    }

    return -1;
}

plot_stage stage(int index, int today)
{
    plot& value = plots[index];
    settle(value, today);
    return stage_of(value);
}

crop crop_of(int index)
{
    return plots[index].planted();
}

bool wet(int index, int today)
{
    plot& value = plots[index];
    settle(value, today);
    return value.watered >= today || weather::rain_day(today);
}

void plant(int index, crop value, int today)
{
    plot& target = plots[index];
    target.bits = uint8_t(4 | (int(value) & 3) | ((int(value) & 4) << 2));
    target.growth = 0;
    target.updated = uint16_t(today);

    // Not watered yet: dry from the day it went in.
    target.watered = uint16_t(bn::max(today - wet_days, 0));
}

void water(int index, int today)
{
    plot& value = plots[index];
    settle(value, today);
    value.watered = uint16_t(today);
}

void clear(int index, int today)
{
    plot& value = plots[index];
    value.bits = 4;
    value.growth = 0;
    value.updated = uint16_t(today);
}

world_gfx gfx(plot_stage value)
{
    switch(value)
    {

    case plot_stage::SPROUT:
    case plot_stage::GROWING:
        return world_gfx::PLOT_GROWING;

    case plot_stage::RIPE:
        return world_gfx::PLOT_RIPE;

    default:
        return world_gfx::PLOT_SOIL;
    }
}

void update(game& owner)
{
    int player_x = owner.hero().position().x().integer() >> world::tile_shift;
    int player_y = owner.hero().position().y().integer() >> world::tile_shift;
    int today = owner.day();

    for(plot& value : plots)
    {
        if(! value.used() || bn::abs(value.x - player_x) > update_tiles || bn::abs(value.y - player_y) > update_tiles)
        {
            continue;
        }

        settle(value, today);
        world_gfx wanted = gfx(stage_of(value));

        if(world::loaded(value.x, value.y) && world::wall_gfx(value.x, value.y) != wanted)
        {
            static_cast<void>(world::set_wall_gfx(value.x, value.y, wanted));
            owner.refresh_tile(value.x, value.y);
        }
    }
}

void save(save::writer& out)
{
    out.put(plots);
}

void load(save::reader& in)
{
    in.get(plots);
}

}

}
