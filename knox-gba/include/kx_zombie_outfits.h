#ifndef KX_ZOMBIE_OUTFITS_H
#define KX_ZOMBIE_OUTFITS_H

#include "kx_loot.h"
#include "kx_macro_map.h"
#include "kx_outfit.h"

namespace kx
{

// Outfits by zone (spec: Zombies > Outfit): palette swaps (kx_outfit.h) that also hint at the loot carried.
enum class zombie_outfit : uint8_t
{
    CIVILIAN,
    POLICE,
    SOLDIER,
    DOCTOR,
    FARMER,
    COUNT
};

static_assert(int(zombie_outfit::COUNT) == outfit::zombie_outfits);

// What the corpse holds when searched.
constexpr loot_table outfit_loot[] = {
    loot_table::ZOMBIE_CORPSE,
    loot_table::POLICE_CORPSE,
    loot_table::SOLDIER_CORPSE,
    loot_table::DOCTOR_CORPSE,
    loot_table::FARMER_CORPSE,
};

static_assert(sizeof(outfit_loot) / sizeof(loot_table) == int(zombie_outfit::COUNT));

// Chance of each outfit (out of the row's sum) by the zone a zombie appears in:
// civilian, police, soldier, doctor, farmer.
constexpr uint8_t outfit_weights[][int(zombie_outfit::COUNT)] = {
    { 80, 2, 2, 1, 15 },    // WILDERNESS: hikers and farmhands
    { 45, 3, 2, 2, 48 },    // FARM
    { 70, 8, 2, 6, 14 },    // VILLAGE
    { 64, 14, 4, 14, 4 },   // TOWN: police and clinic staff
    { 22, 6, 64, 6, 2 },    // MILITARY
};

static_assert(sizeof(outfit_weights) / sizeof(outfit_weights[0]) == int(macro::zone::COUNT));

// The hand-made test map has no zones: a mix, so every outfit shows up.
constexpr uint8_t test_map_outfit_weights[int(zombie_outfit::COUNT)] = { 40, 15, 15, 15, 15 };

}

#endif
