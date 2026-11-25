#include "kx_hordes_detail.h"

#include "kx_hash.h"

namespace kx::hordes::detail
{

// Helpers only reach IWRAM when marked BN_CODE_IWRAM, so the pass's are.
namespace
{
    constexpr int total_cells = macro::cells * macro::cells;

    [[nodiscard]] inline int sign(int value)
    {
        return value > 0 ? 1 : value < 0 ? -1 : 0;
    }

    [[nodiscard]] inline int maximum(int a, int b)
    {
        return a > b ? a : b;
    }

    [[nodiscard]] inline int distance(int a, int b)
    {
        return a > b ? a - b : b - a;
    }

    [[nodiscard]] inline bool water(int cell_x, int cell_y)
    {
        return macro::land_at(cell_x, cell_y) == macro::land::WATER;
    }

    // The hottest recent noise pulling on a cell (heat over distance), or -1.
    [[nodiscard]] BN_CODE_IWRAM int strongest_pull(int cell_x, int cell_y, int active)
    {
        int best = -1;
        int best_score = 0;

        for(int index = 0; index < active; ++index)
        {
            const attraction& value = attractions[index];
            int cells = maximum(distance(value.cell_x, cell_x), distance(value.cell_y, cell_y));

            if(cells == 0 || cells > horde::attract_cells)
            {
                continue;
            }

            int score = (value.heat * 16) / cells;

            if(score > best_score)
            {
                best = index;
                best_score = score;
            }
        }

        return best;
    }

    BN_CODE_IWRAM void migrate_cell(int index, int active)
    {
        uint8_t& count = macro::detail::hordes[index];

        if(! count)
        {
            return;
        }

        int cell_x = index % macro::cells;
        int cell_y = index / macro::cells;
        int pull = active ? strongest_pull(cell_x, cell_y, active) : -1;
        int step_x;
        int step_y;
        int moving;

        if(pull >= 0)
        {
            // One cell along the gradient towards the noise.
            step_x = sign(attractions[pull].cell_x - cell_x);
            step_y = sign(attractions[pull].cell_y - cell_y);
            moving = maximum((count * horde::move_percent) / 100, 1);
        }
        else
        {
            // Now and then a cell's zombies drift to a random neighbour.
            uint32_t roll = hash_xy(uint32_t(migration_hour) * 0x4d494752u, cell_x, cell_y);

            if(int(roll % 100) >= horde::wander_percent)
            {
                return;
            }

            constexpr int directions[8][2] = { { 1, 0 }, { 1, 1 }, { 0, 1 }, { -1, 1 }, { -1, 0 }, { -1, -1 },
                                               { 0, -1 }, { 1, -1 } };
            const int* direction = directions[(roll >> 8) & 7];
            step_x = direction[0];
            step_y = direction[1];
            moving = maximum((count * horde::wander_share) / 100, 1);
        }

        int target_x = cell_x + step_x;
        int target_y = cell_y + step_y;

        // Around water: try the straight steps instead of the diagonal.
        if(step_x && step_y && water(target_x, target_y))
        {
            target_y = cell_y;

            if(water(target_x, target_y))
            {
                target_x = cell_x;
                target_y = cell_y + step_y;
            }
        }

        if(unsigned(target_x) >= unsigned(macro::cells) || unsigned(target_y) >= unsigned(macro::cells) ||
           water(target_x, target_y))
        {
            return;
        }

        count = uint8_t(count - moving);
        uint8_t& arrived = incoming[(target_y * macro::cells) + target_x];
        arrived = uint8_t(arrived + moving > 255 ? 255 : arrived + moving);
    }
}

void migrate_cells(int count)
{
    // Second phase: the movers arrive, a slice at a time like the moves.
    if(migration_next >= total_cells)
    {
        int first = migration_next - total_cells;
        int end = first + count < total_cells ? first + count : total_cells;

        for(int index = first; index < end; ++index)
        {
            if(uint8_t arrived = incoming[index])
            {
                uint8_t& value = macro::detail::hordes[index];
                value = uint8_t(value + arrived > 255 ? 255 : value + arrived);
                incoming[index] = 0;
            }
        }

        migration_next = end + total_cells;

        if(end == total_cells)
        {
            // The pass is over; the noise cools.
            for(int index = 0; index < attractions_count; ++index)
            {
                attractions[index].heat -= attractions[index].heat / 4;
            }

            migration_next = -1;
        }

        return;
    }

    // Only noise loud enough and recent enough pulls; listed first so the per-cell check stays short.
    int active = 0;

    for(int index = 0; index < attractions_count; ++index)
    {
        attraction value = attractions[index];

        if(value.heat >= horde::heat_threshold && migration_hour - value.hour <= horde::memory_hours)
        {
            attractions[index] = attractions[active];
            attractions[active++] = value;
        }
    }

    for(int end = migration_next + count < total_cells ? migration_next + count : total_cells;
        migration_next < end; ++migration_next)
    {
        migrate_cell(migration_next, active);
    }

}

int sum_counts(const uint8_t* counts)
{
    const uint32_t* words = reinterpret_cast<const uint32_t*>(counts);
    int total = 0;

    for(int index = 0; index < total_cells / 4; index += 64)
    {
        // Pairs of bytes added in 16-bit lanes: 64 words at most reach 64 * 2 * 255, well under 65536.
        uint32_t lanes = 0;

        for(int word = index; word < index + 64; ++word)
        {
            uint32_t value = words[word];
            lanes += (value & 0x00FF00FF) + ((value >> 8) & 0x00FF00FF);
        }

        total += int(lanes & 0xFFFF) + int(lanes >> 16);
    }

    return total;
}

}
