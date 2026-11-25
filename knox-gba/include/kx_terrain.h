#ifndef KX_TERRAIN_H
#define KX_TERRAIN_H

#include "kx_movement_tuning.h"

namespace kx
{

enum class terrain : uint8_t
{
    ROAD,
    PAVEMENT,
    FLOOR,
    GRASS,
    FOREST,
    MUD,
    SHALLOW_WATER,
    DEEP_WATER,
    SAND,
    SOIL,
    CROPS,
    COUNT
};

// Ground behaviour per terrain. Multipliers and noise live in the movement tuning table so they can be edited
// live; NONE means full speed / no extra noise.
struct terrain_info
{
    static constexpr int NONE = -1;

    const char* name;
    int speed_param;
    int noise_param;
    bool hides;         // forest, mud and crops (long grass) hide a crouching player (used from milestone 6)
};

constexpr terrain_info terrain_infos[] = {
    { "ROAD", terrain_info::NONE, terrain_info::NONE, false },
    { "PAVEMENT", terrain_info::NONE, terrain_info::NONE, false },
    { "FLOOR", terrain_info::NONE, terrain_info::NONE, false },
    { "GRASS", terrain_info::NONE, terrain_info::NONE, false },
    { "FOREST", int(move_param::FOREST_MULT), terrain_info::NONE, true },
    { "MUD", int(move_param::MUD_MULT), terrain_info::NONE, true },
    { "WATER", int(move_param::SHALLOW_MULT), int(move_param::SHALLOW_NOISE), false },
    { "DEEP", terrain_info::NONE, terrain_info::NONE, false },
    { "SAND", terrain_info::NONE, terrain_info::NONE, false },
    { "SOIL", terrain_info::NONE, terrain_info::NONE, false },
    { "CROPS", terrain_info::NONE, terrain_info::NONE, true },
};

static_assert(sizeof(terrain_infos) / sizeof(terrain_info) == int(terrain::COUNT));

[[nodiscard]] constexpr const terrain_info& info(terrain value)
{
    return terrain_infos[int(value)];
}

}

#endif
