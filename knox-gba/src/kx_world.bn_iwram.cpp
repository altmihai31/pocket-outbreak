#include "kx_world_detail.h"

namespace kx::world
{

bool box_blocked(int left, int top, int right, int bottom, bool swimmer)
{
    int tile_x0 = left >> tile_shift;
    int tile_x1 = right >> tile_shift;
    int tile_y0 = top >> tile_shift;
    int tile_y1 = bottom >> tile_shift;

    for(int tile_y = tile_y0; tile_y <= tile_y1; ++tile_y)
    {
        for(int tile_x = tile_x0; tile_x <= tile_x1; ++tile_x)
        {
            int flags = detail::tile_flags(tile_x, tile_y);

            if(flags & detail::solid_flag)
            {
                if(! swimmer || ! detail::swimmable(tile_x, tile_y))
                {
                    return true;
                }
            }
        }
    }

    for(int index = 0; index < detail::obstacle_count; ++index)
    {
        const obstacle& value = detail::obstacles[index];

        if(left <= value.right && right >= value.left && top <= value.bottom && bottom >= value.top)
        {
            return true;
        }
    }

    return false;
}

bool line_of_sight(int from_x, int from_y, int to_x, int to_y)
{
    // Grid walk (Amanatides-Woo) over the tiles the segment crosses, in 16 px tile units.
    int tile_x = from_x >> tile_shift;
    int tile_y = from_y >> tile_shift;
    int end_x = to_x >> tile_shift;
    int end_y = to_y >> tile_shift;
    int dx = to_x - from_x;
    int dy = to_y - from_y;
    int step_x = dx > 0 ? 1 : -1;
    int step_y = dy > 0 ? 1 : -1;
    int abs_dx = dx > 0 ? dx : -dx;
    int abs_dy = dy > 0 ? dy : -dy;

    // Distances (scaled by abs_dx * abs_dy) to the next vertical and horizontal tile edges.
    int next_x = step_x > 0 ? ((tile_x + 1) << tile_shift) - from_x : from_x - (tile_x << tile_shift);
    int next_y = step_y > 0 ? ((tile_y + 1) << tile_shift) - from_y : from_y - (tile_y << tile_shift);
    int t_max_x = abs_dx ? next_x * abs_dy : 0x7FFFFFFF;
    int t_max_y = abs_dy ? next_y * abs_dx : 0x7FFFFFFF;
    int t_delta_x = tile_size * abs_dy;
    int t_delta_y = tile_size * abs_dx;

    for(int guard = 0; guard < 64; ++guard)
    {
        if(tile_x == end_x && tile_y == end_y)
        {
            return true;
        }

        if(t_max_x < t_max_y)
        {
            tile_x += step_x;
            t_max_x += t_delta_x;
        }
        else
        {
            tile_y += step_y;
            t_max_y += t_delta_y;
        }

        if(tile_x == end_x && tile_y == end_y)
        {
            return true;
        }

        // Outside the world and in chunks not generated yet, tile_flags() blocks sight.
        if(detail::tile_flags(tile_x, tile_y) & detail::opaque_flag)
        {
            return false;
        }
    }

    return false;
}

}
