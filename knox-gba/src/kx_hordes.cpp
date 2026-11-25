#include "kx_hordes.h"
#include "kx_sandbox.h"

#include "kx_hordes_detail.h"

#include "bn_algorithm.h"
#include "bn_math.h"

#include "kx_game.h"
#include "kx_hash.h"
#include "kx_horde_tuning.h"
#include "kx_macro_map.h"
#include "kx_save.h"
#include "kx_world.h"

namespace kx::hordes::detail
{
    attraction attractions[horde::max_attractions];
    int attractions_count = 0;
    alignas(4) BN_DATA_EWRAM_BSS uint8_t incoming[macro::cells * macro::cells];
    int migration_next = -1;
    int migration_hour = 0;
}

namespace kx::hordes
{

using namespace detail;

namespace
{
    constexpr int cell_area = macro::cell_tiles * macro::cell_tiles;
    constexpr int screen_half_columns = 8;      // half the screen, rounded up, in tiles
    constexpr int screen_half_rows = 6;

    int step_timer = 0;
    int edge_counts[edge_directions];


    struct square
    {
        int left;
        int top;
        int right;      // inclusive
        int bottom;
    };

    [[nodiscard]] square active_square(int tile_x, int tile_y)
    {
        return square{ tile_x - horde::active_tiles, tile_y - horde::active_tiles, tile_x + horde::active_tiles,
                       tile_y + horde::active_tiles };
    }

    // Tiles of a macro cell inside the square.
    [[nodiscard]] square overlap(const square& area, int cell_x, int cell_y)
    {
        return square{ bn::max(area.left, cell_x * macro::cell_tiles), bn::max(area.top, cell_y * macro::cell_tiles),
                       bn::min(area.right, (cell_x * macro::cell_tiles) + macro::cell_tiles - 1),
                       bn::min(area.bottom, (cell_y * macro::cell_tiles) + macro::cell_tiles - 1) };
    }

    [[nodiscard]] int area_of(const square& value)
    {
        return value.right < value.left || value.bottom < value.top ? 0 :
               (value.right - value.left + 1) * (value.bottom - value.top + 1);
    }

    [[nodiscard]] bool indoors(int tile_x, int tile_y)
    {
        world_gfx base = world_gfx(world_gfx_base[int(world::ground_gfx(tile_x, tile_y))]);
        return base == world_gfx::WOOD_FLOOR || base == world_gfx::TILE_FLOOR || base == world_gfx::CONCRETE;
    }

    [[nodiscard]] bool open_tile(int tile_x, int tile_y)
    {
        return world::loaded(tile_x, tile_y) && ! world::solid(tile_x, tile_y) &&
               world::terrain_at(tile_x, tile_y) != terrain::DEEP_WATER;
    }

