#include "kx_safe_screen.h"

#include "bn_log.h"

#include "kx_bench.h"
#include "kx_game.h"
#include "kx_hud_tiles.h"
#include "kx_input.h"
#include "kx_sfx.h"
#include "kx_small_font.h"
#include "kx_ui.h"
#include "kx_world.h"
#include "kx_world_map.h"

namespace kx
{

namespace
{
    // Mockup 12 panel 5, in screen pixels.
    constexpr int panel_left = 60;
    constexpr int panel_top = 24;
    constexpr int panel_width = 120;
    constexpr int panel_height = 100;
    constexpr int centre_x = panel_left + (panel_width / 2);
    constexpr int digit_step = 28;
    constexpr int digit_box_top = 56;
    constexpr int digit_box_width = 21;
    constexpr int digit_box_height = 38;
    constexpr int footer_top = 144;
    constexpr int footer_y = 147;

    // A digit at twice the font's size.
    void big_digit(canvas& pixels, int x, int y, int digit, int colour)
    {
        int glyph = '0' + digit - small_font::first;

        for(int row = 0; row < small_font::height; ++row)
        {
            uint32_t bits = small_font::rows[glyph][row];

            for(int column = 0; column < small_font::widths[glyph]; ++column)
            {
                if((bits >> (column * 4)) & 0xF)
                {
                    pixels.rect(x + (column * 2), y + (row * 2), 2, 2, colour);
                }
            }
        }
    }

    void digit_text(char* text, int digit)
    {
        text[0] = char('0' + digit);
        text[1] = 0;
    }
}

void safe_screen::open(game& owner, int container_index)
{
    const container_record& value = owner.containers().record(container_index);
    _container = container_index;
    _house = world::house_id_at(value.tile_x, value.tile_y);
    _digits[0] = _digits[1] = _digits[2] = 0;
    _selected = 0;
    _active = true;
    _dirty = true;
    owner.hud_ref().begin_screen();
    sfx::play(sfx::id::CLICK, milli(600));
}

void safe_screen::close(game& owner)
{
    if(_active)
    {
        _active = false;
        owner.hud_ref().end_screen();
    }
}

void safe_screen::set_digits(int code)
{
    _digits[0] = int8_t((code / 100) % 10);
    _digits[1] = int8_t((code / 10) % 10);
    _digits[2] = int8_t(code % 10);
    _dirty = true;
}

void safe_screen::update(game& owner, const input_frame& input)
{
    if(input.b_pressed || input.start_pressed)
    {
        close(owner);
        return;
    }

    if(input.left_pressed || input.right_pressed)
    {
        _selected = int8_t((_selected + (input.right_pressed ? 1 : 2)) % 3);
        sfx::play(sfx::id::MOODLE, milli(300));
        _dirty = true;
    }

    if(input.up_pressed || input.down_pressed)
    {
        _digits[_selected] = int8_t((_digits[_selected] + (input.up_pressed ? 1 : 9)) % 10);
        sfx::play(sfx::id::CLICK, milli(400));
        _dirty = true;
    }

    if(input.a_pressed)
    {
        int tried = (_digits[0] * 100) + (_digits[1] * 10) + _digits[2];
        bool right = tried == world::safe_code(_house);

        #if KX_BENCH == 12
            BN_LOG("KXSAFE tried ", tried, " house=", _house, " right=", right);
        #endif

        if(! right)
        {
            sfx::play(sfx::id::CLICK, milli(700));
            owner.toast("WRONG CODE", hud_tiles::bank_danger, 45);
            return;
        }

        // Open: its note's mark has done its job; the safe is searched like any container.
        sfx::play(sfx::id::DOOR_CLOSE, milli(700));
        world_map::remove_auto_marks(world_map::auto_mark::NOTE, _house);
        int container = _container;
        close(owner);
        owner.begin_search(container, 0, true);
        return;
    }

    if(_dirty)
    {
        _draw(owner);
    }
}

void safe_screen::_draw(game& owner)
{
    hud& screen = owner.hud_ref();
    canvas& pixels = screen.pixels();
    _dirty = false;

    pixels.rect(0, 0, canvas::width, canvas::height, canvas::backdrop);
    pixels.panel(panel_left, panel_top, panel_width, panel_height, colour::fill, colour::border);
    pixels.text_centered(centre_x, panel_top + 6, "GUN SAFE", colour::accent);

    // Where it is: a police station's, or a house's, in which town.
    char text[40];
    int settlement = world::house_settlement(_house);
    int length = ui::append(text, 0, world::house_is_police(_house) ? "POLICE STATION" :
                                     world::house_is_armory(_house) ? "ARMORY" : "A HOUSE");

    if(settlement >= 0)
    {
        length = ui::append(text, length, ", ");
        ui::append(text, length, world_map::settlement_name(settlement));
    }

    pixels.text_centered(centre_x, panel_top + 14, text, colour::quiet);
    pixels.rect(panel_left + 4, panel_top + 22, panel_width - 8, 1, colour::border);

    for(int index = 0; index < 3; ++index)
    {
        int x = centre_x + ((index - 1) * digit_step);
        bool selected = index == _selected;
        int digit = _digits[index];
        pixels.panel(x - (digit_box_width / 2), digit_box_top, digit_box_width, digit_box_height, colour::fill,
                     selected ? colour::accent : colour::border);
        digit_text(text, (digit + 9) % 10);
        pixels.text_centered(x, digit_box_top + 3, text, colour::body_limb);
        big_digit(pixels, x - 4, digit_box_top + 13, digit, colour::text);
        digit_text(text, (digit + 1) % 10);
        pixels.text_centered(x, digit_box_top + 30, text, colour::body_limb);

        if(selected)
        {
            for(int row = 0; row < 3; ++row)
            {
                pixels.rect(x - row, digit_box_top - 7 + row, 1 + (row * 2), 1, colour::accent);
            }
        }
    }

    // A code read on a note (its mark is still on the map).
    if(world_map::has_auto_mark(world_map::auto_mark::NOTE, _house))
    {
        int code = world::safe_code(_house);
        length = ui::append(text, 0, "FROM A NOTE: ");
        text[length++] = char('0' + (code / 100));
        text[length++] = '-';
        text[length++] = char('0' + ((code / 10) % 10));
        text[length++] = '-';
        text[length++] = char('0' + (code % 10));
        text[length] = 0;
        pixels.text_centered(centre_x, panel_top + 76, text, colour::quiet);
    }

    pixels.text_centered(centre_x, panel_top + 86, "WRONG CODE: IT CLICKS", colour::body_limb);

    pixels.rect(0, footer_top, canvas::width, canvas::height - footer_top, canvas::backdrop);
    constexpr hud::hint hints[] = { { "<>", "DIGIT" }, { "UP/DOWN", "TURN" }, { "A", "TRY" }, { "B", "LEAVE" } };
    screen.hints(footer_y, hints, 4, false);
}

}
