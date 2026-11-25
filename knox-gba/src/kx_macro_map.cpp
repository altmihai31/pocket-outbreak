#include "kx_macro_map.h"

#include "bn_algorithm.h"
#include "bn_math.h"

#include "kx_hash.h"
#include "kx_world_gen.h"

namespace kx::macro
{

namespace detail
{
    BN_DATA_EWRAM_BSS uint16_t cell_data[cells * cells];
    alignas(4) BN_DATA_EWRAM_BSS uint8_t hordes[cells * cells];
}

using namespace detail;
using namespace world_gen;

namespace
{
    settlement settlements[max_settlements];
    int settlements_size = 0;
    int start_index = 0;
    bool linked[max_settlements][max_settlements];

    [[nodiscard]] land land_of(ground_kind kind)
    {
        switch(kind)
        {

        case ground_kind::DEEP:
        case ground_kind::SHALLOW:
            return land::WATER;

        case ground_kind::COAST:
            return land::COAST;

        case ground_kind::WOODS:
            return land::WOODS;

        case ground_kind::FOREST:
            return land::FOREST;

        case ground_kind::HILLS:
            return land::HILLS;

        default:
            return land::FIELD;
        }
    }

    void set_zone(int cell_x, int cell_y, zone value)
    {
        uint16_t& data = cell_data[(cell_y * cells) + cell_x];
        data = uint16_t((data & ~(7 << zone_shift)) | (int(value) << zone_shift));
    }

    void add_road(int cell_x, int cell_y, int bits)
    {
        cell_data[(cell_y * cells) + cell_x] |= uint16_t(bits << roads_shift);
    }

    // Settlements need land they can be built on: no water, no hills.
    [[nodiscard]] bool buildable(int cell_x, int cell_y)
    {
        land value = land_at(cell_x, cell_y);
        return value != land::WATER && value != land::HILLS && zone_at(cell_x, cell_y) == zone::WILDERNESS;
    }

    // Tries random spots; the top-left cell's column stays within [min_x, max_x].
    bool place(roll_stream& rng, zone kind, int size, int min_x, int max_x)
    {
        for(int attempt = 0; attempt < world_gen::placement_attempts && settlements_size < max_settlements;
            ++attempt)
        {
            int cell_x = min_x + rng.next(bn::max(max_x - min_x - size + 2, 1));
            int cell_y = 1 + rng.next(cells - size - 1);
            bool fits = true;

            for(int y = cell_y; y < cell_y + size && fits; ++y)
            {
                for(int x = cell_x; x < cell_x + size && fits; ++x)
                {
                    fits = buildable(x, y);
                }
            }

            // Spacing between centres, in half cells.
            for(int index = 0; index < settlements_size && fits; ++index)
            {
                const settlement& other = settlements[index];
                int dx = bn::abs(((cell_x * 2) + size) - ((other.cell_x * 2) + other.size));
                int dy = bn::abs(((cell_y * 2) + size) - ((other.cell_y * 2) + other.size));
                fits = bn::max(dx, dy) >= world_gen::settlement_spacing * 2;
            }

            if(fits)
            {
                settlement& value = settlements[settlements_size++];
                value.cell_x = int8_t(cell_x);
                value.cell_y = int8_t(cell_y);
                value.size = int8_t(size);
                value.kind = kind;

                for(int y = cell_y; y < cell_y + size; ++y)
                {
                    for(int x = cell_x; x < cell_x + size; ++x)
                    {
                        set_zone(x, y, kind);

                        if(size == 2)
                        {
                            int quarter = (x - cell_x) + ((y - cell_y) * 2);
                            cell_data[(y * cells) + x] |= uint16_t(big_flag | (quarter << quarter_shift));
                        }
                    }
                }

                // Streets between the cells of a bigger settlement.
                if(size == 2)
                {
                    add_road(cell_x, cell_y, road_east | road_south);
                    add_road(cell_x + 1, cell_y, road_west | road_south);
                    add_road(cell_x, cell_y + 1, road_east | road_north);
                    add_road(cell_x + 1, cell_y + 1, road_west | road_north);
                }

                return true;
            }
        }

        return false;
    }

    // Walks an L-shaped road from cell centre to cell centre (along x first or y first); counts the water cells it
    // crosses, and lays it down when asked.
    int walk_road(int from_x, int from_y, int to_x, int to_y, bool x_first, bool lay)
    {
        int x = from_x;
        int y = from_y;
        int water = 0;

        for(int leg = 0; leg < 2; ++leg)
        {
            bool along_x = (leg == 0) == x_first;

            while(along_x ? x != to_x : y != to_y)
            {
                int step = along_x ? (to_x > x ? 1 : -1) : (to_y > y ? 1 : -1);
                int leave = along_x ? (step > 0 ? road_east : road_west) : (step > 0 ? road_south : road_north);
                int enter = along_x ? (step > 0 ? road_west : road_east) : (step > 0 ? road_north : road_south);

                if(lay)
                {
                    add_road(x, y, leave);
                }

                (along_x ? x : y) += step;
                water += land_at(x, y) == land::WATER;

                if(lay)
                {
                    add_road(x, y, enter);
                }
            }
        }

        return water;
    }

