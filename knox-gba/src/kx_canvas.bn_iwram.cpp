#include "kx_canvas.h"

#include "bn_tile.h"

#include "kx_small_font.h"

namespace kx
{

namespace
{
    // Writes `count` (1-8) pixels of `pixels` (4bpp, first pixel in the low nibble) at (x, y), inside one cell.
    [[gnu::always_inline]] inline void put_run(bn::tile* tiles, int x, int y, uint32_t pixels, int count)
    {
        int shift = (x & 7) * 4;
        uint32_t mask = (count >= 8 ? 0xFFFFFFFFu : (1u << (count * 4)) - 1) << shift;
        uint32_t& word = tiles[canvas::first_tile + ((y >> 3) * canvas::columns) + (x >> 3)].data[y & 7];
        word = (word & ~mask) | ((pixels << shift) & mask);
    }
}

void canvas::_write_cell(int column, int row)
{
    int value = _cell_base + first_tile + (row * columns) + column + (_banks[(row * columns) + column] << 12);
    _cells[(row * map_columns) + column] = bn::regular_bg_map_cell(value);
}

void canvas::_set_bank(int x0, int y0, int x1, int y1, int colour)
{
    if((colour & 15) < 8)
    {
        return;
    }

    int bank = colour >> 4;

    for(int row = y0 >> 3; row <= (y1 >> 3); ++row)
    {
        for(int column = x0 >> 3; column <= (x1 >> 3); ++column)
        {
            uint8_t& current = _banks[(row * columns) + column];

            if(current != bank)
            {
                current = uint8_t(bank);
                _write_cell(column, row);
            }
        }
    }
}

void canvas::_fill(bn::tile* tiles, int x0, int y0, int x1, int y1, int index)
{
    // Cell by cell: covered lines of a cell are plain stores, partial ones read-modify-write.
    uint32_t pattern = uint32_t(index) * 0x11111111u;

    for(int row = y0 >> 3; row <= (y1 >> 3); ++row)
    {
        int first_line = y0 > (row * 8) ? y0 - (row * 8) : 0;
        int last_line = y1 < (row * 8) + 7 ? y1 - (row * 8) : 7;

        for(int column = x0 >> 3; column <= (x1 >> 3); ++column)
        {
            int first = x0 > (column * 8) ? x0 - (column * 8) : 0;
            int last = x1 < (column * 8) + 7 ? x1 - (column * 8) : 7;
            uint32_t mask = (last == 7 ? 0xFFFFFFFFu : (1u << ((last + 1) * 4)) - 1) & ~((1u << (first * 4)) - 1);
            uint32_t* words = tiles[first_tile + (row * columns) + column].data;

            if(mask == 0xFFFFFFFFu && first_line == 0 && last_line == 7)
            {
                words[0] = pattern;
                words[1] = pattern;
                words[2] = pattern;
                words[3] = pattern;
                words[4] = pattern;
                words[5] = pattern;
                words[6] = pattern;
                words[7] = pattern;
            }
            else if(mask == 0xFFFFFFFFu)
            {
                for(int line = first_line; line <= last_line; ++line)
                {
                    words[line] = pattern;
                }
            }
            else
            {
                for(int line = first_line; line <= last_line; ++line)
                {
                    words[line] = (words[line] & ~mask) | (pattern & mask);
                }
            }
        }
    }
}

void canvas::_glyphs(bn::tile* tiles, int x, int y, const char* value, int index)
{
    uint32_t pattern = uint32_t(index) * 0x11111111u;

    for(; *value; ++value)
    {
        int glyph = glyph_index(*value);
        const uint32_t* glyph_rows = small_font::rows[glyph];
        int shift = (x & 7) * 4;
        bn::tile* tile = tiles + first_tile + (x >> 3);

        for(int line = 0; line < small_font::height; ++line)
        {
            uint32_t mask = glyph_rows[line];

            if(! mask)
            {
                continue;
            }

            int py = y + line;
            bn::tile* row_tile = tile + ((py >> 3) * columns);
            uint32_t low = mask << shift;
            uint32_t& word = row_tile->data[py & 7];
            word = (word & ~low) | (pattern & low);

            if(shift)
            {
                uint32_t high = mask >> (32 - shift);

                if(high)
                {
                    uint32_t& next = row_tile[1].data[py & 7];
                    next = (next & ~high) | (pattern & high);
                }
            }
        }

        x += small_font::widths[glyph] + small_font::spacing;
    }
}

void canvas::_blit(bn::tile* tiles, int x, int y, const uint32_t (*source)[2], int source_x, int source_width,
                   int source_height)
{
    if(! (x & 7) && ! source_x && source_width == 16)
    {
        // Cell-aligned whole icon (gear and search slots): two plain stores per line.
        for(int line = 0; line < source_height; ++line)
        {
            int py = y + line;
            bn::tile* tile = tiles + first_tile + ((py >> 3) * columns) + (x >> 3);
            tile[0].data[py & 7] = source[line][0];
            tile[1].data[py & 7] = source[line][1];
        }

        return;
    }

    for(int line = 0; line < source_height; ++line)
    {
        uint64_t pixels = ((uint64_t(source[line][1]) << 32) | source[line][0]) >> (source_x * 4);
        int remaining = source_width;
        int px = x;

        while(remaining > 0)
        {
            int count = 8 - (px & 7);

            if(count > remaining)
            {
                count = remaining;
            }

            put_run(tiles, px, y + line, uint32_t(pixels), count);
            pixels >>= count * 4;
            px += count;
            remaining -= count;
        }
    }
}

}
