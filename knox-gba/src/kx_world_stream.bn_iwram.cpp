#include "kx_world_stream.h"

#include "kx_world_detail.h"

namespace kx
{

namespace
{
    constexpr unsigned horizontal_flip = 1 << 10;
    constexpr unsigned vertical_flip = 1 << 11;

    // Bits for a cell's random flips: the same cell always flips the same way, so redraws match.
    [[nodiscard]] inline unsigned cell_bits(int x, int y)
    {
        unsigned value = (unsigned(x) * 0x9e3779b1u) ^ (unsigned(y) * 0x85ebca77u);
        return value ^ (value >> 15);
    }
}

void stream_cells(const stream_target& target, int cell_x, int cell_y, int columns, int rows)
{
    using namespace world::detail;

    uint16_t* vram = target.vram;
    const uint8_t* banks = target.banks;
    unsigned tiles_offset = target.tiles_offset;
    unsigned banks_offset = target.banks_offset;
    unsigned outside = target.outside_gfx;
    int layer = target.layer;

    for(int y = cell_y, y_end = cell_y + rows; y < y_end; ++y)
    {
        bool row_hidden = y >= target.hide_top && y < target.hide_bottom;
        int tile_y = y >> 1;
        unsigned quadrant_y = unsigned(y & 1) << 1;
        int row_offset_in_chunk = (tile_y & (world::chunk_tiles - 1)) * world::chunk_tiles;

        // A 32x32 map is one screenblock (milestone 14).
        unsigned row_offset = (unsigned(y) & 31) * 32;

        // The chunk row is looked up once per chunk the row crosses.
        int current_chunk_x = -0x7FFF;
        const uint8_t* gfx_row = nullptr;
        const uint8_t* style_row = nullptr;

        for(int x = cell_x, x_end = cell_x + columns; x < x_end; ++x)
        {
            int tile_x = x >> 1;
            int chunk_x = tile_x >> world::chunk_shift;

            if(chunk_x != current_chunk_x)
            {
                current_chunk_x = chunk_x;
                const chunk_slot* slot = ready_slot(chunk_x, tile_y >> world::chunk_shift);
                gfx_row = slot ? slot->gfx[layer] + row_offset_in_chunk : nullptr;
                style_row = slot ? slot->style + row_offset_in_chunk : nullptr;
            }

            int column = tile_x & (world::chunk_tiles - 1);
            unsigned graphic = gfx_row ? gfx_row[column] : outside;

            if(row_hidden && x >= target.hide_left && x < target.hide_right)
            {
                graphic = unsigned(world_gfx::EMPTY);
            }

            unsigned bank = banks[graphic];

            // Building colours: the variant of this graphic's kind (roof, wall, floor) in the tile's style.
            if(unsigned paint = world_gfx_paints[graphic])
            {
                unsigned variant = (unsigned(style_row[column]) >> ((paint - 1) * style_bits)) & style_mask;

                if(variant)
                {
                    bank = world_paint_banks[paint - 1][variant];
                }
            }

            // Random flips break up repeats: 8x8 pieces sideways or both ways, or the whole tile mirrored (per
            // column, so a tree's trunk and canopies agree, or per tile for corpses).
            unsigned quadrant_x = unsigned(x) & 1;
            unsigned flips = 0;

            switch(world_gfx_flips[graphic])
            {

            case 1:
                flips = cell_bits(x, y) & horizontal_flip;
                break;

            case 2:
                flips = cell_bits(x, y) & (horizontal_flip | vertical_flip);
                break;

            case 3:
                if(cell_bits(tile_x, 0x5eed) & (1 << 20))
                {
                    quadrant_x ^= 1;
                    flips = horizontal_flip;
                }
                break;

            case 4:
                if(world::decal_mirrored(tile_x, tile_y))
                {
                    quadrant_x ^= 1;
                    flips = horizontal_flip;
                }
                break;

            default:
                break;
            }

            unsigned index = row_offset + (unsigned(x) & 31);
            unsigned tile = tiles_offset + (graphic * 4) + quadrant_y + quadrant_x;
            vram[index] = uint16_t(tile | flips | ((banks_offset + bank) << 12));
        }
    }
}

}
