#include "kx_debug_menu.h"

#include "bn_algorithm.h"
#include "bn_color.h"
#include "bn_keypad.h"
#include "bn_sprite_palette_item.h"

#include "common_variable_8x8_sprite_font.h"

#include "kx_camera_tuning.h"
#include "kx_combat_tuning.h"
#include "kx_game.h"
#include "kx_hordes.h"
#include "kx_guns.h"
#include "kx_light.h"
#include "kx_world_map.h"
#include "kx_macro_map.h"
#include "kx_world.h"
#include "kx_loot.h"
#include "kx_survival.h"
#include "kx_movement_tuning.h"

namespace kx
{

namespace
{
    // Font palette indices: 1 glyph, 2 shadow (docs/palette.md colours).
    constexpr bn::color text_colors[16] = {
        // Fixed font: 1 glyph, 2 shadow. Variable font: 14 glyph, 12 shadow.
        bn::color(0, 0, 0), bn::color(29, 28, 27), bn::color(1, 1, 2), bn::color(0, 0, 0),
        bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0),
        bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0),
        bn::color(1, 1, 2), bn::color(0, 0, 0), bn::color(29, 28, 27), bn::color(0, 0, 0)
    };
    constexpr bn::color accent_colors[16] = {
        // Fixed font: 1 glyph, 2 shadow. Variable font: 14 glyph, 12 shadow.
        bn::color(0, 0, 0), bn::color(29, 24, 8), bn::color(1, 1, 2), bn::color(0, 0, 0),
        bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0),
        bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0),
        bn::color(1, 1, 2), bn::color(0, 0, 0), bn::color(29, 24, 8), bn::color(0, 0, 0)
    };
    constexpr bn::color quiet_colors[16] = {
        // Fixed font: 1 glyph, 2 shadow. Variable font: 14 glyph, 12 shadow.
        bn::color(0, 0, 0), bn::color(21, 22, 23), bn::color(1, 1, 2), bn::color(0, 0, 0),
        bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0),
        bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0), bn::color(0, 0, 0),
        bn::color(1, 1, 2), bn::color(0, 0, 0), bn::color(21, 22, 23), bn::color(0, 0, 0)
    };
    constexpr bn::sprite_palette_item text_palette(text_colors, bn::bpp_mode::BPP_4);
    constexpr bn::sprite_palette_item accent_palette(accent_colors, bn::bpp_mode::BPP_4);
    constexpr bn::sprite_palette_item quiet_palette(quiet_colors, bn::bpp_mode::BPP_4);

    // Tuning tables first, then debug switches and actions, then the world's macro map.
    constexpr int tuning_pages = 13;
    constexpr int debug_page = tuning_pages;
    constexpr int map_page = tuning_pages + 1;
    constexpr int pages_count = tuning_pages + 2;

    // Macro map at 2 px per cell on the right of the backdrop.
    constexpr int map_left = 104;
    constexpr int map_top = 18;
    constexpr int map_scale = 2;

    [[nodiscard]] int map_colour(int cell_x, int cell_y)
    {
        switch(macro::zone_at(cell_x, cell_y))
        {

        case macro::zone::VILLAGE:
            return colour::accent;

        case macro::zone::TOWN:
            return colour::ui_orange;

        case macro::zone::MILITARY:
            return colour::ui_red;

        case macro::zone::FARM:
            return colour::ui_dark_red;

        default:
            break;
        }

        if(macro::roads_at(cell_x, cell_y))
        {
            return colour::text;
        }

        switch(macro::land_at(cell_x, cell_y))
        {

        case macro::land::WATER:
            return colour::ui_water;

        case macro::land::COAST:
            return colour::ui_cold;

        case macro::land::WOODS:
            return ((cell_x + cell_y) & 1) ? colour::ui_green : colour::border;

        case macro::land::FOREST:
            return colour::border;

        case macro::land::HILLS:
            return colour::quiet;

        default:
            return colour::ui_green;
        }
    }

    void draw_macro_map(canvas& pixels, game& owner, int selected)
    {
        for(int cell_y = 0; cell_y < macro::cells; ++cell_y)
        {
            for(int cell_x = 0; cell_x < macro::cells; ++cell_x)
            {
                pixels.rect(map_left + (cell_x * map_scale), map_top + (cell_y * map_scale), map_scale, map_scale,
                            map_colour(cell_x, cell_y));

                // Horde counts: a dot per cell, dark red for a few, orange for dozens, bright red for 100 or more.
                if(int count = macro::horde_count(cell_x, cell_y))
                {
                    pixels.rect(map_left + (cell_x * map_scale), map_top + (cell_y * map_scale), 1, 1,
                                count >= 100 ? colour::ui_red : count >= 30 ? colour::ui_orange : colour::ui_dark_red);
                }
            }
        }

        if(selected < macro::settlement_count())
        {
            const macro::settlement& value = macro::settlement_at(selected);
            pixels.frame(map_left + (value.cell_x * map_scale) - 2, map_top + (value.cell_y * map_scale) - 2,
                         (value.size * map_scale) + 4, (value.size * map_scale) + 4, colour::text);
        }

        int player_cell_x = owner.hero().position().x().integer() >> (world::tile_shift + macro::cell_shift);
        int player_cell_y = owner.hero().position().y().integer() >> (world::tile_shift + macro::cell_shift);
        pixels.rect(map_left + (player_cell_x * map_scale) - 1, map_top + (player_cell_y * map_scale) - 1, 4, 4,
                    colour::fill);
        pixels.rect(map_left + (player_cell_x * map_scale), map_top + (player_cell_y * map_scale), 2, 2,
                    colour::ui_red);
    }

    [[nodiscard]] const char* zone_name(macro::zone value)
    {
        return value == macro::zone::TOWN ? "town" : value == macro::zone::MILITARY ? "military" : "village";
    }

    enum debug_row
    {
        ROW_CPU,
        ROW_GOD,
        ROW_RESPAWN,
        ROW_HORDE,
        ROW_RESTOCK,
        ROW_ITEM,
        ROW_EVENT,
        ROW_DAYS,
        ROW_VEHICLE,
        ROW_HOUR,
        ROW_REVEAL,
        DEBUG_ROWS
    };

    constexpr const char* debug_row_names[] = {
        "CPU readout", "God mode (no bites)", "Respawn test zombies", "Spawn horde (40)", "Give test kit",
        "Give item (A)", "World event (A starts)", "Skip 10 days", "Spawn vehicle (A)",
        "Set hour (A)", "Reveal map (A)"
    };

    [[nodiscard]] tuning_set& page_tuning(int page)
    {
        switch(page)
        {

        case 0:
            return movement_tuning();

        case 1:
            return camera_tuning();

        case 2:
            return combat_tuning();

        case 3:
            return zombie_tuning();

        case 4:
            return inventory_tuning();

        case 5:
            return survival_tuning();

        case 10:
            return light_tuning();

        case 11:
            return gun_tuning();

        case 12:
            return weapon_tuning(weapon::SPEAR);

        default:
            return weapon_tuning(weapon(page - 6));
        }
    }

    void copy_text(char* output, const char* text)
    {
        while(*text)
        {
            *output++ = *text++;
        }

        *output = 0;
    }

    constexpr int left_x = -112;
    constexpr int right_x = 112;
    constexpr int title_y = -70;
    constexpr int first_row_y = -54;
    constexpr int row_height = 11;
    constexpr int footer_y = 70;

    constexpr int repeat_delay = 18;
    constexpr int repeat_interval = 4;
    constexpr int fast_steps = 10;

    int format_int(char* output, int value)
    {
        char digits[12];
        int count = 0;
        bool negative = value < 0;

        if(negative)
        {
            value = -value;
        }

        do
        {
            digits[count++] = char('0' + (value % 10));
            value /= 10;
        }
        while(value > 0 && count < 11);

        int length = 0;

        if(negative)
        {
            output[length++] = '-';
        }

        while(count > 0)
        {
            output[length++] = digits[--count];
        }

        output[length] = 0;
        return length;
    }

    void format_value(char* output, const tuning_entry& entry, bn::fixed value)
    {
        if(entry.integer)
        {
            format_int(output, value.integer());
            return;
        }

        // Thousandths for sub-centi steps (diagonal), hundredths otherwise.
        int decimals = entry.step < milli(10) ? 3 : 2;
        int scale = decimals == 3 ? 1000 : 100;
        int scaled = (value * scale).round_integer();
        int length = format_int(output, scaled / scale);
        output[length++] = '.';
        int fraction = scaled % scale;

        for(int divisor = scale / 10; divisor > 0; divisor /= 10)
        {
            output[length++] = char('0' + ((fraction / divisor) % 10));
        }

        output[length] = 0;
    }
}

