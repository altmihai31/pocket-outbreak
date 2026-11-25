#include "kx_world_renderer.h"

#include "bn_algorithm.h"
#include "bn_display.h"
#include "bn_math.h"
#include "bn_bg_palette_ptr.h"
#include "bn_bgs_mosaic.h"
#include "bn_blending.h"
#include "bn_rect_window.h"
#include "bn_regular_bg_tiles_ptr.h"
#include "bn_window.h"

#include "bn_bg_palette_items_world_palette.h"
#include "bn_regular_bg_tiles_items_world_tiles.h"

#include "kx_light.h"
#include "kx_world_stream.h"

namespace kx
{

namespace
{
    constexpr int visible_columns = (bn::display::width() / 8) + 1;
    constexpr int visible_rows = (bn::display::height() / 8) + 1;

    // Ground draws first (BG3), canopy last (BG1); BG0 belongs to the HUD.
    constexpr int layer_priorities[] = { 3, 2, 1 };
    constexpr world_gfx layer_outside_gfx[] = { world_gfx::DEEP_WATER, world_gfx::EMPTY, world_gfx::EMPTY };

    // Spec: Towns and buildings > Roofs on BG1.
    constexpr int roof_fade_frames = 8;

    // How far the world fades to black behind a menu screen.
    constexpr int menu_dim_percent = 70;

