#include "kx_skills_screen.h"

#include "kx_game.h"
#include "kx_input.h"
#include "kx_sfx.h"
#include "kx_ui.h"

namespace kx
{

namespace
{
    // Mockup 07 layout in screen pixels.
    constexpr int panels_top = 22;
    constexpr int panels_height = 120;
    constexpr int list_left = 12;
    constexpr int list_width = 120;
    constexpr int rows_top = 26;
    constexpr int row_step = 7;
    constexpr int divider_height = 2;
    constexpr int name_x = 17;
    constexpr int pips_x = 102;
    constexpr int pip_step = 5;
    constexpr int detail_left = 136;
    constexpr int detail_width = 92;
    constexpr int detail_x = detail_left + 5;
    constexpr int detail_right = detail_left + detail_width - 6;  // 4 px clear inside each border
    constexpr int footer_top = 144;
    constexpr int footer_y = 147;

    [[nodiscard]] int row_y(int index)
    {
        // The body stats, a divider, then the skills.
        return rows_top + (index * row_step) + (index >= 2 ? divider_height : 0);
    }

    void draw_row(canvas& pixels, const character& skills, int index, bool selected)
    {
        skill value = skill(index);
        int y = row_y(index);
        int level = skills.level(value);
        int needed = skills.xp_needed(value);
        int progress = needed ? (skills.xp(value) * 5) / needed : 0;      // 0-4 px of the next pip

        pixels.rect(list_left + 2, y - 1, list_width - 4, row_step, selected ? colour::accent : colour::fill);

        int name_colour = selected ? colour::fill : info(value).trains_now ? colour::text : colour::body_limb;
        pixels.text(name_x, y, info(value).name, name_colour);

        int on = selected ? colour::fill : colour::accent;
        int off = selected ? colour::fill : colour::empty;

        for(int pip = 0; pip < max_level; ++pip)
        {
            int x = pips_x + (pip * pip_step);

            if(selected && pip >= level)
            {
                // On the highlight, empty pips are outlines.
                pixels.rect(x, y, 4, 5, colour::fill);
                pixels.rect(x + 1, y + 1, 2, 3, colour::accent);
            }
            else
            {
                pixels.rect(x, y, 4, 5, pip < level ? on : off);
            }

            if(pip == level && progress > 0)
            {
                pixels.rect(x, y + 5 - progress, 4, progress, on);
            }
        }
    }
}

void skills_screen::open(game& owner)
{
    _active = true;
    _dirty = true;
    owner.hud_ref().begin_screen();
}

void skills_screen::close(game& owner)
{
    if(_active)
    {
        _active = false;
        owner.hud_ref().end_screen();
    }
}

void skills_screen::update(game& owner, const input_frame& input)
{
    if(input.b_pressed || input.start_pressed)
    {
        close(owner);
        return;
    }

    if(input.up_pressed || input.down_pressed)
    {
        _selected = step_list(_selected, skill_count, input);
        sfx::play(sfx::id::MOODLE, milli(300));
        _dirty = true;
    }

    if(_dirty)
    {
        _draw(owner);
    }
}

void skills_screen::_draw(game& owner)
{
    hud& screen = owner.hud_ref();
    canvas& pixels = screen.pixels();
    const character& skills = owner.hero().skills();
    skill value = skill(_selected);
    const skill_info& value_info = info(value);
    char text[32];
    int length;
    _dirty = false;

    pixels.rect(0, 0, canvas::width, canvas::height, canvas::backdrop);
    screen.tab_bar(3);

    // The list.
    pixels.panel(list_left, panels_top, list_width, panels_height, colour::fill, colour::border);

    for(int index = 0; index < skill_count; ++index)
    {
        draw_row(pixels, skills, index, index == _selected);
    }

    pixels.rect(list_left + 4, row_y(2) - divider_height, list_width - 8, 1, colour::border);

    // The selected one.
    pixels.panel(detail_left, panels_top, detail_width, panels_height, colour::fill, colour::border);
    pixels.text(detail_x, panels_top + 5, value_info.name, colour::accent);
    length = ui::append(text, 0, "LEVEL ");
    ui::append_int(text, length, skills.level(value));
    pixels.text_right(detail_right, panels_top + 5, text, colour::text);

    int needed = skills.xp_needed(value);
    int bar_y = panels_top + 14;
    int bar_width = detail_right - detail_x;
    pixels.rect(detail_x, bar_y, bar_width, 5, colour::border);
    pixels.rect(detail_x + 1, bar_y + 1, bar_width - 2, 3, colour::empty);

    if(needed)
    {
        pixels.rect(detail_x + 1, bar_y + 1, ((bar_width - 2) * skills.xp(value)) / needed, 3, colour::accent);
        length = ui::append_int(text, 0, skills.xp(value));
        length = ui::append(text, length, " / ");
        length = ui::append_int(text, length, needed);
        ui::append(text, length, " XP");
    }
    else
    {
        pixels.rect(detail_x + 1, bar_y + 1, bar_width - 2, 3, colour::accent);
        ui::append(text, 0, "TOP LEVEL");
    }

    pixels.text(detail_x, panels_top + 22, text, colour::quiet);

    // Where the levels came from: the occupation, and for the body stats the traits.
    int y = panels_top + 32;
    int from_job = skills.occupation_levels(value);

    if(from_job)
    {
        length = ui::append(text, 0, info(skills.job()).name);
        length = ui::append(text, length, " +");
        ui::append_int(text, length, from_job);
        pixels.text(detail_x, y, text, colour::ui_green);
        y += 7;
    }

    if(body_stat(value))
    {
        trait good = value == skill::STRENGTH ? trait::STRONG : trait::ATHLETIC;

        if(skills.has(good) || skills.has(opposite(good)))
        {
            trait chosen = skills.has(good) ? good : opposite(good);
            length = ui::append(text, 0, info(chosen).name);
            length = ui::append(text, length, skills.has(good) ? " +" : " -");
            ui::append_int(text, length, character_tuning::trait_body_levels);
            pixels.text(detail_x, y, text, skills.has(good) ? colour::ui_green : colour::ui_red);
        }
    }

    // The book for the current level (skills only).
    y = panels_top + 42;
    int volume = skills.current_volume(value);

    if(! body_stat(value) && volume)
    {
        length = ui::append(text, 0, "BOOK VOL ");
        ui::append_int(text, length, volume);
        pixels.text(detail_x, y, text, colour::text);

        if(skills.read(value, volume))
        {
            length = ui::append(text, 0, "READ ` X");
            length = ui::append_int(text, length, skills.book_multiplier(value));
            ui::append(text, length, " XP");
            pixels.text(detail_x, y + 7, text, colour::ui_green);
        }
        else
        {
            pixels.text(detail_x, y + 7, "NOT READ", colour::quiet);
        }
    }

    // What it does, and how it trains.
    y = panels_top + 59;

    for(const char* line : value_info.effect)
    {
        if(line)
        {
            pixels.text(detail_x, y, line, colour::quiet);
            y += 7;
        }
    }

    y = panels_top + 92;

    for(const char* line : value_info.training)
    {
        if(line)
        {
            pixels.text(detail_x, y, line, colour::body_limb);
            y += 7;
        }
    }

    pixels.rect(0, footer_top, canvas::width, canvas::height - footer_top, canvas::backdrop);
    constexpr hud::hint hints[] = { { "B", "CLOSE" }, { "D-PAD", "CHOOSE" } };
    screen.hints(footer_y, hints, 2, false);
}

}
