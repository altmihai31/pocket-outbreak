#include "kx_menu_screen.h"

#include "bn_algorithm.h"

#include "kx_game.h"
#include "kx_guide_text.h"
#include "kx_input.h"
#include "kx_save.h"
#include "kx_settings.h"
#include "kx_sfx.h"
#include "kx_ui.h"
#include "kx_world.h"

namespace kx
{

namespace
{
    // Mockups 18 and 20, in screen pixels.
    constexpr int panels_top = 22;
    constexpr int panels_height = 120;
    constexpr int list_left = 12;
    constexpr int list_width = 132;
    constexpr int rows_top = 42;
    constexpr int detail_left = 148;
    constexpr int detail_width = 80;
    constexpr int detail_x = detail_left + 5;
    constexpr int footer_top = 144;
    constexpr int footer_y = 147;
    constexpr int bar_steps = settings::volume_levels - 1;
    constexpr int bar_step_px = 5;

    // The guide: a narrow list of topics, then the page.
    constexpr int topics_left = 12;
    constexpr int topics_width = 64;
    constexpr int topic_step = 7;
    constexpr int page_left = 80;
    constexpr int page_width = 148;
    constexpr int page_x = page_left + 6;
    constexpr int line_step = 7;
    static_assert(page_x + guide::page_width <= page_left + page_width - 4);
    static_assert(18 + (guide::topic_count * topic_step) <= panels_height - 4);
    static_assert(18 + (guide::page_lines * line_step) <= panels_height - 4);

    constexpr const char* entry_names[menu_screen::entry_count] = { "SETTINGS", "GUIDE", "WORLD", "SAVE AND QUIT" };
    constexpr int quit_entry = 3;

    constexpr const char* entry_helps[menu_screen::entry_count][menu_screen::entry_help_lines] = {
        { "MUSIC AND", "SOUND LEVELS,", "SCREEN SHAKE,", "THE CPU METER." },
        { "HOW TO PLAY,", "IN 13 TOPICS:", "CONTROLS TO", "SAVING." },
        { "THIS WORLD'S", "SEED AND ITS", "SANDBOX", "SETTINGS." },
        { "SAVES, THEN", "BACK TO THE", "TITLE. HOLD A", "FOR A SECOND." },
    };
    static_assert(guide::topic_count == 13, "the GUIDE entry's help counts the topics");

    // Three steps that fill up to a volume's level (OFF: all empty).
    void level_bar(canvas& pixels, int x, int y, int level, bool selected)
    {
        for(int step = 0; step < bar_steps; ++step)
        {
            bool on = step < level;
            int colour = selected ? (on ? colour::fill : colour::accent) : (on ? colour::accent : colour::empty);
            pixels.rect(x + (step * bar_step_px), y, 4, 5, colour);
        }
    }

    [[nodiscard]] int topic_pages(int topic)
    {
        return guide::topic_first_page[topic + 1] - guide::topic_first_page[topic];
    }