    // A road with one turn, on whichever corner crosses less water (bridges).
    void route(int from_x, int from_y, int to_x, int to_y)
    {
        bool x_first = walk_road(from_x, from_y, to_x, to_y, true, false) <=
                       walk_road(from_x, from_y, to_x, to_y, false, false);
        walk_road(from_x, from_y, to_x, to_y, x_first, true);
    }

    [[nodiscard]] int distance(int a, int b)
    {
        return bn::abs(settlements[a].cell_x - settlements[b].cell_x) +
               bn::abs(settlements[a].cell_y - settlements[b].cell_y);
    }

    void link(int a, int b)
    {
        linked[a][b] = true;
        linked[b][a] = true;
        route(settlements[a].cell_x, settlements[a].cell_y, settlements[b].cell_x, settlements[b].cell_y);
    }

    // Spec: roads are a minimum spanning tree between the settlements plus a few extra links for loops.
    void build_roads(roll_stream& rng)
    {
        for(auto& row : linked)
        {
            for(bool& value : row)
            {
                value = false;
            }
        }

        bool in_tree[max_settlements] = {};
        in_tree[0] = true;

        for(int added = 1; added < settlements_size; ++added)
        {
            int best_from = -1;
            int best_to = -1;
            int best = 0x7FFF;

            for(int from = 0; from < settlements_size; ++from)
            {
                for(int to = 0; to < settlements_size && in_tree[from]; ++to)
                {
                    if(! in_tree[to] && distance(from, to) < best)
                    {
                        best = distance(from, to);
                        best_from = from;
                        best_to = to;
                    }
                }
            }

            in_tree[best_to] = true;
            link(best_from, best_to);
        }

        for(int extra = 0; extra < world_gen::extra_links && settlements_size > 2; ++extra)
        {
            int from = rng.next(settlements_size);
            int nearest = -1;

            for(int to = 0; to < settlements_size; ++to)
            {
                if(to != from && ! linked[from][to] && (nearest < 0 || distance(from, to) < distance(from, nearest)))
                {
                    nearest = to;
                }
            }

            if(nearest >= 0)
            {
                link(from, nearest);
            }
        }
    }
}

void generate(uint32_t seed)
{
    // Land from the same noise the chunks use, sampled at each cell's centre.
    for(int cell_y = 0; cell_y < cells; ++cell_y)
    {
        for(int cell_x = 0; cell_x < cells; ++cell_x)
        {
            int tile_x = (cell_x * cell_tiles) + (cell_tiles / 2);
            int tile_y = (cell_y * cell_tiles) + (cell_tiles / 2);
            ground_kind kind = classify(elevation(seed, tile_x, tile_y), moisture(seed, tile_x, tile_y));
            cell_data[(cell_y * cells) + cell_x] = uint16_t(land_of(kind));
            hordes[(cell_y * cells) + cell_x] = 0;
        }
    }

    // Settlements first: towns and the military base inland, villages anywhere.
    roll_stream rng(hash32(seed ^ 0x544f574eu));
    settlements_size = 0;

    for(int index = 0; index < world_gen::military_bases; ++index)
    {
        place(rng, zone::MILITARY, 2, 1, world_gen::inland_cells / 2);
    }

    for(int index = 0; index < world_gen::towns; ++index)
    {
        place(rng, zone::TOWN, 2, 1, world_gen::inland_cells);
    }

    int first_village = settlements_size;

    for(int index = 0; index < world_gen::villages; ++index)
    {
        place(rng, zone::VILLAGE, 1, 1, cells - 2);
    }

    // Farmland around the villages.
    for(int index = first_village; index < settlements_size; ++index)
    {
        constexpr int sides[4][2] = { { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 } };

        for(const auto& side : sides)
        {
            int x = settlements[index].cell_x + side[0];
            int y = settlements[index].cell_y + side[1];

            if(buildable(x, y) && rng.next(100) < world_gen::farm_percent)
            {
                set_zone(x, y, zone::FARM);
            }
        }
    }

    if(settlements_size > 1)
    {
        build_roads(rng);
    }

    start_index = first_village < settlements_size ? first_village + rng.next(settlements_size - first_village) : 0;
}

void generate_empty()
{
    for(int index = 0; index < cells * cells; ++index)
    {
        cell_data[index] = uint16_t(land::FIELD);
        hordes[index] = 0;
    }

    settlements_size = 0;
    start_index = 0;
}

int settlement_count()
{
    return settlements_size;
}

const settlement& settlement_at(int index)
{
    return settlements[index];
}

int start_settlement()
{
    return start_index;
}

uint8_t& horde_count(int cell_x, int cell_y)
{
    return hordes[(bn::clamp(cell_y, 0, cells - 1) * cells) + bn::clamp(cell_x, 0, cells - 1)];
}

}
