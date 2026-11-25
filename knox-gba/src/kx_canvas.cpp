#include "kx_canvas.h"

#include "bn_algorithm.h"
#include "bn_bg_palette_ptr.h"
#include "bn_display.h"
#include "bn_tile.h"

#if KX_DUMP
    #include "bn_log.h"
    #include "bn_string_view.h"
#endif

#include "bn_bg_palette_items_hud_palette.h"

#include "kx_icon_pixels.h"
#include "kx_small_font.h"

namespace kx
{

canvas::canvas() :
    _tiles(bn::regular_bg_tiles_ptr::allocate(first_tile + (columns * rows), bn::bpp_mode::BPP_4, false)),
    _map(bn::regular_bg_map_ptr::allocate(bn::size(map_columns, map_columns), _tiles,
                                          bn::bg_palette_items::hud_palette.create_palette())),
    _bg(bn::regular_bg_ptr::create((map_columns * 4) - (bn::display::width() / 2),
                                   (map_columns * 4) - (bn::display::height() / 2), _map))
{
    // The position above puts map cell (0, 0) at the screen's top-left corner.
    _bg.set_priority(0);
    _cell_base = _map.tiles_offset() | (_map.palette_banks_offset() << 12);

    // Allocated VRAM stays put while it is referenced, so the pointers are looked up once.
    bn::span<bn::regular_bg_map_cell> cells = *_map.vram();
    _cells = cells.data();
    _pixels = _tiles.vram()->data();

    for(bn::regular_bg_map_cell& cell : cells)
    {
        cell = bn::regular_bg_map_cell(_cell_base);
    }

    for(int row = 0; row < rows; ++row)
    {
        for(int column = 0; column < columns; ++column)
        {
            _write_cell(column, row);
        }
    }

    clear();
}

void canvas::set_bank_colours(int bank, const bn::color* colours)
{
    constexpr int bank_colours = 8;
    bn::span<const bn::color> drawn = bn::bg_palette_items::hud_palette.colors_ref();
    bn::color swapped[16 * colour::banks];
    int count = drawn.size();

    for(int index = 0; index < count; ++index)
    {
        swapped[index] = drawn[index];
    }

    for(int index = 0; index < bank_colours; ++index)
    {
        swapped[(bank * 16) + bank_colours + index] = colours[index];
    }

    bn::bg_palette_ptr palette = _map.palette();
    palette.set_colors(bn::span<const bn::color>(swapped, count));
}

void canvas::restore_colours()
{
    bn::bg_palette_ptr palette = _map.palette();
    palette.set_colors(bn::bg_palette_items::hud_palette.colors_ref());
}

void canvas::clear()
{
    _fill(_pixels, 0, 0, width - 1, height - 1, 0);
}

void canvas::erase(const area& region)
{
    rect(region.x, region.y, region.width, region.height, 0);
}

void canvas::rect(int x, int y, int rect_width, int rect_height, int colour)
{
    int x0 = bn::max(x, 0);
    int y0 = bn::max(y, 0);
    int x1 = bn::min(x + rect_width, width) - 1;
    int y1 = bn::min(y + rect_height, height) - 1;

    if(x1 < x0 || y1 < y0)
    {
        return;
    }

    _set_bank(x0, y0, x1, y1, colour);
    _fill(_pixels, x0, y0, x1, y1, colour & 15);
}

void canvas::frame(int x, int y, int frame_width, int frame_height, int colour)
{
    rect(x, y, frame_width, 1, colour);
    rect(x, y + frame_height - 1, frame_width, 1, colour);
    rect(x, y + 1, 1, frame_height - 2, colour);
    rect(x + frame_width - 1, y + 1, 1, frame_height - 2, colour);
}

int canvas::text_width(const char* value)
{
    int result = 0;

    for(; *value; ++value)
    {
        result += small_font::widths[glyph_index(*value)] + small_font::spacing;
    }

    return result > 0 ? result - small_font::spacing : 0;
}

int canvas::text(int x, int y, const char* value, int colour)
{
    int text_end = x + text_width(value);

    // Text is only drawn when it fits on screen (layouts keep it there).
    if(x >= 0 && y >= 0 && text_end <= width && y + small_font::height <= height && text_end > x)
    {
        _set_bank(x, y, text_end - 1, y + small_font::height - 1, colour);
        _glyphs(_pixels, x, y, value, colour & 15);
    }

    return text_end;
}

int canvas::text_right(int right_x, int y, const char* value, int colour)
{
    return text(right_x - text_width(value) + 1, y, value, colour);
}

int canvas::text_centered(int center_x, int y, const char* value, int colour)
{
    return text(center_x - (text_width(value) / 2), y, value, colour);
}

void canvas::icon(int x, int y, item_icon value, int source_x, int source_y, int source_width, int source_height)
{
    // Icons are only drawn when they fit on screen (layouts keep them there).
    if(x < 0 || y < 0 || x + source_width > width || y + source_height > height)
    {
        return;
    }

    _set_bank(x, y, x + source_width - 1, y + source_height - 1, (icon_banks[int(value)] << 4) | 8);
    _blit(_pixels, x, y, icon_pixels[int(value)] + source_y, source_x, source_width, source_height);
}

#if KX_DUMP
    void canvas::dump(int frame)
    {
        constexpr char digits[] = "0123456789ABCDEF";
        BN_LOG("KXDUMP begin ", frame);

        for(int row = 0; row < rows; ++row)
        {
            char banks[columns + 1];

            for(int column = 0; column < columns; ++column)
            {
                banks[column] = digits[_banks[(row * columns) + column]];
            }

            banks[columns] = 0;
            BN_LOG("KXDUMP b ", row, " ", bn::string_view(banks));
        }

        // The palette as it is now (a screen may have swapped a bank's colours): 16 colours of 4 hex digits.
        bn::bg_palette_ptr palette = _map.palette();
        bn::span<const bn::color> colours = palette.colors();

        for(int bank = 0; bank < colours.size() / 16; ++bank)
        {
            char hex[16 * 4 + 1];

            for(int index = 0; index < 16; ++index)
            {
                int value = colours[(bank * 16) + index].data();

                for(int digit = 0; digit < 4; ++digit)
                {
                    hex[(index * 4) + digit] = digits[(value >> ((3 - digit) * 4)) & 15];
                }
            }

            hex[16 * 4] = 0;
            BN_LOG("KXDUMP p ", bank, " ", bn::string_view(hex));
        }

        const bn::tile* tiles = _pixels;

        for(int index = 0; index < columns * rows; ++index)
        {
            char words[8 * 8 + 1];

            for(int line = 0; line < 8; ++line)
            {
                uint32_t value = tiles[first_tile + index].data[line];

                for(int nibble = 0; nibble < 8; ++nibble)
                {
                    words[(line * 8) + nibble] = digits[(value >> ((7 - nibble) * 4)) & 15];
                }
            }

            words[64] = 0;
            BN_LOG("KXDUMP t ", index, " ", bn::string_view(words));
        }

        BN_LOG("KXDUMP end ", frame);
    }
#endif

}