    // Butano's restore() shows every layer and the sprites again but leaves the window's blending flag as it was,
    // so a window once set to no blending stayed that way (the fog haze and the night fade would then miss it).
    void restore_window(bn::window value)
    {
        value.restore();
        value.set_show_blending(true);
    }
}

world_renderer::world_renderer(const bn::camera_ptr& camera) :
    _camera(camera),
    _roof_level(roof_fade_frames)
{
    bn::regular_bg_tiles_ptr tiles = bn::regular_bg_tiles_items::world_tiles.create_tiles(false);
    bn::bg_palette_ptr palette = bn::bg_palette_items::world_palette.create_palette();
    _palette = palette;

    for(int index = 0; index < int(world::layer::COUNT); ++index)
    {
        bn::regular_bg_map_ptr map = bn::regular_bg_map_ptr::allocate(bn::size(map_cells, map_cells), tiles, palette);

        // A BG at (128, 128) puts map cell (x mod 32, y mod 32) under world pixel (x * 8, y * 8).
        bn::regular_bg_ptr bg = bn::regular_bg_ptr::create(map_cells * 4, map_cells * 4, map);
        bg.set_priority(layer_priorities[index]);
        bg.set_camera(camera);

        bn::span<bn::regular_bg_map_cell> vram = *map.vram();
        auto empty_cell = bn::regular_bg_map_cell(map.tiles_offset() | (map.palette_banks_offset() << 12));

        for(bn::regular_bg_map_cell& cell : vram)
        {
            cell = empty_cell;
        }

        _layers.push_back(layer_bg{ bn::move(map), bn::move(bg) });
    }
}

void world_renderer::set_backdrop(bn::fixed mosaic_stretch)
{
    bn::bgs_mosaic::set_stretch(mosaic_stretch);
    bn::blending::set_black_fade_color();

    for(layer_bg& layer : _layers)
    {
        layer.bg.set_mosaic_enabled(true);
        layer.bg.set_blending_enabled(true);
    }
}

void world_renderer::update(int camera_left, int camera_top)
{
    int cell_x = camera_left >> 3;
    int cell_y = camera_top >> 3;

    if(! _drawn || bn::abs(cell_x - _cell_x) >= visible_columns || bn::abs(cell_y - _cell_y) >= visible_rows)
    {
        _draw_cells(cell_x, cell_y, visible_columns, visible_rows);
        _drawn = true;
    }
    else
    {
        if(cell_x > _cell_x)
        {
            _draw_cells(_cell_x + visible_columns, cell_y, cell_x - _cell_x, visible_rows);
        }
        else if(cell_x < _cell_x)
        {
            _draw_cells(cell_x, cell_y, _cell_x - cell_x, visible_rows);
        }

        if(cell_y > _cell_y)
        {
            _draw_cells(cell_x, _cell_y + visible_rows, visible_columns, cell_y - _cell_y);
        }
        else if(cell_y < _cell_y)
        {
            _draw_cells(cell_x, cell_y, visible_columns, _cell_y - cell_y);
        }
    }

    _cell_x = cell_x;
    _cell_y = cell_y;
}

world_renderer::~world_renderer()
{
    _end_night();
    _end_cutaway();
}

void world_renderer::update_effects(const world_effects& value)
{
    if(value.darkness > 0)
    {
        _update_night(value);
        return;
    }

    if(_night)
    {
        _end_night();

        // Dawn while indoors: the roof was not drawn, so the day's cut-away starts with it fully faded out.
        if(value.inside)
        {
            _roof_left = value.left;
            _roof_top = value.top;
            _roof_right = value.left + value.width;
            _roof_bottom = value.top + value.height;
            _roof_level = 0;
        }
    }

    _update_roof(value.inside, value.left, value.top, value.width, value.height);
    _update_haze(value.haze);
}

void world_renderer::_update_night(const world_effects& value)
{
    // The day's effects make way: the roof cut-away's transparency and the fog's white haze use the same
    // hardware blending as the night's fade.
    _end_cutaway();

    if(! _night)
    {
        _night = true;
        _hazed = false;
        bn::blending::set_black_fade_color();

        for(layer_bg& layer : _layers)
        {
            layer.bg.set_blending_enabled(true);
        }

        // Light windows: inside the window sprites the fade is off (everything else shows as outside them).
        bn::window::sprites().set_show_blending(false);
    }

    // A lightning flash lifts the night to a quarter of its darkness for its few frames.
    int steps = value.flash ? value.darkness / 4 : value.darkness;
    bn::blending::set_fade_alpha(bn::fixed(steps) / light::full_steps);

    // The roof of the building the player is in is not drawn; the day's position of the fade follows, so a dawn
    // indoors carries on without the roof.
    if(value.inside)
    {
        _hide_roof(value.left >> 3, value.top >> 3, (value.left + value.width) >> 3,
                   (value.top + value.height) >> 3);
        _roof_left = value.left;
        _roof_top = value.top;
        _roof_right = value.left + value.width;
        _roof_bottom = value.top + value.height;
        _roof_level = 0;
    }
    else
    {
        _hide_roof(0, 0, 0, 0);
        _roof_level = roof_fade_frames;
    }

    // The building's lights: a window over it with the fade off (it wins over the light window sprites there).
    bool lit = value.inside && value.lit_inside;

    if(lit)
    {
        bn::rect_window room = bn::rect_window::internal();

        if(! _room_lit)
        {
            room.set_camera(_camera);
            room.set_show_blending(false);
            _room_lit = true;
        }

        room.set_boundaries(value.top, value.left, value.top + value.height, value.left + value.width);
    }
    else if(_room_lit)
    {
        restore_window(bn::rect_window::internal());
        _room_lit = false;
    }
}

void world_renderer::_end_night()
{
    if(! _night)
    {
        return;
    }

    _night = false;

    for(layer_bg& layer : _layers)
    {
        layer.bg.set_blending_enabled(false);
    }

    bn::blending::set_fade_alpha(0);
    restore_window(bn::window::sprites());

    if(_room_lit)
    {
        restore_window(bn::rect_window::internal());
        _room_lit = false;
    }

    _hide_roof(0, 0, 0, 0);
}

void world_renderer::_hide_roof(int left, int top, int right, int bottom)
{
    if(left == _hide_left && top == _hide_top && right == _hide_right && bottom == _hide_bottom)
    {
        return;
    }

    // Redraw the canopy where the old rectangle was (the roof comes back), then where the new one is.
    int old_left = _hide_left;
    int old_top = _hide_top;
    int old_right = _hide_right;
    int old_bottom = _hide_bottom;
    _hide_left = left;
    _hide_top = top;
    _hide_right = right;
    _hide_bottom = bottom;

    int canopy = int(world::layer::CANOPY);
    _redraw_rect(old_left, old_top, old_right, old_bottom, canopy);
    _redraw_rect(left, top, right, bottom, canopy);
}

void world_renderer::_redraw_rect(int left, int top, int right, int bottom, int only_layer)
{
    // Only the cells in the streamed window: the others are drawn when they scroll in, and in the wraparound map
    // they may alias visible cells.
    if(! _drawn)
    {
        return;
    }

    int x0 = bn::max(left, _cell_x);
    int y0 = bn::max(top, _cell_y);
    int x1 = bn::min(right, _cell_x + visible_columns);
    int y1 = bn::min(bottom, _cell_y + visible_rows);

    if(x1 > x0 && y1 > y0)
    {
        _draw_cells(x0, y0, x1 - x0, y1 - y0, only_layer);
    }
}

void world_renderer::_begin_cutaway()
{
    // Blending on the canopy layer, but only inside the window over the building: every other window (the
    // second rectangle and the sprite window are switched on with it) and the outside keep the canopies solid.
    bn::rect_window inside_window = bn::rect_window::internal();
    inside_window.set_camera(_camera);
    inside_window.set_boundaries(_roof_top, _roof_left, _roof_bottom, _roof_right);
    bn::rect_window::external().set_show_blending(false);
    bn::window::sprites().set_show_blending(false);
    bn::window::outside().set_show_blending(false);
    _layers[int(world::layer::CANOPY)].bg.set_blending_enabled(true);
    _cutaway = true;

    // Fog haze makes way for the roof fade (one blending mode at a time).
    if(_hazed)
    {
        for(int index = 0; index < int(world::layer::COUNT); ++index)
        {
            if(index != int(world::layer::CANOPY))
            {
                _layers[index].bg.set_blending_enabled(false);
            }
        }

        bn::blending::set_fade_alpha(0);
        _hazed = false;
    }
}

void world_renderer::_update_roof(bool inside, int left, int top, int width, int height)
{
    // Entering another building moves the window there at once (the old roof pops back).
    if(inside && (left != _roof_left || top != _roof_top || left + width != _roof_right ||
                  top + height != _roof_bottom))
    {
        _roof_left = left;
        _roof_top = top;
        _roof_right = left + width;
        _roof_bottom = top + height;
        _roof_level = roof_fade_frames;

        if(_cutaway)
        {
            bn::rect_window::internal().set_boundaries(_roof_top, _roof_left, _roof_bottom, _roof_right);
        }
    }

    int target = inside ? 0 : roof_fade_frames;

    if(_roof_level == target)
    {
        if(! inside)
        {
            _end_cutaway();
        }
        else if(! _cutaway)
        {
            // Dawn indoors: the roof is already gone, so the cut-away starts at its end.
            _begin_cutaway();
            bn::blending::set_transparency_alpha(bn::fixed(_roof_level) / roof_fade_frames);
        }

        return;
    }

    _roof_level += _roof_level < target ? 1 : -1;

    if(! _cutaway)
    {
        _begin_cutaway();
    }

    bn::blending::set_transparency_alpha(bn::fixed(_roof_level) / roof_fade_frames);
}

void world_renderer::_update_haze(bn::fixed alpha)
{
    bool want = alpha > 0 && ! _cutaway;

    if(want != _hazed)
    {
        // The canopy's blending belongs to the roof cut-away while it runs.
        for(int index = 0; index < int(world::layer::COUNT); ++index)
        {
            if(index != int(world::layer::CANOPY) || ! _cutaway)
            {
                _layers[index].bg.set_blending_enabled(want);
            }
        }

        _hazed = want;

        if(want)
        {
            bn::blending::set_white_fade_color();
        }
        else
        {
            bn::blending::set_fade_alpha(0);
        }
    }

    if(want)
    {
        bn::blending::set_fade_alpha(alpha);
    }
}

void world_renderer::set_season(int autumn_percent, int winter_percent)
{
    int key = (autumn_percent * 128) + winter_percent;

    if(key == _season_key)
    {
        return;
    }

    _season_key = key;

    // Grass, light grass, leaves and the forest floor (world palette indices 1, 2, 3 and 13) in the normal and decal
    // banks: summer as drawn, then towards autumn browns, then towards snow.
    constexpr int indices[] = { 1, 2, 3, 13 };
    constexpr bn::color autumn[] = { bn::color(17, 15, 7), bn::color(21, 18, 8), bn::color(16, 9, 4),
                                     bn::color(11, 9, 5) };
    constexpr bn::color winter[] = { bn::color(25, 26, 27), bn::color(28, 29, 30), bn::color(7, 10, 8),
                                     bn::color(22, 23, 24) };
    bn::span<const bn::color> drawn = bn::bg_palette_items::world_palette.colors_ref();
    bn::color colors[256];
    int count = drawn.size();

    for(int index = 0; index < count; ++index)
    {
        colors[index] = drawn[index];
    }

    auto blend = [](bn::color from, bn::color to, int percent)
    {
        return bn::color(from.red() + (((to.red() - from.red()) * percent) / 100),
                         from.green() + (((to.green() - from.green()) * percent) / 100),
                         from.blue() + (((to.blue() - from.blue()) * percent) / 100));
    };

    for(int bank = 0; bank < 2; ++bank)
    {
        for(int which = 0; which < 4; ++which)
        {
            bn::color& value = colors[(bank * 16) + indices[which]];
            value = blend(blend(value, autumn[which], autumn_percent), winter[which], winter_percent);
        }
    }

    _palette->set_colors(bn::span<const bn::color>(colors, count));
}

void world_renderer::_end_cutaway()
{
    if(_cutaway)
    {
        _cutaway = false;
        _layers[int(world::layer::CANOPY)].bg.set_blending_enabled(false);
        bn::blending::set_transparency_alpha(1);
        restore_window(bn::window::outside());
        restore_window(bn::window::sprites());
        restore_window(bn::rect_window::external());
        restore_window(bn::rect_window::internal());
    }
}

void world_renderer::set_dimmed(bool dimmed)
{
    if(_palette)
    {
        _palette->set_fade(bn::color(0, 0, 0), dimmed ? bn::fixed(menu_dim_percent) / 100 : bn::fixed(0));
    }
}

void world_renderer::set_visible(bool visible)
{
    for(layer_bg& layer : _layers)
    {
        layer.bg.set_visible(visible);
    }
}

void world_renderer::refresh_tile(int tile_x, int tile_y)
{
    int cell_x = tile_x * 2;
    int cell_y = tile_y * 2;

    // Cells outside the window may alias visible cells in the wraparound map, so only redraw inside it.
    if(_drawn && cell_x + 1 >= _cell_x && cell_x < _cell_x + visible_columns &&
            cell_y + 1 >= _cell_y && cell_y < _cell_y + visible_rows)
    {
        int x0 = bn::max(cell_x, _cell_x);
        int y0 = bn::max(cell_y, _cell_y);
        int x1 = bn::min(cell_x + 2, _cell_x + visible_columns);
        int y1 = bn::min(cell_y + 2, _cell_y + visible_rows);
        _draw_cells(x0, y0, x1 - x0, y1 - y0);
    }
}

#if KX_BENCH
    int world_renderer::verify() const
    {
        // The same streaming into a scratch map (in EWRAM: IWRAM is kept for code and the stack), then the visible
        // cells compared.
        alignas(4) BN_DATA_EWRAM_BSS static uint16_t scratch[map_cells * map_cells];
        int differ = 0;

        for(int index = 0; index < int(world::layer::COUNT); ++index)
        {
            const layer_bg& layer = _layers[index];
            auto vram_cells = layer.map.vram();
            const auto& vram = *vram_cells;

            stream_target target;
            target.vram = scratch;
            target.banks = world_gfx_banks;
            target.layer = uint8_t(index);
            target.tiles_offset = uint16_t(layer.map.tiles_offset());
            target.banks_offset = uint16_t(layer.map.palette_banks_offset());
            target.outside_gfx = uint8_t(layer_outside_gfx[index]);

            if(index == int(world::layer::CANOPY))
            {
                target.hide_left = int16_t(_hide_left);
                target.hide_top = int16_t(_hide_top);
                target.hide_right = int16_t(_hide_right);
                target.hide_bottom = int16_t(_hide_bottom);
            }

            stream_cells(target, _cell_x, _cell_y, visible_columns, visible_rows);

            for(int y = _cell_y; y < _cell_y + visible_rows; ++y)
            {
                for(int x = _cell_x; x < _cell_x + visible_columns; ++x)
                {
                    int cell = ((y & (map_cells - 1)) * map_cells) + (x & (map_cells - 1));
                    differ += vram[cell] != scratch[cell];
                }
            }
        }

        return differ;
    }
#endif

void world_renderer::_draw_cells(int cell_x, int cell_y, int columns, int rows, int only_layer)
{
    for(int index = 0; index < int(world::layer::COUNT); ++index)
    {
        if(only_layer >= 0 && index != only_layer)
        {
            continue;
        }

        layer_bg& layer = _layers[index];
        bn::span<bn::regular_bg_map_cell> vram = *layer.map.vram();

        stream_target target;
        target.vram = vram.data();
        target.banks = world_gfx_banks;
        target.layer = uint8_t(index);
        target.tiles_offset = uint16_t(layer.map.tiles_offset());
        target.banks_offset = uint16_t(layer.map.palette_banks_offset());
        target.outside_gfx = uint8_t(layer_outside_gfx[index]);

        if(index == int(world::layer::CANOPY))
        {
            target.hide_left = int16_t(_hide_left);
            target.hide_top = int16_t(_hide_top);
            target.hide_right = int16_t(_hide_right);
            target.hide_bottom = int16_t(_hide_bottom);
        }

        stream_cells(target, cell_x, cell_y, columns, rows);
    }
}

}
