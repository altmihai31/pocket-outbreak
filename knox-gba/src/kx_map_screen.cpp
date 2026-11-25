#include "kx_map_screen.h"

#include "bn_algorithm.h"
#include "bn_color.h"

#include "kx_base.h"
#include "kx_game.h"
#include "kx_input.h"
#include "kx_macro_map.h"
#include "kx_sfx.h"
#include "kx_ui.h"
#include "kx_world_map.h"

namespace kx
{

namespace
{
    // Mockup 05 layout in screen pixels. The map's area starts and ends on 8x8 cell edges, so its cells hold only
    // the map's palette bank and shared colours (kx_hud_palette.h): 17 map cells of 12 px, plus a sliver of an 18th
    // to fill the last 4 px. Playtest 1: the map ends where the other tabs' panels do (x 224-228). Playtest 4: the
    // world's sandbox settings moved to the MENU tab's WORLD page, so the map takes the width (it began at x 48).
    constexpr int map_left = 16;
    constexpr int map_top = 24;
    constexpr int map_width = 208;
    constexpr int map_height = 120;
    constexpr int map_right = map_left + map_width;
    constexpr int map_bottom = map_top + map_height;
    constexpr int cell_px = 12;
    constexpr int chunk_px = cell_px / 2;
    constexpr int view_cells_x = 18;            // the 18th only in part
    constexpr int view_centre_x = 8;            // the cell in the middle of the view (x 112-124 of 120)
    constexpr int view_cells_y = map_height / cell_px;
    constexpr int footer_y = 147;
    constexpr int footer_top = 145;

    // The map's colours, put into the Health tab's bank while the Map tab is open (all 16 background banks are in
    // use). Mockup 05: water and its ripples, field sand and its dots, woods and forest, roads, the player.
    constexpr int map_bank = colour::stats_bank;
    constexpr bn::color map_colours[8] = {
        bn::color(0x8f >> 3, 0xb4 >> 3, 0xc4 >> 3),     // water
        bn::color(0x6f >> 3, 0x98 >> 3, 0xad >> 3),     // ripples
        bn::color(0xc8 >> 3, 0xbc >> 3, 0x8a >> 3),     // sand
        bn::color(0xb3 >> 3, 0xa6 >> 3, 0x74 >> 3),     // sand, darker
        bn::color(0x6f >> 3, 0x8a >> 3, 0x55 >> 3),     // woods
        bn::color(0x4f >> 3, 0x6a >> 3, 0x3d >> 3),     // forest
        bn::color(0xa0 >> 3, 0x52 >> 3, 0x2d >> 3),     // road
        bn::color(0xd0 >> 3, 0x45 >> 3, 0x3a >> 3)      // the player's marker
    };

    constexpr int water = (map_bank << 4) | 8;
    constexpr int ripples = (map_bank << 4) | 9;
    constexpr int sand = (map_bank << 4) | 10;
    constexpr int sand_dark = (map_bank << 4) | 11;
    constexpr int woods = (map_bank << 4) | 12;
    constexpr int forest = (map_bank << 4) | 13;
    constexpr int road = (map_bank << 4) | 14;
    constexpr int marker = (map_bank << 4) | 15;

    // Unexplored ground: dark, striped every 3 px (shared colours).
    constexpr int fog_fill = colour::empty;
    constexpr int fog_stripe = colour::border;

    // Holding the D-pad: the first step at once, then one every 3 frames after 14.
    constexpr int repeat_delay = 14;
    constexpr int repeat_step = 3;

    // Where fill() may draw: the map's area, or one chunk of it while the land is drawn chunk by chunk.
    int clip_left = map_left;
    int clip_top = map_top;
    int clip_right = map_right;
    int clip_bottom = map_bottom;

    void set_clip(int left, int top, int right, int bottom)
    {
        clip_left = bn::max(left, map_left);
        clip_top = bn::max(top, map_top);
        clip_right = bn::min(right, map_right);
        clip_bottom = bn::min(bottom, map_bottom);
    }

