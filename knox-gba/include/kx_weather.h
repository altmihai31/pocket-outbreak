#ifndef KX_WEATHER_H
#define KX_WEATHER_H

#include "bn_fixed.h"
#include "bn_optional.h"
#include "bn_sprite_ptr.h"
#include "bn_vector.h"

namespace kx
{

class game;

// Weather and seasons (spec: World timeline and events): rain, fog and storms derived from hash(seed, day), so past
// and future weather never need storing. Rain soaks you, halves noise radii and fills rain collectors; fog cuts
// sight; storms are rain with lightning. Temperature falls from about day 60 and winter sets in about day 150.
enum class weather_kind : uint8_t
{
    CLEAR,
    RAIN,
    FOG,
    STORM
};

namespace weather_tuning
{
    // Chances (percent) of a day having rain, fog or a storm, by season: summer, autumn, winter.
    constexpr int rain_percent[3] = { 22, 32, 30 };
    constexpr int fog_percent[3] = { 10, 20, 18 };
    constexpr int storm_percent[3] = { 8, 10, 4 };
    constexpr int min_hours = 2;                    // how long it lasts
    constexpr int max_hours = 9;
    constexpr int fog_first_hour = 3;               // fog comes in the early morning
    constexpr int fog_last_hour = 8;

    constexpr int rain_wetness = 2;                 // wetness per minute outdoors in rain (of 100)
    constexpr int raincoat_percent = 25;            // a raincoat lets this much through
    constexpr int noise_percent = 50;               // spec: rain halves all noise radii
    constexpr int fog_sight_percent = 50;           // fog cuts sight
    constexpr int collector_fill = 3;               // drinks a rain collector gains per hour of rain

    constexpr int autumn_day = 60;                  // spec: temperature falls from day ~60
    constexpr int winter_day = 150;                 // spec: winter from day ~150
    constexpr int autumn_full_day = 110;            // autumn colours complete
    constexpr int winter_full_day = 165;            // snow complete
    constexpr int winter_temp = -16;                // degrees off the day and night temperatures

    constexpr int rain_drops = 20;                  // sprites
    constexpr int haze_percent = 35;                // fog: the world layers faded this much to white
    constexpr int storm_flash_min = 240;            // frames between lightning flashes
    constexpr int storm_flash_extra = 600;
    constexpr int flash_frames = 3;
    constexpr int thunder_delay_max = 60;           // frames from a flash to its thunder

    // Milestone 14 sound pass: rain on the roof indoors (the sound is 1.2 s long, played again each time), and
    // gusts of wind outdoors in the wild and in storms, now and then.
    constexpr int roof_rain_frames = 70;
    constexpr int wind_min_frames = 8 * 60;
    constexpr int wind_extra_frames = 14 * 60;
}

namespace weather
{
    // The weather at a moment (in-game minute since the start).
    [[nodiscard]] weather_kind at(int world_minute);

    [[nodiscard]] inline bool wet(weather_kind value)
    {
        return value == weather_kind::RAIN || value == weather_kind::STORM;
    }

    // Rain (or a storm) at some time on a day (1-based), for crops worked out lazily.
    [[nodiscard]] bool rain_day(int day);

    // Degrees the season takes off the temperature on a day (0 in summer).
    [[nodiscard]] int season_temp(int day);

    // How far into autumn's colours and winter's snow a day is (0-100).
    [[nodiscard]] int autumn_percent(int day);
    [[nodiscard]] int winter_percent(int day);
}

// What weather looks like on screen: rain streaks (sprites), lightning flashes with thunder.
class weather_effects
{

public:
    // Once per frame. Rain is not drawn indoors.
    void update(game& owner, weather_kind value, bool inside);

    // A lightning flash is lighting everything up.
    [[nodiscard]] bool flashing() const
    {
        return _flash > 0;
    }

    void hide();

private:
    bn::vector<bn::sprite_ptr, weather_tuning::rain_drops> _drops;
    bool _shaded_dark = false;      // the night shading the drops have (kx_light)
    int _frame = 0;
    int _next_flash = 0;
    int _flash = 0;
    int _thunder = -1;
    int _roof_rain = 0;             // frames to the next patter on the roof
    int _wind = 0;                  // frames to the next gust
};

}

#endif