    // Turns one counted zombie near the player into an individual: a cell picked by its count weighted by how much
    // of it lies in range, then a spot in it out of sight, indoors when a try finds one. False if none is left.
    bool materialise_one(game& owner)
    {
        bn::random& random = owner.random();
        int player_x = owner.hero().position().x().integer() >> world::tile_shift;
        int player_y = owner.hero().position().y().integer() >> world::tile_shift;
        square area = active_square(player_x, player_y);
        int total = 0;

        for(int cell_y = area.top >> macro::cell_shift; cell_y <= area.bottom >> macro::cell_shift; ++cell_y)
        {
            for(int cell_x = area.left >> macro::cell_shift; cell_x <= area.right >> macro::cell_shift; ++cell_x)
            {
                total += macro::horde_count(cell_x, cell_y) * area_of(overlap(area, cell_x, cell_y));
            }
        }

        // Less than half a zombie expected in range: leave the rest counted.
        if(total < cell_area / 2)
        {
            return false;
        }

        int pick = random.get_int(total);

        for(int cell_y = area.top >> macro::cell_shift; cell_y <= area.bottom >> macro::cell_shift; ++cell_y)
        {
            for(int cell_x = area.left >> macro::cell_shift; cell_x <= area.right >> macro::cell_shift; ++cell_x)
            {
                uint8_t& count = macro::horde_count(cell_x, cell_y);
                square tiles = overlap(area, cell_x, cell_y);
                int weight = count * area_of(tiles);

                if(pick >= weight)
                {
                    pick -= weight;
                    continue;
                }

                for(int attempt = 0; attempt < horde::spawn_tries; ++attempt)
                {
                    int tile_x = tiles.left + random.get_int(tiles.right - tiles.left + 1);
                    int tile_y = tiles.top + random.get_int(tiles.bottom - tiles.top + 1);
                    int dx = bn::abs(tile_x - player_x);
                    int dy = bn::abs(tile_y - player_y);
                    bool hidden = dx > screen_half_columns || dy > screen_half_rows;
                    bool want_indoors = attempt < horde::spawn_tries / 2;

                    if(hidden && bn::max(dx, dy) >= horde::min_spawn_tiles && open_tile(tile_x, tile_y) &&
                       (! want_indoors || indoors(tile_x, tile_y)))
                    {
                        owner.zombies().spawn(bn::fixed_point((tile_x * world::tile_size) + (world::tile_size / 2),
                                                              (tile_y * world::tile_size) + 12), random);
                        --count;
                        return true;
                    }
                }

                return false;
            }
        }

        return false;
    }
}

void seed_counts(uint32_t seed)
{
    roll_stream rng(hash32(seed ^ 0x484f5244u));

    for(int cell_y = 0; cell_y < macro::cells; ++cell_y)
    {
        for(int cell_x = 0; cell_x < macro::cells; ++cell_x)
        {
            const int* range = horde::start_counts[int(macro::zone_at(cell_x, cell_y))];
            int count = range[0] + rng.next(range[1] - range[0] + 1);
            count = (count * sandbox::tuning::population_percent[sandbox::value(sandbox::setting::ZOMBIES)]) / 100;
            macro::land land = macro::land_at(cell_x, cell_y);

            if(land == macro::land::WATER)
            {
                count = 0;
            }
            else if(land == macro::land::COAST)
            {
                count = (count * horde::coast_percent) / 100;
            }

            macro::horde_count(cell_x, cell_y) = uint8_t(bn::min(count, 255));
        }
    }

    step_timer = 0;
    attractions_count = 0;
    migration_next = -1;

    for(uint8_t& value : incoming)
    {
        value = 0;
    }
}

int counted_near(const bn::fixed_point& position)
{
    square area = active_square(position.x().integer() >> world::tile_shift,
                                position.y().integer() >> world::tile_shift);
    int total = 0;

    for(int cell_y = area.top >> macro::cell_shift; cell_y <= area.bottom >> macro::cell_shift; ++cell_y)
    {
        for(int cell_x = area.left >> macro::cell_shift; cell_x <= area.right >> macro::cell_shift; ++cell_x)
        {
            total += macro::horde_count(cell_x, cell_y) * area_of(overlap(area, cell_x, cell_y));
        }
    }

    return total / cell_area;
}

int edge_count(int direction)
{
    return edge_counts[direction];
}

namespace
{
    // Which of the 8 directions an offset points to (east first, counter-clockwise; y grows downwards).
    [[nodiscard]] int direction_of(int dx, int dy)
    {
        int ax = bn::abs(dx);
        int ay = bn::abs(dy);

        if(ax * 5 > ay * 12)
        {
            return dx > 0 ? 0 : 4;
        }

        if(ay * 5 > ax * 12)
        {
            return dy < 0 ? 2 : 6;
        }

        return dx > 0 ? (dy < 0 ? 1 : 7) : (dy < 0 ? 3 : 5);
    }