    void reset_clip()
    {
        set_clip(map_left, map_top, map_right, map_bottom);
    }

    // A rectangle cut to the clip area.
    void fill(canvas& pixels, int x, int y, int width, int height, int colour)
    {
        int x0 = bn::max(x, clip_left);
        int y0 = bn::max(y, clip_top);
        int x1 = bn::min(x + width, clip_right);
        int y1 = bn::min(y + height, clip_bottom);

        if(x1 > x0 && y1 > y0)
        {
            pixels.rect(x0, y0, x1 - x0, y1 - y0, colour);
        }
    }

    void draw_cell(canvas& pixels, int cell_x, int cell_y, int x, int y)
    {
        // The land, a few marks of texture, then the zone's look.
        switch(macro::land_at(cell_x, cell_y))
        {

        case macro::land::WATER:
            fill(pixels, x, y, cell_px, cell_px, water);
            fill(pixels, x + 1, y + 3, 4, 1, ripples);
            fill(pixels, x + 7, y + 8, 4, 1, ripples);
            break;

        case macro::land::COAST:
            fill(pixels, x, y, cell_px, cell_px, sand);
            break;

        case macro::land::WOODS:
            fill(pixels, x, y, cell_px, cell_px, woods);
            fill(pixels, x + 2, y + 2, 2, 2, forest);
            fill(pixels, x + 7, y + 7, 2, 2, forest);
            break;

        case macro::land::FOREST:
            fill(pixels, x, y, cell_px, cell_px, forest);
            fill(pixels, x + 3, y + 3, 2, 2, woods);
            fill(pixels, x + 8, y + 8, 1, 1, woods);
            break;

        case macro::land::HILLS:
            fill(pixels, x, y, cell_px, cell_px, sand_dark);
            fill(pixels, x + 3, y + 4, 3, 1, sand);
            fill(pixels, x + 7, y + 8, 3, 1, sand);
            break;

        default:
            fill(pixels, x, y, cell_px, cell_px, sand);
            fill(pixels, x + 3, y + 3, 1, 1, sand_dark);
            fill(pixels, x + 8, y + 7, 1, 1, sand_dark);
            break;
        }

        if(macro::zone_at(cell_x, cell_y) == macro::zone::FARM)
        {
            // Rows of crops.
            for(int row = 1; row < cell_px; row += 3)
            {
                fill(pixels, x, y + row, cell_px, 1, sand_dark);
            }
        }

        // Roads leave the cell's middle towards the sides in its mask.
        int roads = macro::roads_at(cell_x, cell_y);

        if(roads)
        {
            constexpr int middle = (cell_px / 2) - 1;
            fill(pixels, x + middle, y + middle, 2, 2, road);

            if(roads & macro::road_north)
            {
                fill(pixels, x + middle, y, 2, middle, road);
            }

            if(roads & macro::road_south)
            {
                fill(pixels, x + middle, y + middle + 2, 2, cell_px - middle - 2, road);
            }

            if(roads & macro::road_west)
            {
                fill(pixels, x, y + middle, middle, 2, road);
            }

            if(roads & macro::road_east)
            {
                fill(pixels, x + middle + 2, y + middle, cell_px - middle - 2, 2, road);
            }
        }
    }

    // Unexplored ground over a chunk: dark, striped every 3 px.
    void draw_fog(canvas& pixels, int x, int y)
    {
        fill(pixels, x, y, chunk_px, chunk_px, fog_fill);
        fill(pixels, x, y, chunk_px, 1, fog_stripe);
        fill(pixels, x, y + 3, chunk_px, 1, fog_stripe);
    }

    // A settlement: four dark blocks round its crossroads (mockup 05).
    void draw_settlement(canvas& pixels, int x, int y)
    {
        fill(pixels, x - 4, y - 4, 3, 3, colour::border);
        fill(pixels, x + 1, y - 4, 3, 3, colour::border);
        fill(pixels, x - 4, y + 1, 3, 3, colour::border);
        fill(pixels, x + 1, y + 1, 3, 3, colour::border);
    }

