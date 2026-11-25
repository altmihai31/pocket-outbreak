#ifndef KX_WORLD_GEN_H
#define KX_WORLD_GEN_H

#include "bn_common.h"

#include "kx_world_gen_tuning.h"

// Terrain noise shared by the macro map and the chunk generator, so a macro cell's land always matches the tiles
// generated inside it.
namespace kx::world_gen
{

constexpr int elevation_octaves = sizeof(elevation_scales) / sizeof(int);
constexpr int moisture_octaves = sizeof(moisture_scales) / sizeof(int);

// Smoothstep on 0..255.
[[nodiscard]] constexpr int smooth(int t)
{
    return (t * t * (768 - (2 * t))) >> 16;
}

// One noise octave along a tile row, within one lattice cell: the values at the cell's left and right edges.
struct octave_row
{
    int left;
    int right;
    int shift;
    int weight;
};

// Elevation and moisture octaves for a tile row inside one chunk.
struct noise_row
{
    octave_row elevation[elevation_octaves];
    octave_row moisture[moisture_octaves];
};

template<int count>
[[nodiscard]] inline int evaluate(const octave_row (&octaves)[count], int tile_x)
{
    int total = 0;

    for(const octave_row& octave : octaves)
    {
        int fx = smooth(((tile_x & ((1 << octave.shift) - 1)) << 8) >> octave.shift);
        total += (octave.left + (((octave.right - octave.left) * fx) >> 8)) * octave.weight;
    }

    return total >> 4;
}

[[nodiscard]] BN_CODE_IWRAM octave_row make_octave(uint32_t seed, int cell_x, int tile_y, int shift, int weight);

// The row's octaves for the chunk holding tile_x.
BN_CODE_IWRAM void make_noise_row(uint32_t seed, int tile_x, int tile_y, noise_row& output);

// 0..255 at a tile of a prepared row.
[[nodiscard]] BN_CODE_IWRAM int row_elevation(const noise_row& row, int tile_x);
[[nodiscard]] BN_CODE_IWRAM int row_moisture(const noise_row& row, int tile_x);

// 0..255 at any tile, from the world seed (the macro map samples it once per cell).
[[nodiscard]] BN_CODE_IWRAM int elevation(uint32_t seed, int tile_x, int tile_y);
[[nodiscard]] BN_CODE_IWRAM int moisture(uint32_t seed, int tile_x, int tile_y);

enum class ground_kind : uint8_t
{
    DEEP,
    SHALLOW,
    COAST,
    OPEN,
    WOODS,
    FOREST,
    HILLS
};

[[nodiscard]] constexpr ground_kind classify(int elevation_value, int moisture_value)
{
    if(elevation_value < deep_water_below)
    {
        return ground_kind::DEEP;
    }

    if(elevation_value < shallow_water_below)
    {
        return ground_kind::SHALLOW;
    }

    if(elevation_value < coast_below)
    {
        return ground_kind::COAST;
    }

    if(elevation_value > hills_above)
    {
        return ground_kind::HILLS;
    }

    if(moisture_value > forest_moisture)
    {
        return ground_kind::FOREST;
    }

    return moisture_value > woods_moisture ? ground_kind::WOODS : ground_kind::OPEN;
}

}

#endif
