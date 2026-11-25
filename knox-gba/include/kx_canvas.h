#ifndef KX_CANVAS_H
#define KX_CANVAS_H

#include "bn_color.h"
#include "bn_common.h"
#include "bn_regular_bg_map_ptr.h"
#include "bn_regular_bg_ptr.h"
#include "bn_regular_bg_tiles_ptr.h"

#include "kx_bench.h"
#include "kx_hud_palette.h"
#include "kx_item_icons.h"
#include "kx_small_font.h"

namespace kx
{

// Pixel area on the screen.
struct area
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    [[nodiscard]] bool empty() const
    {
        return width <= 0 || height <= 0;
    }

    [[nodiscard]] bool intersects(const area& other) const
    {
        return ! empty() && ! other.empty() && x < other.x + other.width && other.x < x + width &&
               y < other.y + other.height && other.y < y + height;
    }
};

// BG0 as a 240x160 pixel surface: every visible 8x8 cell owns one 4bpp tile, written in VRAM directly, so text
// (the 5 px small font), 1 px frames, bars and icons sit at any pixel like the mockups. Colours are
// kx::colour values: shared indices keep a cell's palette bank, bank colours switch it (see kx_hud_palette.h).
// Pixel 0 is transparent, so the world shows wherever nothing is drawn.
class canvas
{

public:
    static constexpr int columns = 30;
    static constexpr int rows = 20;
    static constexpr int width = columns * 8;
    static constexpr int height = rows * 8;
    static constexpr int line_height = 7;          // text rows in multi-line boxes
    static constexpr int first_tile = 1;           // tile 0 stays transparent for the map cells off screen
    static constexpr int map_columns = 32;

    // Menu screens' background: see-through, so the dimmed world shows behind their panels (playtest 1, mockup 03).
    static constexpr int backdrop = 0;

    canvas();

    [[nodiscard]] bn::regular_bg_ptr& bg()
    {
        return _bg;
    }

    // Everything transparent.
    void clear();

    // Swaps a palette bank's own 8 colours (indices 8-15) for others, for a screen that needs more colours than
    // the banks give it (all 16 background banks are used); restore_colours puts the drawn palette back.
    void set_bank_colours(int bank, const bn::color* colours);
    void restore_colours();

    // Transparent rectangle.
    void erase(const area& region);

    void rect(int x, int y, int rect_width, int rect_height, int colour);
    void frame(int x, int y, int frame_width, int frame_height, int colour);

    // Filled rectangle with a 1 px border.
    void panel(int x, int y, int panel_width, int panel_height, int fill_colour, int border_colour)
    {
        rect(x, y, panel_width, panel_height, fill_colour);
        frame(x, y, panel_width, panel_height, border_colour);
    }

    // Left / right / centre aligned text (capitals, '`' a middle dot, '^' a degree sign); returns the pixel
    // after its right end.
    int text(int x, int y, const char* value, int colour);
    int text_right(int right_x, int y, const char* value, int colour);
    int text_centered(int center_x, int y, const char* value, int colour);
    [[nodiscard]] static int text_width(const char* value);

    // A list's scroll marks (playtest 6): an up and a down triangle (5x3 each) ending at right_x, side by side on a
    // heading line at y; each in the text colour while more rows lie that way, else in the border colour.
    void scroll_marks(int right_x, int y, bool more_above, bool more_below);

    // A tick box (5x5) at (x, y): an outline when not ticked, a tick in it when ticked.
    void tick_box(int x, int y, bool ticked, int colour);

    // Part of a 16x16 icon (source rectangle in icon pixels) at (x, y).
    void icon(int x, int y, item_icon value, int source_x = 0, int source_y = 0, int source_width = 16,
              int source_height = 16);

    #if KX_DUMP
        // Logs the cell banks and tile pixels (KXDUMP lines) for tools/render_hud_dump.py.
        void dump(int frame);
    #endif

private:
    bn::regular_bg_tiles_ptr _tiles;
    bn::regular_bg_map_ptr _map;
    bn::regular_bg_ptr _bg;
    bn::regular_bg_map_cell* _cells = nullptr;
    bn::tile* _pixels = nullptr;
    int _cell_base = 0;
    uint8_t _banks[columns * rows] = {};

    [[nodiscard]] static int glyph_index(char c)
    {
        if(c >= 'a' && c <= 'z')
        {
            c = char(c - 'a' + 'A');
        }

        int index = c - small_font::first;
        return index >= 0 && index < small_font::count ? index : '?' - small_font::first;
    }

    // Pixel and map loops (ARM code in IWRAM, kx_canvas.bn_iwram.cpp); coordinates are already on screen.
    BN_CODE_IWRAM void _set_bank(int x0, int y0, int x1, int y1, int colour);
    BN_CODE_IWRAM void _write_cell(int column, int row);
    BN_CODE_IWRAM static void _fill(bn::tile* tiles, int x0, int y0, int x1, int y1, int index);
    BN_CODE_IWRAM static void _glyphs(bn::tile* tiles, int x, int y, const char* value, int index);
    BN_CODE_IWRAM static void _blit(bn::tile* tiles, int x, int y, const uint32_t (*source)[2], int source_x,
                                    int source_width, int source_height);
};

}

#endif