    // A safehouse: a small house, light so it stands out.
    void draw_safehouse(canvas& pixels, int x, int y)
    {
        fill(pixels, x - 1, y - 4, 3, 1, colour::text);
        fill(pixels, x - 2, y - 3, 5, 1, colour::text);
        fill(pixels, x - 3, y - 2, 7, 1, colour::text);
        fill(pixels, x - 2, y - 1, 5, 4, colour::text);
        fill(pixels, x, y + 1, 1, 2, colour::fill);
    }

    // A mark: a yellow cross on the chunk.
    void draw_mark(canvas& pixels, int x, int y)
    {
        fill(pixels, x - 3, y - 1, 7, 3, colour::fill);
        fill(pixels, x - 1, y - 3, 3, 7, colour::fill);
        fill(pixels, x - 2, y, 5, 1, colour::accent);
        fill(pixels, x, y - 2, 1, 5, colour::accent);
    }

    // Automatic marks (milestone 12.7, mockup 12), in cream: a key, or a sheet of paper for a note.
    void draw_auto_mark(canvas& pixels, int x, int y, world_map::auto_mark kind)
    {
        if(kind == world_map::auto_mark::KEY)
        {
            fill(pixels, x - 4, y - 3, 9, 6, colour::fill);
            fill(pixels, x - 3, y - 2, 3, 3, colour::text);
            fill(pixels, x - 2, y - 1, 1, 1, colour::fill);
            fill(pixels, x, y - 1, 4, 1, colour::text);
            fill(pixels, x + 2, y, 1, 2, colour::text);
        }
        else
        {
            fill(pixels, x - 3, y - 4, 7, 9, colour::fill);
            fill(pixels, x - 2, y - 3, 5, 7, colour::text);
            fill(pixels, x - 1, y - 1, 3, 1, colour::quiet);
            fill(pixels, x - 1, y + 1, 3, 1, colour::quiet);
        }
    }

    // The cursor: four corners round its chunk.
    void draw_cursor(canvas& pixels, int x, int y)
    {
        int left = x - 2;
        int top = y - 2;
        int right = x + chunk_px + 1;
        int bottom = y + chunk_px + 1;
        fill(pixels, left, top, 3, 1, colour::text);
        fill(pixels, left, top, 1, 3, colour::text);
        fill(pixels, right - 2, top, 3, 1, colour::text);
        fill(pixels, right, top, 1, 3, colour::text);
        fill(pixels, left, bottom, 3, 1, colour::text);
        fill(pixels, left, bottom - 2, 1, 3, colour::text);
        fill(pixels, right - 2, bottom, 3, 1, colour::text);
        fill(pixels, right, bottom - 2, 1, 3, colour::text);
    }

    // A label in a dark box, only if it fits inside the map.
    void draw_label(canvas& pixels, int x, int y, const char* text)
    {
        int width = canvas::text_width(text) + 4;

        if(x >= map_left && y >= map_top && x + width <= map_right && y + 9 <= map_bottom)
        {
            pixels.rect(x, y, width, 9, colour::fill);
            pixels.text(x + 2, y + 2, text, colour::text);
        }
    }