    void update_edges(game& owner)
    {
        int sums[edge_directions] = {};

        if(owner.zombies().count() >= horde::max_active)
        {
            int player_x = owner.hero().position().x().integer() >> world::tile_shift;
            int player_y = owner.hero().position().y().integer() >> world::tile_shift;
            constexpr int half = horde::edge_block_tiles / 2;

            for(int center_y = half - horde::active_tiles; center_y < horde::active_tiles; center_y += horde::edge_block_tiles)
            {
                for(int center_x = half - horde::active_tiles; center_x < horde::active_tiles;
                    center_x += horde::edge_block_tiles)
                {
                    if(bn::abs(center_x) < screen_half_columns && bn::abs(center_y) < screen_half_rows)
                    {
                        continue;
                    }

                    square block{ player_x + center_x - half, player_y + center_y - half,
                                  player_x + center_x + half - 1, player_y + center_y + half - 1 };
                    int sum = 0;

                    for(int cell_y = block.top >> macro::cell_shift; cell_y <= block.bottom >> macro::cell_shift; ++cell_y)
                    {
                        for(int cell_x = block.left >> macro::cell_shift; cell_x <= block.right >> macro::cell_shift;
                            ++cell_x)
                        {
                            sum += macro::horde_count(cell_x, cell_y) * area_of(overlap(block, cell_x, cell_y));
                        }
                    }

                    sums[direction_of(center_x, center_y)] += sum;
                }
            }
        }

        for(int direction = 0; direction < edge_directions; ++direction)
        {
            edge_counts[direction] = sums[direction] / cell_area;
        }
    }
}

void update(game& owner)
{
    if(world::test_world())
    {
        return;
    }

    if(migration_next >= 0)
    {
        migrate_cells(horde::cells_per_frame);
    }

    if(++step_timer < horde::step_frames)
    {
        return;
    }

    step_timer = 0;
    owner.zombies().dissolve(owner.hero().position(), horde::dissolve_tiles * world::tile_size,
                             horde::dissolves_per_step);

    for(int spawn = 0; spawn < horde::spawns_per_step && owner.zombies().count() < horde::max_active; ++spawn)
    {
        if(! materialise_one(owner))
        {
            break;
        }
    }

    update_edges(owner);
}

void fill(game& owner)
{
    if(world::test_world())
    {
        return;
    }

    owner.zombies().dissolve(owner.hero().position(), horde::dissolve_tiles * world::tile_size);

    for(int& value : edge_counts)
    {
        value = 0;
    }

    for(int attempt = 0; attempt < horde::max_active * 2 && owner.zombies().count() < horde::max_active; ++attempt)
    {
        if(! materialise_one(owner) && attempt > horde::max_active)
        {
            break;
        }
    }
}

void noise(const bn::fixed_point& position, int radius_tiles, int hour)
{
    if(world::test_world() || radius_tiles < horde::loud_tiles)
    {
        return;
    }

    int cell_x = (position.x().integer() >> world::tile_shift) >> macro::cell_shift;
    int cell_y = (position.y().integer() >> world::tile_shift) >> macro::cell_shift;
    int slot = -1;

    // The cell's entry if there is one, else a free slot, else the coldest one.
    for(int index = 0; index < attractions_count && slot < 0; ++index)
    {
        if(attractions[index].cell_x == cell_x && attractions[index].cell_y == cell_y)
        {
            slot = index;
        }
    }

    if(slot < 0)
    {
        if(attractions_count < horde::max_attractions)
        {
            slot = attractions_count++;
        }
        else
        {
            slot = 0;

            for(int index = 1; index < attractions_count; ++index)
            {
                if(attractions[index].heat < attractions[slot].heat)
                {
                    slot = index;
                }
            }
        }

        attractions[slot] = attraction{ cell_x, cell_y, 0, hour };
    }

    attractions[slot].heat = bn::min(attractions[slot].heat + radius_tiles, 0x7FFF);
    attractions[slot].hour = hour;
}

void hour_passed(int hour, bool at_once)
{
    if(world::test_world())
    {
        return;
    }

    // An unfinished pass completes first.
    while(migration_next >= 0)
    {
        migrate_cells(macro::cells * macro::cells);
    }

    migration_hour = hour;
    migration_next = 0;

    while(at_once && migration_next >= 0)
    {
        migrate_cells(macro::cells * macro::cells);
    }
}

void finish_migration()
{
    while(migration_next >= 0)
    {
        migrate_cells(macro::cells * macro::cells);
    }
}

void save(save::writer& out)
{
    finish_migration();
    out.bytes(macro::detail::hordes, macro::cells * macro::cells);
    out.put(attractions_count);
    out.put(attractions);
    out.put(migration_hour);
}

void load(save::reader& in)
{
    in.bytes(macro::detail::hordes, macro::cells * macro::cells);
    in.get(attractions_count);
    in.get(attractions);
    in.get(migration_hour);
    migration_next = -1;
    step_timer = 0;

    for(uint8_t& value : incoming)
    {
        value = 0;
    }
}

int total_counted()
{
    // Mid-pass, the movers wait in incoming.
    return sum_counts(macro::detail::hordes) + sum_counts(incoming);
}

}