debug_menu::debug_menu() :
    _text(common::variable_8x8_sprite_font)
{
    _text.set_bg_priority(0);
}

void debug_menu::open(game& owner)
{
    _active = true;
    owner.hud_ref().set_menu_backdrop(true);
    _redraw(owner);
}

void debug_menu::update(game& owner)
{
    if(bn::keypad::start_pressed() || bn::keypad::b_pressed())
    {
        _close(owner);
        return;
    }

    bool changed = false;

    if(bn::keypad::l_pressed() || bn::keypad::r_pressed())
    {
        _page = (_page + (bn::keypad::r_pressed() ? 1 : pages_count - 1)) % pages_count;
        _row = 0;
        _scroll = 0;
        changed = true;
        owner.hud_ref().set_menu_backdrop(true);
    }

    int rows = _rows_count();

    if(_repeat(bn::keypad::up_pressed(), bn::keypad::up_held()))
    {
        _row = (_row + rows - 1) % rows;
        changed = true;
    }
    else if(_repeat(bn::keypad::down_pressed(), bn::keypad::down_held()))
    {
        _row = (_row + 1) % rows;
        changed = true;
    }

    int direction = 0;

    if(_repeat(bn::keypad::left_pressed(), bn::keypad::left_held()))
    {
        direction = -1;
    }
    else if(_repeat(bn::keypad::right_pressed(), bn::keypad::right_held()))
    {
        direction = 1;
    }

    if(_page < tuning_pages)
    {
        if(direction)
        {
            _tuning().adjust(_row, direction * (bn::keypad::a_held() ? fast_steps : 1));
            changed = true;
        }

        if(bn::keypad::select_pressed())
        {
            _tuning().reset();
            changed = true;
        }
    }
    else if(_page == map_page)
    {
        int count = macro::settlement_count();

        if(direction && count)
        {
            _settlement = (_settlement + direction + count) % count;
            changed = true;
        }
        else if(bn::keypad::a_pressed() && count)
        {
            _close(owner);
            owner.teleport_to_settlement(_settlement);
            return;
        }
    }
    else if(direction || bn::keypad::a_pressed())
    {
        changed = true;

        switch(_row)
        {

        case ROW_CPU:
            owner.hud_ref().set_perf_visible(! owner.hud_ref().perf_visible());
            owner.hud_ref().set_menu_backdrop(true);
            break;

        case ROW_GOD:
            owner.set_god_mode(! owner.god_mode());
            break;

        case ROW_RESPAWN:
            owner.respawn_zombies();
            break;

        case ROW_HORDE:
            owner.spawn_horde();
            break;

        case ROW_ITEM:
        {
            // Left/Right pick any item (holding repeats), A gives it: a full stack, or one.
            constexpr int count = int(item_id::COUNT);

            if(direction)
            {
                do
                {
                    _item = (_item + direction + count) % count;
                }
                while(_item == int(item_id::NONE) || item_id(_item) == item_id::TAP_WATER);
            }
            else
            {
                owner.give_item(item_id(_item));
            }
            break;
        }

        case ROW_EVENT:
            // Left/Right pick the event, A starts it.
            if(direction)
            {
                _event = (_event + direction + 4) % 4;
            }
            else
            {
                _close(owner);
                owner.trigger_event(_event);
                return;
            }
            break;

        case ROW_DAYS:
            if(bn::keypad::a_pressed())
            {
                owner.skip_days(10);
            }
            break;

        case ROW_VEHICLE:
            // Left/Right pick a car that runs or a bicycle, A puts it beside the player.
            if(direction)
            {
                _vehicle ^= 1;
            }
            else
            {
                _close(owner);
                owner.spawn_vehicle(_vehicle);
                return;
            }
            break;

        case ROW_REVEAL:
            if(bn::keypad::a_pressed())
            {
                world_map::reveal_all();
            }
            break;

        case ROW_HOUR:
            // Left/Right pick the hour, A moves the clock forward to it.
            if(direction)
            {
                _hour = (_hour + direction + 24) % 24;
            }
            else
            {
                owner.skip_to_hour(_hour);
            }
            break;

        default:
            owner.give_test_kit();
            break;
        }
    }

    if(! bn::keypad::up_held() && ! bn::keypad::down_held() && ! bn::keypad::left_held() &&
            ! bn::keypad::right_held())
    {
        _repeat_frames = 0;
    }

    if(changed)
    {
        if(_row < _scroll)
        {
            _scroll = _row;
        }
        else if(_row >= _scroll + visible_rows)
        {
            _scroll = _row - visible_rows + 1;
        }

        _redraw(owner);
    }
}