    int append_three_digits(char* output, int length, int value)
    {
        output[length++] = char('0' + ((value / 100) % 10));
        output[length++] = char('0' + ((value / 10) % 10));
        output[length++] = char('0' + (value % 10));
        output[length] = 0;
        return length;
    }
}

void map_screen::open(game& owner)
{
    _active = true;
    _dirty = true;
    _repeat = 0;
    hud& screen = owner.hud_ref();
    screen.begin_screen();
    screen.pixels().set_bank_colours(map_bank, map_colours);

    // With a compass the map opens where you are; without one, where it was left (the first time, where you are).
    const player& hero = owner.hero();
    bool compass = hero.items().find(item_id::COMPASS) >= 0;

    if(compass || _cursor_x < 0)
    {
        _cursor_x = (hero.position().x().integer() >> world::tile_shift) >> world_map::chunk_shift;
        _cursor_y = (hero.position().y().integer() >> world::tile_shift) >> world_map::chunk_shift;
        _cursor_x = bn::clamp(_cursor_x, 0, world_map::chunks - 1);
        _cursor_y = bn::clamp(_cursor_y, 0, world_map::chunks - 1);
        _view_x = (_cursor_x / 2) - view_centre_x;
        _view_y = (_cursor_y / 2) - (view_cells_y / 2);
    }

    _follow_cursor();
}

void map_screen::close(game& owner)
{
    if(_active)
    {
        _active = false;
        hud& screen = owner.hud_ref();
        screen.pixels().restore_colours();
        screen.end_screen();
    }
}

void map_screen::update(game& owner, const input_frame& input)
{
    if(input.b_pressed || input.start_pressed)
    {
        close(owner);
        return;
    }

    bool has_map = owner.hero().items().find(item_id::MAP) >= 0;

    if(has_map)
    {
        // The cursor: a step on each press, then repeating while held.
        bool pressed = input.up_pressed || input.down_pressed || input.left_pressed || input.right_pressed;
        bool step = false;

        if(pressed)
        {
            _repeat = 0;
            step = true;
        }
        else if(input.dx || input.dy)
        {
            ++_repeat;
            step = _repeat > repeat_delay && (_repeat - repeat_delay) % repeat_step == 0;
        }
        else
        {
            _repeat = 0;
        }

        if(step)
        {
            _cursor_x = bn::clamp(_cursor_x + input.dx, 0, world_map::chunks - 1);
            _cursor_y = bn::clamp(_cursor_y + input.dy, 0, world_map::chunks - 1);
            _follow_cursor();
            _dirty = true;
        }

        if(input.a_pressed)
        {
            bool done = world_map::toggle_mark(_cursor_x, _cursor_y);
            sfx::play(done ? sfx::id::POP : sfx::id::DENIED, milli(500));
            _dirty = true;
        }
    }

    if(_dirty)
    {
        _draw(owner);
    }
}

void map_screen::_follow_cursor()
{
    // The view keeps the cursor a cell away from its edges, and stays on the map.
    int cell_x = _cursor_x / 2;
    int cell_y = _cursor_y / 2;
    _view_x = bn::clamp(_view_x, cell_x - (view_cells_x - 3), cell_x - 1);
    _view_y = bn::clamp(_view_y, cell_y - (view_cells_y - 2), cell_y - 1);
    // Playtest 2: the view may go half a view past the world's edges (there it is all unexplored), so the cursor
    // can sit in the middle anywhere: it used to stop at the edge, leaving a survivor near one in a corner.
    _view_x = bn::clamp(_view_x, -view_centre_x, macro::cells - 1 - view_centre_x);
    _view_y = bn::clamp(_view_y, -(view_cells_y / 2), macro::cells - 1 - (view_cells_y / 2));
}

void map_screen::_draw(game& owner)
{
    hud& screen = owner.hud_ref();
    canvas& pixels = screen.pixels();
    const player& hero = owner.hero();
    _dirty = false;

    pixels.rect(0, 0, canvas::width, canvas::height, canvas::backdrop);
    screen.tab_bar(4);

    if(hero.items().find(item_id::MAP) < 0)
    {
        // Spec: without a map there is no map screen.
        pixels.panel(map_left, map_top, map_width, map_height, colour::fill, colour::border);
        pixels.text_centered(map_left + (map_width / 2), map_top + (map_height / 2) - 8, "NO MAP", colour::text);
        pixels.text_centered(map_left + (map_width / 2), map_top + (map_height / 2) + 2,
                             "FIND ONE IN HOUSES AND SHOPS", colour::quiet);
        constexpr hud::hint hints[] = { { "B", "BACK" } };
        screen.hints(footer_y, hints, 1, false);
        return;
    }

    int origin_x = map_left - (_view_x * cell_px);
    int origin_y = map_top - (_view_y * cell_px);

    // The land and roads where explored, fog elsewhere. Each chunk is drawn once, as land or as fog: the canvas
    // writes VRAM directly and a redraw takes more than a frame, so land drawn first and fogged after would show
    // the hidden map for a frame every time the view scrolls (playtest 1).
    int first_chunk_x = _view_x * 2;
    int first_chunk_y = _view_y * 2;

    for(int row = 0; row < view_cells_y; ++row)
    {
        for(int column = 0; column < view_cells_x; ++column)
        {
            int cell_x = _view_x + column;
            int cell_y = _view_y + row;
            int x = origin_x + (cell_x * cell_px);
            int y = origin_y + (cell_y * cell_px);
            int explored = 0;

            for(int quarter = 0; quarter < 4; ++quarter)
            {
                explored += world_map::explored((cell_x * 2) + (quarter & 1), (cell_y * 2) + (quarter >> 1));
            }

            if(explored == 4)
            {
                draw_cell(pixels, cell_x, cell_y, x, y);
                continue;
            }

            for(int quarter = 0; quarter < 4; ++quarter)
            {
                int chunk_left = x + ((quarter & 1) * chunk_px);
                int chunk_top = y + ((quarter >> 1) * chunk_px);

                if(world_map::explored((cell_x * 2) + (quarter & 1), (cell_y * 2) + (quarter >> 1)))
                {
                    set_clip(chunk_left, chunk_top, chunk_left + chunk_px, chunk_top + chunk_px);
                    draw_cell(pixels, cell_x, cell_y, x, y);
                    reset_clip();
                }
                else
                {
                    draw_fog(pixels, chunk_left, chunk_top);
                }
            }
        }
    }

    // Settlements: their blocks only over explored chunks.
    for(int index = 0; index < macro::settlement_count(); ++index)
    {
        const macro::settlement& value = macro::settlement_at(index);
        int x = origin_x + (value.cell_x * cell_px) + (value.size * cell_px / 2);
        int y = origin_y + (value.cell_y * cell_px) + (value.size * cell_px / 2);

        if(x + 4 <= map_left || x - 4 >= map_right || y + 4 <= map_top || y - 4 >= map_bottom)
        {
            continue;
        }

        // The blocks span x - 4 .. x + 3: the chunks under that square.
        int chunk_x0 = first_chunk_x + bn::max((x - 4 - map_left) / chunk_px, 0);
        int chunk_x1 = first_chunk_x + ((x + 3 - map_left) / chunk_px);
        int chunk_y0 = first_chunk_y + bn::max((y - 4 - map_top) / chunk_px, 0);
        int chunk_y1 = first_chunk_y + ((y + 3 - map_top) / chunk_px);

        for(int chunk_y = chunk_y0; chunk_y <= chunk_y1; ++chunk_y)
        {
            for(int chunk_x = chunk_x0; chunk_x <= chunk_x1; ++chunk_x)
            {
                if(world_map::explored(chunk_x, chunk_y))
                {
                    int chunk_left = map_left + ((chunk_x - first_chunk_x) * chunk_px);
                    int chunk_top = map_top + ((chunk_y - first_chunk_y) * chunk_px);
                    set_clip(chunk_left, chunk_top, chunk_left + chunk_px, chunk_top + chunk_px);
                    draw_settlement(pixels, x, y);
                }
            }
        }

        reset_clip();
    }

    // Safehouses, names of the settlements visited (their crossroads explored), marks.
    for(int index = 0; index < base_tuning::max_safehouses; ++index)
    {
        int tile_x;
        int tile_y;

        if(base::safehouse_centre(index, tile_x, tile_y))
        {
            draw_safehouse(pixels, origin_x + ((tile_x * cell_px) >> macro::cell_shift),
                           origin_y + ((tile_y * cell_px) >> macro::cell_shift));
        }
    }

    for(int index = 0; index < macro::settlement_count(); ++index)
    {
        const macro::settlement& value = macro::settlement_at(index);
        int centre_chunk_x = (value.cell_x * 2) + value.size;
        int centre_chunk_y = (value.cell_y * 2) + value.size;

        if(world_map::explored(centre_chunk_x, centre_chunk_y))
        {
            draw_label(pixels, origin_x + (value.cell_x * cell_px) + (value.size * cell_px / 2) + 6,
                       origin_y + (value.cell_y * cell_px) + (value.size * cell_px / 2) - 4,
                       world_map::settlement_name(index));
        }
    }

    for(int index = 0; index < world_map::auto_mark_count(); ++index)
    {
        world_map::auto_mark kind;
        uint16_t id;
        int chunk_x;
        int chunk_y;
        world_map::auto_mark_at(index, kind, id, chunk_x, chunk_y);
        draw_auto_mark(pixels, origin_x + (chunk_x * chunk_px) + (chunk_px / 2),
                       origin_y + (chunk_y * chunk_px) + (chunk_px / 2), kind);
    }

    for(int index = 0; index < world_map::mark_count(); ++index)
    {
        int chunk_x;
        int chunk_y;
        world_map::mark(index, chunk_x, chunk_y);
        draw_mark(pixels, origin_x + (chunk_x * chunk_px) + (chunk_px / 2),
                  origin_y + (chunk_y * chunk_px) + (chunk_px / 2));
    }

    // With a compass: where you are, and north.
    if(hero.items().find(item_id::COMPASS) >= 0)
    {
        int tile_x = hero.position().x().integer() >> world::tile_shift;
        int tile_y = hero.position().y().integer() >> world::tile_shift;
        int x = origin_x + ((tile_x * cell_px) >> macro::cell_shift);
        int y = origin_y + ((tile_y * cell_px) >> macro::cell_shift);
        fill(pixels, x - 3, y - 3, 7, 7, colour::text);
        fill(pixels, x - 2, y - 2, 5, 5, marker);

        constexpr int north_x = map_left + 3;
        constexpr int north_y = map_top + 3;
        pixels.panel(north_x, north_y, 11, 16, colour::fill, colour::border);
        pixels.rect(north_x + 5, north_y + 2, 1, 1, colour::text);
        pixels.rect(north_x + 4, north_y + 3, 3, 1, colour::text);
        pixels.rect(north_x + 3, north_y + 4, 5, 1, colour::text);
        pixels.rect(north_x + 5, north_y + 5, 1, 2, colour::text);
        pixels.text(north_x + 3, north_y + 9, "N", colour::text);
    }

    draw_cursor(pixels, map_left + ((_cursor_x - first_chunk_x) * chunk_px),
                map_top + ((_cursor_y - first_chunk_y) * chunk_px));

    // Inside the map's bottom edge: the cursor's chunk and the scale (the footer's right side is the save hint).
    char text[24];
    int length = ui::append(text, 0, "GRID ");
    length = append_three_digits(text, length, _cursor_x);
    length = ui::append(text, length, " ` ");
    append_three_digits(text, length, _cursor_y);
    draw_label(pixels, map_left + 2, map_bottom - 11, text);

    // 1 km: two map cells (128 tiles).
    constexpr int scale_width = cell_px * 2;
    constexpr int scale_box = scale_width + 26;
    constexpr int scale_x = map_right - scale_box - 2;
    constexpr int scale_y = map_bottom - 11;
    pixels.rect(scale_x, scale_y, scale_box, 9, colour::fill);
    pixels.rect(scale_x + 2, scale_y + 3, scale_width, 3, colour::text);
    pixels.rect(scale_x + 3, scale_y + 4, scale_width - 2, 1, colour::fill);
    pixels.text(scale_x + scale_width + 5, scale_y + 2, "1 KM", colour::text);

    // The frame, outside the map's cells.
    pixels.frame(map_left - 1, map_top - 1, map_width + 2, map_height + 2, colour::border);

    pixels.rect(0, footer_top, canvas::width, canvas::height - footer_top, canvas::backdrop);
    constexpr hud::hint hints[] = { { "D-PAD", "MOVE" }, { "A", "MARK" }, { "B", "BACK" } };
    screen.hints(footer_y, hints, 3, false);
}

}
