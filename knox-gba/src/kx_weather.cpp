#include "kx_weather.h"
#include "kx_light.h"

#include "bn_bg_palettes.h"
#include "bn_display.h"
#include "bn_sprite_items_fx.h"
#include "bn_sprite_palettes.h"

#include "kx_game.h"
#include "kx_hash.h"
#include "kx_macro_map.h"
#include "kx_sfx.h"
#include "kx_world.h"

namespace kx
{

using namespace weather_tuning;

namespace
{
    constexpr int rain_frame = 19;              // fx frame (after the build cursor corners)
    constexpr int drop_bg_priority = 1;

    [[nodiscard]] int season_of(int day)
    {
        return day >= winter_day ? 2 : day >= autumn_day ? 1 : 0;
    }
}

namespace weather
{

weather_kind at(int world_minute)
{
    // The hand-made test map (benches) stays dry, so their numbers stay comparable.
    if(world::test_world())
    {
        return weather_kind::CLEAR;
    }

    int day = (world_minute / (24 * 60)) + 1;
    int hour = (world_minute / 60) % 24;
    uint32_t roll = hash32(world::seed() ^ (uint32_t(day) * 0x9e3779b9u) ^ 0x57544852u);
    int season = season_of(day);
    int pick = int(roll % 100);
    weather_kind kind = weather_kind::CLEAR;

    if(pick < storm_percent[season])
    {
        kind = weather_kind::STORM;
    }
    else if(pick < storm_percent[season] + rain_percent[season])
    {
        kind = weather_kind::RAIN;
    }
    else if(pick < storm_percent[season] + rain_percent[season] + fog_percent[season])
    {
        kind = weather_kind::FOG;
    }
    else
    {
        return weather_kind::CLEAR;
    }

    // When it starts and how long it lasts that day.
    int length = min_hours + int((roll >> 8) % uint32_t(max_hours - min_hours + 1));
    int start = kind == weather_kind::FOG ? fog_first_hour + int((roll >> 16) % uint32_t(fog_last_hour - fog_first_hour + 1)) :
                int((roll >> 16) % 24);
    int into = (hour - start + 24) % 24;
    return into < length ? kind : weather_kind::CLEAR;
}

bool rain_day(int day)
{
    if(world::test_world())
    {
        return false;
    }

    // The same roll as at(): rain or a storm some time that day.
    uint32_t roll = hash32(world::seed() ^ (uint32_t(day) * 0x9e3779b9u) ^ 0x57544852u);
    int season = season_of(day);
    return int(roll % 100) < storm_percent[season] + rain_percent[season];
}

int season_temp(int day)
{
    if(day <= autumn_day)
    {
        return 0;
    }

    if(day >= winter_day)
    {
        return winter_temp;
    }

    return (winter_temp * (day - autumn_day)) / (winter_day - autumn_day);
}

int autumn_percent(int day)
{
    return bn::clamp(((day - autumn_day) * 100) / (autumn_full_day - autumn_day), 0, 100);
}

int winter_percent(int day)
{
    return bn::clamp(((day - winter_day) * 100) / (winter_full_day - winter_day), 0, 100);
}

}

void weather_effects::update(game& owner, weather_kind value, bool inside)
{
    ++_frame;

    // Rain: streaks falling across the screen (screen space, so they don't move with the camera).
    bool raining = weather::wet(value) && ! inside;

    if(raining && _drops.empty())
    {
        for(int index = 0; index < rain_drops; ++index)
        {
            int x = int(hash32(uint32_t(index) * 7919u) % uint32_t(bn::display::width())) - (bn::display::width() / 2);
            int y = int(hash32(uint32_t(index) * 104729u) % uint32_t(bn::display::height())) - (bn::display::height() / 2);
            bn::sprite_ptr drop = bn::sprite_items::fx.create_sprite(x, y, rain_frame);
            drop.set_bg_priority(drop_bg_priority);
            light::shade(drop);
            _drops.push_back(bn::move(drop));
        }
    }
    else if(! raining && ! _drops.empty())
    {
        _drops.clear();
    }

    // Night: new drops are shaded when they appear, the others when the night begins or ends.
    bool reshade = light::sprites_dark() != _shaded_dark;
    _shaded_dark = light::sprites_dark();

    for(int index = 0; index < _drops.size(); ++index)
    {
        bn::sprite_ptr& drop = _drops[index];
        int speed = 5 + (index & 1);
        int x = drop.x().integer() - 2;
        int y = drop.y().integer() + speed;

        if(y > (bn::display::height() / 2) + 8)
        {
            y -= bn::display::height() + 16;
            x = int(hash32(uint32_t(_frame + index) * 2654435761u) % uint32_t(bn::display::width() + 32)) -
                (bn::display::width() / 2);
        }

        if(x < -(bn::display::width() / 2) - 8)
        {
            x += bn::display::width() + 16;
        }

        drop.set_position(x, y);

        if(reshade)
        {
            light::shade(drop);
        }
    }

    // Storms: lightning lights everything up for a moment; the thunder follows.
    if(value == weather_kind::STORM)
    {
        if(_next_flash <= 0)
        {
            _next_flash = storm_flash_min + owner.random().get_int(storm_flash_extra);
            _flash = flash_frames;
            _thunder = owner.random().get_int(thunder_delay_max);
            bn::bg_palettes::set_brightness(bn::fixed(1) / 2);
            bn::sprite_palettes::set_brightness(bn::fixed(1) / 2);
        }
        else
        {
            --_next_flash;
        }
    }

    if(_flash > 0 && --_flash == 0)
    {
        bn::bg_palettes::set_brightness(0);
        bn::sprite_palettes::set_brightness(0);
    }

    if(_thunder >= 0 && --_thunder < 0)
    {
        sfx::play(sfx::id::THUNDER, inside ? bn::fixed(1) / 2 : bn::fixed(1), owner.random().get_int(160) - 80);
    }

    // Milestone 14 sound pass: rain drums on the roof while you are indoors.
    if(weather::wet(value) && inside)
    {
        if(--_roof_rain <= 0)
        {
            _roof_rain = roof_rain_frames;
            sfx::play(sfx::id::RAIN_ROOF, value == weather_kind::STORM ? bn::fixed(1) / 2 : bn::fixed(1) / 3);
        }
    }
    else
    {
        _roof_rain = 0;
    }

    // Wind gusts outdoors in the wild (and anywhere in a storm). Their timing comes from the frame count, so the game's
    // random numbers (and the benches) don't move.
    if(--_wind <= 0)
    {
        uint32_t roll = hash32(uint32_t(_frame) * 0x9e3779b9u ^ 0x57494e44u);
        _wind = wind_min_frames + int(roll % uint32_t(wind_extra_frames));
        bool storm = value == weather_kind::STORM;
        bool wild = false;

        if(! world::test_world())
        {
            int tile_x = owner.hero().position().x().integer() >> world::tile_shift;
            int tile_y = owner.hero().position().y().integer() >> world::tile_shift;
            wild = macro::zone_at(tile_x >> macro::cell_shift, tile_y >> macro::cell_shift) == macro::zone::WILDERNESS;
        }

        if(! inside && (wild || storm))
        {
            sfx::play(sfx::id::WIND, storm ? bn::fixed(3) / 5 : bn::fixed(7) / 20, int((roll >> 16) % 161) - 80);
        }
    }
}

void weather_effects::hide()
{
    _drops.clear();

    if(_flash > 0)
    {
        _flash = 0;
        bn::bg_palettes::set_brightness(0);
        bn::sprite_palettes::set_brightness(0);
    }
}

}
