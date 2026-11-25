#include "kx_settings.h"

#include "bn_math.h"

#include "kx_music.h"

namespace kx::settings
{

namespace
{
    constexpr uint8_t save_marker = 0x53;       // 'S': the slot header's reserved bytes hold settings

    uint8_t values[count] = { uint8_t(defaults(setting::MUSIC)), uint8_t(defaults(setting::SOUND)),
                              uint8_t(defaults(setting::SHAKE)), uint8_t(defaults(setting::CPU)) };

    constexpr const char* names[count] = { "MUSIC", "SOUND EFFECTS", "SCREEN SHAKE", "CPU METER" };
    constexpr const char* volume_names[volume_levels] = { "OFF", "LOW", "MEDIUM", "FULL" };
    constexpr const char* switch_names[2] = { "OFF", "ON" };

    constexpr const char* helps[count][help_lines] = {
        { "THE TITLE,", "CALM, NIGHT", "AND CHASE", "TUNES." },
        { "HITS, STEPS,", "ZOMBIES,", "DOORS: ALL", "THE REST." },
        { "JOLTS ON", "HEAVY HITS", "AND BLASTS.", nullptr },
        { "PERFORMANCE", "READOUT BY", "THE CLOCK.", nullptr },
    };
}

int value_count(setting which)
{
    return which == setting::MUSIC || which == setting::SOUND ? volume_levels : 2;
}

int value(setting which)
{
    return values[int(which)];
}

bool step(setting which, int delta)
{
    int now = values[int(which)];
    int next = bn::clamp(now + delta, 0, value_count(which) - 1);

    if(next == now)
    {
        return false;
    }

    values[int(which)] = uint8_t(next);

    if(which == setting::MUSIC)
    {
        music::apply_volume();
    }

    return true;
}

const char* name(setting which)
{
    return names[int(which)];
}

const char* value_text(setting which, int value)
{
    return value_count(which) == volume_levels ? volume_names[value] : switch_names[value];
}

const char* const* help(setting which)
{
    return helps[int(which)];
}

bn::fixed music_scale()
{
    return bn::fixed(volume_percent[values[int(setting::MUSIC)]]) / 100;
}

bn::fixed sound_scale()
{
    return bn::fixed(volume_percent[values[int(setting::SOUND)]]) / 100;
}

bool shake()
{
    return values[int(setting::SHAKE)];
}

bool cpu_meter()
{
    return values[int(setting::CPU)];
}

void pack(uint8_t* output)
{
    output[0] = save_marker;

    for(int index = 0; index < count; ++index)
    {
        output[1 + index] = values[index];
    }
}

void unpack(const uint8_t* input)
{
    if(input[0] != save_marker)
    {
        return;
    }

    for(int index = 0; index < count; ++index)
    {
        values[index] = uint8_t(bn::min(int(input[1 + index]), value_count(setting(index)) - 1));
    }

    music::apply_volume();
}

}
