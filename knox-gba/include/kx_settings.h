#ifndef KX_SETTINGS_H
#define KX_SETTINGS_H

#include "bn_fixed.h"

#include "kx_bench.h"

// The game's own settings (playtest 2, mockup 18): music and sound effect volumes, screen shake, the CPU meter. Not
// the sandbox settings, which belong to a world. Changed in the Start menu's SETTINGS tab or on the title screen,
// they apply at once and go into the save's slot header (its reserved bytes, outside the header CRC: saves from
// before them read as the defaults, and no format change was needed).
namespace kx::settings
{

enum class setting : uint8_t
{
    MUSIC,
    SOUND,
    SHAKE,
    CPU,
    COUNT
};

constexpr int count = int(setting::COUNT);

// Volumes take 4 levels (OFF, LOW, MEDIUM, FULL: these percentages of today's levels), switches 2 (OFF, ON).
constexpr int volume_levels = 4;
constexpr int volume_percent[volume_levels] = { 0, 33, 66, 100 };

[[nodiscard]] constexpr int defaults(setting which)
{
    return which == setting::CPU ? KX_DEBUG : which == setting::SHAKE ? 1 : volume_levels - 1;
}

// How many values a setting takes, and its value now.
[[nodiscard]] int value_count(setting which);
[[nodiscard]] int value(setting which);

// Steps a setting by -1 or +1 (stopping at the ends) and applies it; false if it was already at that end.
bool step(setting which, int delta);

// Name, value text and up to four help lines of 12 characters (the tab's right panel).
[[nodiscard]] const char* name(setting which);
[[nodiscard]] const char* value_text(setting which, int value);
[[nodiscard]] const char* const* help(setting which);
constexpr int help_lines = 4;

// What the rest of the game reads.
[[nodiscard]] bn::fixed music_scale();
[[nodiscard]] bn::fixed sound_scale();
[[nodiscard]] bool shake();
[[nodiscard]] bool cpu_meter();

// The save's bytes: a marker, then each value. Unpacking bytes without the marker (an older save) keeps the
// defaults; values out of range are clamped.
constexpr int save_bytes = 1 + count;
void pack(uint8_t* output);
void unpack(const uint8_t* input);

}

#endif
