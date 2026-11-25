#ifndef KX_WORLD_STREAM_H
#define KX_WORLD_STREAM_H

#include "bn_common.h"

namespace kx
{

struct stream_target
{
    uint16_t* vram;             // 32x32 cell map (one hardware screenblock)
    const uint8_t* banks;       // palette bank per graphic (world_gfx_banks)
    uint16_t tiles_offset;
    uint16_t banks_offset;
    uint8_t layer;              // world::layer
    uint8_t outside_gfx;        // graphic drawn outside the world and in chunks not generated yet
    // Cells drawn empty (world cell coordinates, right and bottom exclusive): at night the roof of the building
    // the player is in (an empty rectangle hides nothing).
    int16_t hide_left = 0;
    int16_t hide_top = 0;
    int16_t hide_right = 0;
    int16_t hide_bottom = 0;
};

// Writes the cells of the given hardware-tile rectangle (world cell coordinates, 8 px each) for one layer, reading
// the graphics from the chunk cache.
BN_CODE_IWRAM void stream_cells(const stream_target& target, int cell_x, int cell_y, int columns, int rows);

}

#endif
