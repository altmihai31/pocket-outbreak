#ifndef KX_MACRO_MAP_H
#define KX_MACRO_MAP_H

#include "bn_common.h"

namespace kx::macro
{

// Macro map (spec: World > Two layers): 64x64 cells of 64x64 tiles, 2 bytes each (land, zone, road mask, flags),
// built once at world creation; plus a 1-byte horde count per cell for milestone 6.
constexpr int cells = 64;
constexpr int cell_tiles = 64;
constexpr int cell_shift = 6;

enum class land : uint8_t
{
    WATER,
    COAST,
    FIELD,
    WOODS,
    FOREST,
    HILLS,
    COUNT
};

enum class zone : uint8_t
{
    WILDERNESS,
    FARM,
    VILLAGE,
    TOWN,
    MILITARY,
    COUNT
};

// Road mask bits: a road leaves the cell centre towards that side.
constexpr int road_north = 1;
constexpr int road_east = 2;
constexpr int road_south = 4;
constexpr int road_west = 8;

struct settlement
{
    int8_t cell_x = 0;      // top-left cell
    int8_t cell_y = 0;
    int8_t size = 1;        // cells per side
    zone kind = zone::VILLAGE;
};

constexpr int max_settlements = 16;

// Builds the map from a seed (well under a second).
void generate(uint32_t seed);

// Open fields, no settlements or roads (the benches' test world).
void generate_empty();

namespace detail
{
    // Bits 0-2: land, 3-5: zone, 8-11: road mask, 12-13: quarter of a 2x2 settlement, 14: part of a 2x2
    // settlement, 15: free.
    extern uint16_t cell_data[cells * cells];

    // Zombie head count per cell (horde_count).
    alignas(4) extern uint8_t hordes[cells * cells];

    constexpr int zone_shift = 3;
    constexpr int roads_shift = 8;
    constexpr int quarter_shift = 12;
    constexpr uint16_t big_flag = 1 << 14;

    [[nodiscard]] inline uint16_t cell(int cell_x, int cell_y)
    {
        return unsigned(cell_x) < unsigned(cells) && unsigned(cell_y) < unsigned(cells) ?
                   cell_data[(cell_y * cells) + cell_x] : uint16_t(land::WATER);
    }
}

// Outside the map: open water, no zone, no roads.
[[nodiscard]] inline land land_at(int cell_x, int cell_y)
{
    return land(detail::cell(cell_x, cell_y) & 7);
}

[[nodiscard]] inline zone zone_at(int cell_x, int cell_y)
{
    return zone((detail::cell(cell_x, cell_y) >> detail::zone_shift) & 7);
}

[[nodiscard]] inline int roads_at(int cell_x, int cell_y)
{
    return (detail::cell(cell_x, cell_y) >> detail::roads_shift) & 15;
}

// Which quarter of a 2x2 settlement a cell is (0 top-left, 1 top-right, 2 bottom-left, 3 bottom-right), or -1.
[[nodiscard]] inline int settlement_quarter(int cell_x, int cell_y)
{
    uint16_t value = detail::cell(cell_x, cell_y);
    return (value & detail::big_flag) ? (value >> detail::quarter_shift) & 3 : -1;
}

[[nodiscard]] int settlement_count();
[[nodiscard]] const settlement& settlement_at(int index);

// Where a new survivor starts (spec: a random low-tier town): the centre of a village, on its crossroads.
[[nodiscard]] int start_settlement();

[[nodiscard]] uint8_t& horde_count(int cell_x, int cell_y);

}

#endif
