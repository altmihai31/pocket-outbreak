#include "kx_flow_field.h"

#include "kx_world_detail.h"

namespace kx::flow
{

namespace
{
    constexpr int area = size * size;
    constexpr int half = size / 2;

    // Two fields: one complete (read by the zombies), one being built. 8 KB, plus the fill's queue.
    BN_DATA_EWRAM_BSS uint8_t fields[2][area];
    BN_DATA_EWRAM_BSS uint16_t queue[area];
    int origin_x[2];
    int origin_y[2];
    int ready = -1;             // the complete field, or -1
    int building = -1;          // the field being built, or -1 between fills
    int head = 0;
    int tail = 0;
    int limit = 0;              // the fill being built stops at this distance
    int ready_limit = 0;        // and the complete one did

    // Path length the fill covers for chasers this far away (in a straight line): room to go around walls.
    [[nodiscard]] inline int path_limit(int reach_tiles)
    {
        int value = (reach_tiles * 2) + 4;
        return value < blocked - 1 ? value : blocked - 1;
    }

    [[nodiscard]] inline bool passable(int tile_x, int tile_y)
    {
        uint8_t flags = world::detail::tile_flags(tile_x, tile_y);

        if((flags & world::detail::solid_flag) && ! (flags & world::detail::breakable_flag))
        {
            return false;
        }

        // Parked cars are in the way too.
        return ! world::detail::obstacle_count || ! world::detail::obstacle_on_tile(tile_x, tile_y);
    }

    // Reaches a neighbour: its distance if it is open (queued to spread further), else marked blocked so it is
    // never looked up again.
    BN_CODE_IWRAM void reach(uint8_t* field, int index, int local_x, int local_y, int distance)
    {
        if(field[index] != unknown)
        {
            return;
        }

        if(passable(origin_x[building] + local_x, origin_y[building] + local_y))
        {
            field[index] = uint8_t(distance < blocked - 1 ? distance : blocked - 1);
            queue[tail++] = uint16_t(index);
        }
        else
        {
            field[index] = blocked;
        }
    }
}

void reset()
{
    ready = -1;
    building = -1;
}

void update(int player_tile_x, int player_tile_y, int pops, int reach_tiles)
{
    if(building < 0)
    {
        // Nobody chasing, or the last field still fits: nothing to do.
        if(reach_tiles <= 0)
        {
            return;
        }

        if(ready >= 0 && origin_x[ready] + half == player_tile_x && origin_y[ready] + half == player_tile_y &&
           path_limit(reach_tiles) <= ready_limit)
        {
            return;
        }

        // A new fill from where the player stands.
        building = ready == 0 ? 1 : 0;
        limit = path_limit(reach_tiles);
        origin_x[building] = player_tile_x - half;
        origin_y[building] = player_tile_y - half;

        uint32_t* words = reinterpret_cast<uint32_t*>(fields[building]);

        for(int index = 0; index < area / 4; ++index)
        {
            words[index] = 0xFFFFFFFF;
        }

        int start = (half * size) + half;
        fields[building][start] = 0;
        queue[0] = uint16_t(start);
        head = 0;
        tail = 1;
    }

    uint8_t* field = fields[building];

    for(int pop = 0; pop < pops && head < tail; ++pop)
    {
        int index = queue[head];
        int distance = field[index] + 1;

        // Past the farthest chaser: done.
        if(distance > limit)
        {
            head = tail;
            break;
        }

        ++head;
        int local_x = index & (size - 1);
        int local_y = index / size;

        if(local_x > 0)
        {
            reach(field, index - 1, local_x - 1, local_y, distance);
        }

        if(local_x < size - 1)
        {
            reach(field, index + 1, local_x + 1, local_y, distance);
        }

        if(local_y > 0)
        {
            reach(field, index - size, local_x, local_y - 1, distance);
        }

        if(local_y < size - 1)
        {
            reach(field, index + size, local_x, local_y + 1, distance);
        }
    }

    if(head >= tail)
    {
        ready = building;
        ready_limit = limit;
        building = -1;
    }
}

int distance(int tile_x, int tile_y)
{
    if(ready < 0)
    {
        return unknown;
    }

    int local_x = tile_x - origin_x[ready];
    int local_y = tile_y - origin_y[ready];

    if(unsigned(local_x) >= unsigned(size) || unsigned(local_y) >= unsigned(size))
    {
        return unknown;
    }

    return fields[ready][(local_y * size) + local_x];
}

bool next_step(int tile_x, int tile_y, int& next_x, int& next_y)
{
    if(ready < 0)
    {
        return false;
    }

    const uint8_t* field = fields[ready];
    int local_x = tile_x - origin_x[ready];
    int local_y = tile_y - origin_y[ready];

    if(local_x < 1 || local_y < 1 || local_x >= size - 1 || local_y >= size - 1)
    {
        return false;
    }

    int index = (local_y * size) + local_x;
    int best = field[index];

    if(best >= blocked)
    {
        return false;
    }

    int best_dx = 0;
    int best_dy = 0;

    for(int dy = -1; dy <= 1; ++dy)
    {
        for(int dx = -1; dx <= 1; ++dx)
        {
            int value = field[index + (dy * size) + dx];

            if(value >= best || value >= blocked)
            {
                continue;
            }

            // Diagonals only past open corners, so zombies do not cut through wall ends.
            if(dx && dy && (field[index + dx] >= blocked || field[index + (dy * size)] >= blocked))
            {
                continue;
            }

            best = value;
            best_dx = dx;
            best_dy = dy;
        }
    }

    if(! best_dx && ! best_dy)
    {
        return false;
    }

    next_x = tile_x + best_dx;
    next_y = tile_y + best_dy;
    return true;
}

}