tuning_set& debug_menu::_tuning() const
{
    return page_tuning(_page);
}

int debug_menu::_rows_count() const
{
    return _page < tuning_pages ? _tuning().size() : _page == debug_page ? DEBUG_ROWS : 1;
}

bool debug_menu::_repeat(bool pressed, bool held)
{
    if(pressed)
    {
        _repeat_frames = 0;
        return true;
    }

    if(held)
    {
        ++_repeat_frames;
        return _repeat_frames >= repeat_delay && (_repeat_frames - repeat_delay) % repeat_interval == 0;
    }

    return false;
}

void debug_menu::_close(game& owner)
{
    _active = false;
    _sprites.clear();
    owner.hud_ref().set_menu_backdrop(false);
}

void debug_menu::_redraw(game& owner)
{
    _sprites.clear();

    char buffer[48];
    const char* page_title = _page < tuning_pages ? _tuning().title() : _page == debug_page ? "DEBUG" : "MAP";

    _text.set_left_alignment();
    _text.set_palette_item(accent_palette);
    _text.generate(left_x, title_y, page_title, _sprites);

    int page_length = format_int(buffer, _page + 1);
    buffer[page_length++] = '/';
    format_int(buffer + page_length, pages_count);
    _text.set_right_alignment();
    _text.set_palette_item(quiet_palette);
    _text.generate(right_x, title_y, buffer, _sprites);

    int rows = _page == map_page ? 0 : _rows_count();

    if(_page == map_page)
    {
        // Seed, the settlement A teleports to (Left/Right picks it) and the map.
        draw_macro_map(owner.hud_ref().pixels(), owner, _settlement);
        _text.set_left_alignment();
        _text.set_palette_item(quiet_palette);
        copy_text(buffer, "Seed ");
        uint32_t seed = world::seed();

        for(int shift = 28, length = 5; shift >= 0; shift -= 4, ++length)
        {
            int digit = int((seed >> shift) & 15);
            buffer[length] = char(digit < 10 ? '0' + digit : 'A' + digit - 10);
            buffer[length + 1] = 0;
        }

        _text.generate(left_x, first_row_y, buffer, _sprites);
        _text.set_palette_item(accent_palette);
        _text.generate(left_x, first_row_y + (row_height * 2), "> Go to", _sprites);

        if(int count = macro::settlement_count())
        {
            int length = format_int(buffer, _settlement + 1);
            buffer[length++] = '/';
            length += format_int(buffer + length, count);
            buffer[length++] = ' ';
            copy_text(buffer + length, zone_name(macro::settlement_at(_settlement).kind));
        }
        else
        {
            copy_text(buffer, "none (test map)");
        }

        _text.generate(left_x + 8, first_row_y + (row_height * 3), buffer, _sprites);

        // The zombies around: individuals and the head counts still in range.
        _text.set_palette_item(quiet_palette);
        int length = format_int(buffer, owner.zombies().count());
        copy_text(buffer + length, " zombies here");
        _text.generate(left_x, first_row_y + (row_height * 5), buffer, _sprites);
        length = format_int(buffer, hordes::counted_near(owner.hero().position()));
        copy_text(buffer + length, " more counted");
        _text.generate(left_x, first_row_y + (row_height * 6), buffer, _sprites);
    }

    for(int index = 0; index < visible_rows && _scroll + index < rows; ++index)
    {
        int row = _scroll + index;
        int y = first_row_y + (index * row_height);
        bool selected = row == _row;
        const char* name;
        bool modified = false;

        if(_page < tuning_pages)
        {
            const tuning_entry& entry = _tuning().entry(row);
            name = entry.name;
            format_value(buffer, entry, _tuning().get(row));
            modified = ! _tuning().is_default(row);
        }
        else
        {
            name = debug_row_names[row];

            if(row == ROW_CPU)
            {
                copy_text(buffer, owner.hud_ref().perf_visible() ? "ON" : "OFF");
            }
            else if(row == ROW_GOD)
            {
                copy_text(buffer, owner.god_mode() ? "ON" : "OFF");
            }
            else if(row == ROW_ITEM)
            {
                copy_text(buffer, info(item_id(_item)).name);
            }
            else if(row == ROW_EVENT)
            {
                constexpr const char* names[] = { "Helicopter", "Gunshots", "Alarm", "Crash" };
                copy_text(buffer, names[_event]);
            }
            else if(row == ROW_VEHICLE)
            {
                copy_text(buffer, _vehicle ? "Bicycle" : "Car");
            }
            else if(row == ROW_DAYS)
            {
                copy_text(buffer, "day ");
                format_int(buffer + 4, owner.day());
            }
            else if(row == ROW_HOUR)
            {
                int length = format_int(buffer, _hour);
                copy_text(buffer + length, ":00 now ");
                length += 8;
                int hour = owner.minute_of_day() / 60;
                int minute = owner.minute_of_day() % 60;
                length += format_int(buffer + length, hour);
                buffer[length++] = ':';
                buffer[length++] = char('0' + (minute / 10));
                buffer[length++] = char('0' + (minute % 10));
                buffer[length] = 0;
            }
            else if(row == ROW_HORDE || row == ROW_RESPAWN)
            {
                int length = format_int(buffer, owner.zombies().count());
                copy_text(buffer + length, " alive");
            }
            else
            {
                copy_text(buffer, "A");
            }
        }

        _text.set_left_alignment();
        _text.set_palette_item(selected ? accent_palette : text_palette);
        _text.generate(left_x, y, selected ? ">" : " ", _sprites);
        _text.generate(left_x + 8, y, name, _sprites);

        _text.set_right_alignment();
        _text.set_palette_item(selected ? accent_palette : (modified ? text_palette : quiet_palette));
        _text.generate(right_x, y, buffer, _sprites);

        if(modified)
        {
            _text.generate(right_x - 40, y, "*", _sprites);
        }
    }

    _text.set_left_alignment();
    _text.set_palette_item(quiet_palette);

    _text.generate(left_x, footer_y, "L1/R1 page  A+</>: x10  SEL: reset  START: close", _sprites);
}

}