    // Text wrapped at spaces to `width` px, a line every 7 px from y; returns the y after the last line.
    int wrapped(canvas& pixels, int x, int y, int width, const char* value, int colour)
    {
        constexpr int line_capacity = 40;
        char line[line_capacity + 1];
        int length = 0;

        for(const char* word = value; *word;)
        {
            const char* end = word;

            while(*end && *end != ' ')
            {
                ++end;
            }

            // The line with this word added: kept if it fits (or the line is empty), else the line goes out first.
            int before = length;

            if(length && length < line_capacity)
            {
                line[length++] = ' ';
            }

            for(const char* letter = word; letter < end && length < line_capacity; ++letter)
            {
                line[length++] = *letter;
            }

            line[length] = 0;

            if(before && canvas::text_width(line) > width)
            {
                line[before] = 0;
                pixels.text(x, y, line, colour);
                y += 7;
                length = 0;

                for(const char* letter = word; letter < end && length < line_capacity; ++letter)
                {
                    line[length++] = *letter;
                }

                line[length] = 0;
            }

            word = *end ? end + 1 : end;
        }

        if(length)
        {
            pixels.text(x, y, line, colour);
            y += 7;
        }

        return y;
    }
}

const char* const* menu_screen::entry_help(int entry)
{
    return entry_helps[entry];
}

const char* menu_screen::entry_name(int entry)
{
    return entry_names[entry];
}

void menu_screen::draw_entries(canvas& pixels, int left, int top, int width, int selected, int count)
{
    for(int index = 0; index < count; ++index)
    {
        int y = top + (index * row_step);
        bool chosen = index == selected;
        pixels.rect(left + 2, y - 3, width - 4, 11, chosen ? colour::accent : colour::fill);
        pixels.text(left + 6, y, entry_names[index], chosen ? colour::fill : colour::text);

        // An arrow on those that open a page (SAVE AND QUIT is held instead).
        if(index != quit_entry)
        {
            pixels.text_right(left + width - 7, y, ">", chosen ? colour::fill : colour::quiet);
        }
    }
}

void menu_screen::draw_settings(canvas& pixels, int left, int top, int width, int selected)
{
    for(int index = 0; index < settings::count; ++index)
    {
        settings::setting which = settings::setting(index);
        int y = top + (index * row_step);
        bool chosen = index == selected;
        int value = settings::value(which);
        pixels.rect(left + 2, y - 3, width - 4, 11, chosen ? colour::accent : colour::fill);
        pixels.text(left + 6, y, settings::name(which), chosen ? colour::fill : colour::text);

        int right = left + width - 7;

        if(settings::value_count(which) == settings::volume_levels)
        {
            level_bar(pixels, right - (bar_steps * bar_step_px) + 1, y, value, chosen);
            right -= (bar_steps * bar_step_px) + 5;
        }

        pixels.text_right(right, y, settings::value_text(which, value), chosen ? colour::fill : colour::quiet);
    }
}

bool menu_screen::handle_settings(int& selected, int dx, int dy)
{
    if(dy)
    {
        selected = (selected + dy + settings::count) % settings::count;
        sfx::play(sfx::id::MOODLE, milli(300));
        return true;
    }

    if(dx)
    {
        if(settings::step(settings::setting(selected), dx))
        {
            sfx::play(sfx::id::CLICK, milli(500));
            return true;
        }

        sfx::play(sfx::id::DENIED, milli(400));
    }

    return false;
}

void menu_screen::draw_guide(canvas& pixels, int top, int topic, int guide_page)
{
    // The topics, the chosen one on an accent bar.
    pixels.panel(topics_left, top, topics_width, panels_height, colour::fill, colour::border);
    pixels.text(topics_left + 6, top + 5, "GUIDE", colour::quiet);
    pixels.rect(topics_left + 4, top + 13, topics_width - 8, 1, colour::border);

    for(int index = 0; index < guide::topic_count; ++index)
    {
        int y = top + 18 + (index * topic_step);
        bool chosen = index == topic;

        if(chosen)
        {
            pixels.rect(topics_left + 2, y - 1, topics_width - 4, 7, colour::accent);
        }

        pixels.text(topics_left + 6, y, guide::topic_names[index], chosen ? colour::fill : colour::text);
    }

    // The page: the topic, which page of how many, then its lines.
    int pages = topic_pages(topic);
    const guide::page& shown = guide::pages[guide::topic_first_page[topic] + guide_page];
    pixels.panel(page_left, top, page_width, panels_height, colour::fill, colour::border);
    pixels.text(page_x, top + 5, guide::topic_names[topic], colour::accent);

    char counter[8];
    int length = ui::append_int(counter, 0, guide_page + 1);
    length = ui::append(counter, length, "/");
    ui::append_int(counter, length, pages);
    pixels.text_right(page_left + page_width - 6, top + 5, counter, colour::quiet);
    pixels.rect(page_left + 4, top + 13, page_width - 8, 1, colour::border);

    for(int line = 0; line < shown.count; ++line)
    {
        pixels.text(page_x, top + 18 + (line * line_step), guide::lines[shown.first + line], colour::text);
    }
}

bool menu_screen::handle_guide(int& topic, int& guide_page, int dx, int dy)
{
    if(dy)
    {
        topic = (topic + dy + guide::topic_count) % guide::topic_count;
        guide_page = 0;
        sfx::play(sfx::id::MOODLE, milli(300));
        return true;
    }

    if(dx)
    {
        int next = guide_page + dx;

        if(next >= 0 && next < topic_pages(topic))
        {
            guide_page = next;
            sfx::play(sfx::id::CLICK, milli(500));
            return true;
        }

        sfx::play(sfx::id::DENIED, milli(400));
    }

    return false;
}

void menu_screen::draw_world(canvas& pixels, int top, uint32_t seed, const sandbox::settings& world,
                             int selected)
{
    // The seed and the settings, read-only.
    pixels.panel(list_left, top, list_width, panels_height, colour::fill, colour::border);
    pixels.text(list_left + 6, top + 5, "WORLD", colour::quiet);

    char text[16];
    int length = ui::append(text, 0, "SEED ");

    for(int shift = 28; shift >= 0; shift -= 4)
    {
        text[length++] = "0123456789ABCDEF"[(seed >> shift) & 15];
    }

    text[length] = 0;
    pixels.text_right(list_left + list_width - 7, top + 5, text, colour::quiet);
    pixels.rect(list_left + 4, top + 13, list_width - 8, 1, colour::border);
    int rows = top + (rows_top - panels_top);

    for(int index = 0; index < sandbox::setting_count; ++index)
    {
        sandbox::setting which = sandbox::setting(index);
        int y = rows + (index * row_step);
        bool chosen = index == selected;
        pixels.rect(list_left + 2, y - 3, list_width - 4, 11, chosen ? colour::accent : colour::fill);
        pixels.text(list_left + 6, y, sandbox::name(which), chosen ? colour::fill : colour::text);
        pixels.text_right(list_left + list_width - 7, y, sandbox::value_name(which, world.values[index]),
                          chosen ? colour::fill : colour::quiet);
    }

    pixels.text(list_left + 6, top + panels_height - 14, "SET WHEN THE WORLD BEGAN", colour::body_limb);

    // The chosen one: its values, this world's marked, then what that value means.
    sandbox::setting which = sandbox::setting(selected);
    int value = world.values[selected];
    pixels.panel(detail_left, top, detail_width, panels_height, colour::fill, colour::border);
    pixels.text(detail_x, top + 5, sandbox::name(which), colour::accent);
    int y = top + 17;

    for(int index = 0, limit = sandbox::value_count(which); index < limit; ++index)
    {
        bool current = index == value;

        if(current)
        {
            pixels.tick_box(detail_x, y, true, colour::accent);
        }

        pixels.text(detail_x + 9, y, sandbox::value_name(which, index), current ? colour::accent : colour::quiet);
        y += 8;
    }

    wrapped(pixels, detail_x, y + 6, detail_width - 10, sandbox::detail(which, value), colour::quiet);
}

bool menu_screen::handle_world(int& selected, int dy)
{
    if(! dy)
    {
        return false;
    }

    selected = (selected + dy + sandbox::setting_count) % sandbox::setting_count;
    sfx::play(sfx::id::MOODLE, milli(300));
    return true;
}

bool menu_screen::quit_requested() const
{
    return _quit_hold >= save::quit_hold_frames;
}

void menu_screen::open(game& owner)
{
    _active = true;
    _dirty = true;
    _quit_hold = 0;
    _page = page::ENTRIES;
    owner.hud_ref().begin_screen();
}

void menu_screen::close(game& owner)
{
    if(_active)
    {
        _active = false;
        owner.hud_ref().end_screen();
    }
}

#if KX_DUMP
    void menu_screen::show(page value, int choice, int guide_page)
    {
        _page = value;
        _dirty = true;

        if(value == page::ENTRIES)
        {
            _entry = choice;
        }
        else if(value == page::SETTINGS)
        {
            _setting = choice;
        }
        else if(value == page::GUIDE)
        {
            _topic = choice;
            _guide_page = guide_page;
        }
        else
        {
            _world_row = choice;
        }
    }
#endif

void menu_screen::update(game& owner, const input_frame& input)
{
    if(input.start_pressed || (input.b_pressed && _page == page::ENTRIES))
    {
        close(owner);
        return;
    }

    int dx = int(input.right_pressed) - int(input.left_pressed);
    int dy = int(input.down_pressed) - int(input.up_pressed);

    switch(_page)
    {

    case page::ENTRIES:
        if(_entry == quit_entry)
        {
            // Held, with a bar filling under the row; let go and it empties.
            int held = input.a_held && ! dy ? _quit_hold + 1 : 0;

            if(held != _quit_hold)
            {
                _quit_hold = held;
                _dirty = true;
            }
        }

        if(dy)
        {
            _entry = (_entry + dy + entry_count) % entry_count;
            sfx::play(sfx::id::MOODLE, milli(300));
            _dirty = true;
        }
        else if(input.a_pressed && _entry != quit_entry)
        {
            _page = _entry == 0 ? page::SETTINGS : _entry == 1 ? page::GUIDE : page::WORLD;
            sfx::play(sfx::id::CLICK, milli(500));
            _dirty = true;
        }
        break;

    case page::SETTINGS:
        if(input.b_pressed)
        {
            _page = page::ENTRIES;
            _dirty = true;
        }
        else if(handle_settings(_setting, dx, dy))
        {
            // The CPU meter is the HUD's (the HUD keeps its own flag, which the debug menu also toggles).
            owner.hud_ref().set_perf_visible(settings::cpu_meter());
            _dirty = true;
        }
        break;

    case page::GUIDE:
        if(input.b_pressed)
        {
            _page = page::ENTRIES;
            _dirty = true;
        }
        else if(handle_guide(_topic, _guide_page, dx, dy))
        {
            _dirty = true;
        }
        break;

    case page::WORLD:
        if(input.b_pressed)
        {
            _page = page::ENTRIES;
            _dirty = true;
        }
        else if(handle_world(_world_row, dy))
        {
            _dirty = true;
        }
        break;

    default:
        break;
    }

    if(_dirty)
    {
        _draw(owner);
    }
}

void menu_screen::_draw(game& owner)
{
    hud& screen = owner.hud_ref();
    canvas& pixels = screen.pixels();
    _dirty = false;

    pixels.rect(0, 0, canvas::width, canvas::height, canvas::backdrop);
    screen.tab_bar(5);

    if(_page == page::GUIDE)
    {
        draw_guide(pixels, panels_top, _topic, _guide_page);
        pixels.rect(0, footer_top, canvas::width, canvas::height - footer_top, canvas::backdrop);
        constexpr hud::hint hints[] = { { "B", "BACK" }, { "UP/DOWN", "TOPIC" }, { "<>", "PAGE" } };
        screen.hints(footer_y, hints, 3, false);
        return;
    }

    if(_page == page::WORLD)
    {
        draw_world(pixels, panels_top, world::seed(), sandbox::current(), _world_row);
        pixels.rect(0, footer_top, canvas::width, canvas::height - footer_top, canvas::backdrop);
        constexpr hud::hint hints[] = { { "B", "BACK" }, { "D-PAD", "CHOOSE" } };
        screen.hints(footer_y, hints, 2, false);
        return;
    }

    pixels.panel(list_left, panels_top, list_width, panels_height, colour::fill, colour::border);
    pixels.panel(detail_left, panels_top, detail_width, panels_height, colour::fill, colour::border);

    if(_page == page::ENTRIES)
    {
        // The two entries; on the right what the chosen one opens.
        pixels.text(list_left + 6, panels_top + 5, "MENU", colour::quiet);
        pixels.rect(list_left + 4, panels_top + 13, list_width - 8, 1, colour::border);
        draw_entries(pixels, list_left, rows_top, list_width, _entry);
        pixels.text(detail_x, panels_top + 5, entry_names[_entry], colour::accent);

        for(int line = 0; line < entry_help_lines; ++line)
        {
            pixels.text(detail_x, panels_top + 17 + (line * 7), entry_helps[_entry][line], colour::quiet);
        }

        if(_entry == quit_entry)
        {
            // The hold's bar, under the row's text.
            if(_quit_hold > 0)
            {
                int bar_y = rows_top + (quit_entry * row_step) + 6;
                pixels.rect(list_left + 6, bar_y, ((list_width - 12) * bn::min(_quit_hold, save::quit_hold_frames)) /
                            save::quit_hold_frames, 1, colour::fill);
            }

            // Milestone 13: the world's save lists nearly full (it was beside the Select hint).
            int storage = owner.storage_percent();

            if(storage > save::storage_show_percent)
            {
                char text[24];
                int length = ui::append(text, 0, "WORLD ");
                ui::append(text, ui::append_int(text, length, bn::min(storage, 100)), "% FULL");
                pixels.text(detail_x, panels_top + 17 + (entry_help_lines * 7) + 6, text,
                            storage >= save::storage_warn_percent ? colour::ui_red : colour::accent);
            }
        }

        pixels.rect(0, footer_top, canvas::width, canvas::height - footer_top, canvas::backdrop);
        constexpr hud::hint open_hints[] = { { "A", "OPEN" }, { "B", "CLOSE" }, { "D-PAD", "CHOOSE" } };
        constexpr hud::hint quit_hints[] = { { "HOLD A", "SAVE AND QUIT" }, { "B", "CLOSE" }, { "D-PAD", "CHOOSE" } };
        screen.hints(footer_y, _entry == quit_entry ? quit_hints : open_hints, 3, false);
        return;
    }

    // The settings list.
    pixels.text(list_left + 6, panels_top + 5, "GAME SETTINGS", colour::quiet);
    pixels.rect(list_left + 4, panels_top + 13, list_width - 8, 1, colour::border);
    draw_settings(pixels, list_left, rows_top, list_width, _setting);
    pixels.text(list_left + 6, panels_top + panels_height - 14, "KEPT IN THE SAVE", colour::body_limb);

    // The chosen one: its values, the current one marked, then what it does.
    settings::setting which = settings::setting(_setting);
    pixels.text(detail_x, panels_top + 5, settings::name(which), colour::accent);
    int y = panels_top + 17;

    for(int value = 0, limit = settings::value_count(which); value < limit; ++value)
    {
        bool current = value == settings::value(which);

        // The current value ticked, as traits are (playtest 6: it was ">FULL", a third way to mark a choice).
        if(current)
        {
            pixels.tick_box(detail_x, y, true, colour::accent);
        }

        pixels.text(detail_x + 9, y, settings::value_text(which, value), current ? colour::accent : colour::quiet);
        y += 8;
    }

    y += 6;

    for(int line = 0; line < settings::help_lines; ++line)
    {
        if(const char* text = settings::help(which)[line])
        {
            pixels.text(detail_x, y, text, colour::quiet);
            y += 7;
        }
    }

    pixels.rect(0, footer_top, canvas::width, canvas::height - footer_top, canvas::backdrop);
    constexpr hud::hint hints[] = { { "B", "BACK" }, { "UP/DOWN", "CHOOSE" }, { "<>", "CHANGE" } };
    screen.hints(footer_y, hints, 3, false);
}

}
