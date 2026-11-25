#ifndef KX_COMBAT_TUNING_H
#define KX_COMBAT_TUNING_H

#include "kx_tuning.h"

namespace kx
{

// ---------------------------------------------------------------------------------------------------------------------
// Weapons (spec: Combat > Melee frame data). One table per weapon so the debug menu can edit each one.
// Damage is in zombie health points (see zombie tiers below); reach and knockback are px and px/frame.
// ---------------------------------------------------------------------------------------------------------------------

enum class weapon : uint8_t
{
    FISTS,
    KNIFE,
    BAT,
    AXE,
    SPEAR,
    COUNT
};

enum class weapon_param : int
{
    WINDUP,
    ACTIVE,
    RECOVERY,
    DAMAGE,
    HITSTOP,
    TARGETS,
    REACH,
    ARC,
    KNOCKBACK,
    SHAKE,
    NOISE,
    COUNT
};

#define KX_WEAPON_TABLE(windup, active, recovery, damage, hitstop, targets, reach, arc, knockback, shake, noise) \
    { \
        tune_int("Wind-up frames", windup, 1, 60), \
        tune_int("Active frames", active, 1, 30), \
        tune_int("Recovery frames", recovery, 1, 90), \
        tune_int("Damage", damage, 0, 20), \
        tune_int("Hitstop frames", hitstop, 0, 20), \
        tune_int("Targets per swing", targets, 1, 5), \
        tune_int("Reach px", reach, 4, 48), \
        tune_int("Arc degrees", arc, 10, 360, 5), \
        tune_milli("Knockback", knockback, 0, 8000, 250), \
        tune_int("Shake px", shake, 0, 4), \
        tune_int("Hit noise tiles", noise, 0, 40), \
    }

// Weapon classes; each melee item (kx_items.h) names its class and carries its own durability.
// Fists: 5/3/12, very low damage, hitstop 2, 1 target; last resort.
constexpr tuning_entry fists_tuning_defaults[] = KX_WEAPON_TABLE(5, 3, 12, 1, 2, 1, 14, 70, 1000, 1, 2);
// Knife (Blade): 4/3/10, low damage, hitstop 3, 1 target; quiet, stealth kills.
constexpr tuning_entry knife_tuning_defaults[] = KX_WEAPON_TABLE(4, 3, 10, 2, 3, 1, 17, 70, 750, 1, 1);
// Bat (Blunt): 8/4/16, medium damage, hitstop 4, 2 targets; widest arc, strong knockback.
constexpr tuning_entry bat_tuning_defaults[] = KX_WEAPON_TABLE(8, 4, 16, 3, 4, 2, 22, 150, 3000, 2, 4);
// Axe: 12/4/22, high damage, hitstop 6, 2 targets; a charged hit kills outright.
constexpr tuning_entry axe_tuning_defaults[] = KX_WEAPON_TABLE(12, 4, 22, 5, 6, 2, 21, 110, 2000, 3, 4);
// Spear: 9/5/18, medium damage, 1 target; the longest reach in a narrow arc (milestone 12.6).
constexpr tuning_entry spear_tuning_defaults[] = KX_WEAPON_TABLE(9, 5, 18, 3, 4, 1, 28, 40, 1500, 1, 3);

#undef KX_WEAPON_TABLE

[[nodiscard]] tuning_set& weapon_tuning(weapon value);

[[nodiscard]] inline bn::fixed tuning(weapon value, weapon_param param)
{
    return weapon_tuning(value).get(int(param));
}

[[nodiscard]] constexpr bool is_blade(weapon value)
{
    return value == weapon::KNIFE;
}

// ---------------------------------------------------------------------------------------------------------------------
// Player combat actions (spec: Combat): charge, shove, stomp, grabs and game feel.
// ---------------------------------------------------------------------------------------------------------------------

enum class combat_param : int
{
    CHARGE_FRAMES,
    CHARGE_DAMAGE,
    CHARGE_KNOCKBACK,
    SHOVE_FRAMES,
    SHOVE_COOLDOWN,
    SHOVE_REACH,
    SHOVE_ARC,
    SHOVE_KNOCKBACK,
    SHOVE_STAGGER,
    SHOVE_HITSTOP,
    SHOVE_NOISE,
    TAP_FRAMES,
    STOMP_FRAMES,
    STOMP_REACH,
    STOMP_HITSTOP,
    STOMP_NOISE,
    GRAB_FRAMES,
    GRAB_PRESS,
    GRAB_DECAY,
    GRAB_EXTRA,
    WOUND_SCRATCH,
    WOUND_LACERATION,
    WOUND_BITE,
    KNOCKDOWN,
    FLASH_FRAMES,
    COUNT
};

constexpr tuning_entry combat_tuning_defaults[] = {
    tune_int("Full charge frames", 30, 1, 120),
    tune_milli("Charged damage x", 2000, 1000, 5000, 250),
    tune_milli("Charged knockback x", 1500, 1000, 5000, 250),
    tune_int("Shove frames", 14, 1, 60),
    tune_int("Shove cooldown", 36, 0, 180),             // "short cooldown"
    tune_int("Shove reach px", 18, 4, 48),
    tune_int("Shove arc degrees", 140, 10, 360, 5),
    tune_milli("Shove knockback", 2500, 0, 8000, 250),
    tune_int("Shove stagger frames", 30, 0, 120),       // spec: 30-frame stagger
    tune_int("Shove hitstop", 2, 0, 20),
    tune_int("Shove noise tiles", 4, 0, 40),            // spec: shove 4
    tune_int("B tap frames", 8, 1, 30),                 // B released sooner is a shove; held longer is sprint
    tune_int("Stomp frames", 18, 4, 60),
    tune_int("Stomp reach px", 18, 4, 32),
    tune_int("Stomp hitstop", 5, 0, 20),
    tune_int("Stomp noise tiles", 3, 0, 40),
    tune_int("Grab frames to bite", 120, 10, 600, 5),
    tune_int("Grab meter per A", 18, 1, 100),            // meter is 0-100
    tune_int("Grab meter decay", 1, 0, 20),
    tune_int("Grab extra zombie %", 60, 0, 300, 5),      // each extra grabber adds this much to the meter needed
    tune_int("Wound scratch weight", 60, 0, 100),
    tune_int("Wound lacer. weight", 30, 0, 100),
    tune_int("Wound bite weight", 10, 0, 100),
    tune_milli("Knockdown impulse", 3000, 500, 10000, 250), // hits at or above this knock zombies down (bat)
    tune_int("White flash frames", 2, 0, 10),            // spec: 2-frame white flash
};

static_assert(sizeof(combat_tuning_defaults) / sizeof(tuning_entry) == int(combat_param::COUNT));

[[nodiscard]] tuning_set& combat_tuning();

[[nodiscard]] inline bn::fixed tuning(combat_param param)
{
    return combat_tuning().get(int(param));
}

// ---------------------------------------------------------------------------------------------------------------------
// Zombies (spec: Zombies). Speeds in px/frame, distances in px, times in frames.
// ---------------------------------------------------------------------------------------------------------------------

enum class zombie_param : int
{
    SHAMBLER_SPEED,
    FAST_SPEED,
    SPRINTER_SPEED,
    FAST_PERCENT,
    SPRINTER_PERCENT,
    CRAWL_SPEED,
    CRAWL_GRAB_DELAY,
    HP_TIER_1,
    HP_TIER_2,
    HP_TIER_3,
    SIGHT_PX,
    SIGHT_CONE,
    CHASE_SIGHT_PX,
    NIGHT_SIGHT_PX,
    NIGHT_CHASE_SIGHT_PX,
    HIDE_PX,
    SEARCH_FRAMES,
    SEARCH_MAX_FRAMES,
    LUNGE_RANGE,
    WINDUP_FRAMES,
    LUNGE_SPEED,
    LUNGE_FRAMES,
    GRAB_RANGE,
    RECOVER_FRAMES,
    HIT_STAGGER,
    KNOCKBACK_FRICTION,
    DOWN_FRAMES,
    DECISION_INTERVAL,
    SEPARATION_PX,
    GROAN_CHANCE,
    FLOW_POPS,
    TRACK_TILES,
    COUNT
};

constexpr tuning_entry zombie_tuning_defaults[] = {
    tune_milli("Shambler speed", 350, 50, 3000, 25),
    tune_milli("Fast shambler speed", 650, 50, 3000, 25),
    tune_milli("Sprinter speed", 1750, 50, 4000, 25),
    tune_int("Fast shambler %", 18, 0, 100),            // spec: ~80 / 18 / 2
    tune_int("Sprinter %", 2, 0, 100),
    tune_milli("Crawler speed", 200, 25, 2000, 25),
    tune_int("Crawler grab delay", 20, 0, 120),          // frames next to the player before it takes hold
    tune_int("HP tier 1", 2, 1, 30),                     // hidden 1-3 toughness tier
    tune_int("HP tier 2", 5, 1, 30),
    tune_int("HP tier 3", 9, 1, 30),
    tune_int("Sight px", 128, 0, 320, 8),                // 8 tiles (daylight)
    tune_int("Sight cone degrees", 120, 10, 360, 5),
    tune_int("Chase sight px", 176, 0, 320, 8),          // keeps tracking a target it already sees
    tune_int("Night sight px", 48, 0, 320, 8),           // spec: 3 tiles at night
    tune_int("Night chase sight px", 80, 0, 320, 8),
    tune_int("Hide px", 32, 0, 160, 8),                  // crouched in forest, mud or crops: seen within 2 tiles
    tune_int("Search frames", 1200, 0, 3600, 60),        // spec: 20-60 s at the last known position (random)
    tune_int("Search max frames", 3600, 0, 7200, 60),
    tune_int("Lunge range px", 18, 4, 64),               // inside bat/axe reach, so approaching zombies can be hit
    tune_int("Lunge wind-up", 18, 1, 60),                // spec: 18-frame wind-up
    tune_milli("Lunge speed", 2250, 250, 6000, 250),
    tune_int("Lunge frames", 8, 1, 30),
    tune_int("Grab range px", 9, 2, 24),
    tune_int("Recover frames", 30, 0, 120),
    tune_int("Hit stagger frames", 14, 0, 60),
    tune_milli("Knockback friction", 820, 500, 990, 10), // velocity kept per frame while sliding
    tune_int("Down frames", 70, 10, 300, 5),             // lying before it starts crawling
    tune_int("Decision interval", 4, 1, 16),             // spec: full decision every 4th frame, staggered
    tune_int("Separation px", 10, 0, 24),
    tune_int("Groan chance 1/n", 400, 10, 5000, 10),
    tune_int("Flow tiles per frame", 128, 32, 1024, 32), // spec: 512; fills stop past the farthest chaser
    tune_int("Track tiles", 10, 0, 40),                  // a chaser out of sight follows the flow field this far
};

static_assert(sizeof(zombie_tuning_defaults) / sizeof(tuning_entry) == int(zombie_param::COUNT));

[[nodiscard]] tuning_set& zombie_tuning();

[[nodiscard]] inline bn::fixed tuning(zombie_param param)
{
    return zombie_tuning().get(int(param));
}

}

#endif
