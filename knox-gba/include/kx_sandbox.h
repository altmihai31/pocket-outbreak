#ifndef KX_SANDBOX_H
#define KX_SANDBOX_H

#include <cstdint>

namespace kx::sandbox
{

// Sandbox settings (milestone 13; spec: world creation, zombie speed and loot rarity, plus the rest): chosen on the
// new world screen, kept in the save header, shown read-only on the MENU's WORLD page (playtest 4; it was the Map
// tab). Index 1 of each is the game as it was.
enum class setting : uint8_t
{
    ZOMBIE_SPEED,       // shamblers, normal, fast
    ZOMBIES,            // few, normal, many
    LOOT,               // rare, normal, plenty
    SHUTOFF,            // week 1, normal (days 10-30), month 2, never
    DAY_LENGTH,         // 30, 60, 90 real minutes
    COUNT
};

constexpr int setting_count = int(setting::COUNT);

struct settings
{
    uint8_t values[setting_count] = { 1, 1, 1, 1, 1 };
};

// What each value does.
namespace tuning
{
    // Zombie speed: % of new zombies that are sprinters and fast shamblers (normal: the zombie tuning table's).
    constexpr int sprinter_percent[3] = { 0, -1, 10 };
    constexpr int fast_percent[3] = { 10, -1, 30 };

    constexpr int population_percent[3] = { 50, 100, 150 };
    constexpr int loot_percent[3] = { 50, 100, 150 };

    // Shutoff: the days the water and the power go (first, last; rolled per world); normal is the timeline's own
    // range (kx_timeline.h), "never" is past any game.
    constexpr int water_off[4][2] = { { 4, 6 }, { -1, -1 }, { 40, 50 }, { 30000, 30000 } };
    constexpr int power_off[4][2] = { { 6, 8 }, { -1, -1 }, { 50, 62 }, { 30000, 30000 } };

    constexpr int day_minutes[3] = { 30, 60, 90 };
}

[[nodiscard]] const settings& current();
void set(const settings& value);
void reset();

[[nodiscard]] inline int value(setting which)
{
    return current().values[int(which)];
}

// How many values a setting has, its name and a value's name (for the screens).
[[nodiscard]] int value_count(setting which);
[[nodiscard]] const char* name(setting which);
[[nodiscard]] const char* value_name(setting which, int index);

// A line about the focused value on the new world screen.
[[nodiscard]] const char* detail(setting which, int index);

}

#endif
