#include "kx_fixtures.h"

#include "kx_world.h"

namespace kx::fixtures
{

fixture at(int tile_x, int tile_y)
{
    loot_table table = world::container_table(tile_x, tile_y);

    if(table != loot_table::KITCHEN && table != loot_table::BATHROOM)
    {
        return fixture::NONE;
    }

    fixture first = table == loot_table::KITCHEN ? fixture::KITCHEN_SINK : fixture::BATHROOM_SINK;
    int left;
    int top;
    int width;
    int height;

    if(! world::building_over(tile_x, tile_y, left, top, width, height))
    {
        return first;
    }

    // The cabinets of this kind in the building, in reading order: the first is the sink, the second the stove.
    int rank = 0;

    for(int y = top; y < top + height; ++y)
    {
        for(int x = left; x < left + width; ++x)
        {
            if(world::container_table(x, y) == table)
            {
                if(x == tile_x && y == tile_y)
                {
                    return rank == 0 ? first : rank == 1 && table == loot_table::KITCHEN ? fixture::STOVE :
                                                                                          fixture::NONE;
                }

                ++rank;
            }
        }
    }

    return fixture::NONE;
}

const char* name(fixture value)
{
    switch(value)
    {

    case fixture::KITCHEN_SINK:
        return "KITCHEN SINK";

    case fixture::STOVE:
        return "STOVE";

    case fixture::BATHROOM_SINK:
        return "BATHROOM SINK";

    default:
        return nullptr;
    }
}

}
