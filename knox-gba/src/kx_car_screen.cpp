#include "kx_car_screen.h"

#include "kx_game.h"
#include "kx_hud_tiles.h"
#include "kx_input.h"
#include "kx_sfx.h"
#include "kx_ui.h"
#include "kx_vehicles.h"

namespace kx
{

namespace
{
    // Mockup 10 layout in screen pixels.
    constexpr int header_left = 12;
    constexpr int header_top = 4;
    constexpr int header_width = 216;
    constexpr int header_height = 14;
    constexpr int header_text_y = 9;
    constexpr int panels_top = 22;
    constexpr int panels_height = 120;
    constexpr int list_left = 12;
    constexpr int list_width = 120;
    constexpr int rows_top = 37;
    constexpr int row_step = 7;
    constexpr int detail_left = 136;
    constexpr int detail_width = 92;
    constexpr int detail_x = detail_left + 5;
    constexpr int detail_right = detail_left + detail_width - 6;  // 4 px clear inside each border (playtest 2)
    constexpr int detail_text_width = detail_right - detail_x + 1;
    constexpr int line_step = 7;
    constexpr int footer_top = 144;
    constexpr int footer_y = 147;

    constexpr int part_count = int(car_part::COUNT);
    constexpr int action_count = int(car_action::COUNT);
    constexpr int row_count = part_count + action_count;
    constexpr int actions_title_y = rows_top + (part_count * row_step) + 7;
    constexpr int actions_top = actions_title_y + 8;
    constexpr int tracked_warning = vehicle_tuning::max_tracked - vehicle_tuning::reserved_slots;

    constexpr const char* part_titles[] = { "BATTERY", "SPARK PLUG", "RADIATOR", "TYRES", "FUEL" };
    constexpr const char* action_titles[] = { "TRUNK", "GLOVE BOX", "SIPHON FUEL", "HOTWIRE", "REPAIR", "DRIVE" };
    static_assert(sizeof(action_titles) / sizeof(action_titles[0]) == int(car_action::COUNT));

    [[nodiscard]] int row_y(int row)
    {
        return row < part_count ? rows_top + (row * row_step) : actions_top + ((row - part_count) * row_step);
    }

    void tick(canvas& pixels, int x, int y, int colour)
    {
        constexpr int points[][2] = { { 0, 2 }, { 1, 3 }, { 2, 2 }, { 3, 1 }, { 4, 0 } };

        for(const auto& point : points)
        {
            pixels.rect(x + point[0], y + point[1], 1, 2, colour);
        }
    }

    void cross(canvas& pixels, int x, int y, int colour)
    {
        for(int step = 0; step < 5; ++step)
        {
            pixels.rect(x + step, y + step, 1, 1, colour);
            pixels.rect(x + 4 - step, y + step, 1, 1, colour);
        }
    }

    // Text wrapped at spaces to the detail panel's width, a line every 7 px (playtest 2: problems such as "A WRECK
    // CAN'T BE FIXED" and some descriptions ran into the panel's border). Draws only if `draw`; returns the lines.
    int wrap(canvas& pixels, int y, const char* value, int colour, bool draw = true)
    {
        char line[40];
        int length = 0;
        int lines = 0;
        const char* word = value;

        while(*word)
        {
            const char* end = word;

            while(*end && *end != ' ')
            {
                ++end;
            }

            // The line with this word added: kept if it fits (or holds nothing yet), else the line goes out first.
            char trial[40];
            int trial_length = 0;

            for(int index = 0; index < length; ++index)
            {
                trial[trial_length++] = line[index];
            }

            if(trial_length)
            {
                trial[trial_length++] = ' ';
            }

            for(const char* letter = word; letter < end && trial_length < 39; ++letter)
            {
                trial[trial_length++] = *letter;
            }

            trial[trial_length] = 0;

            if(length && canvas::text_width(trial) > detail_text_width)
            {
                line[length] = 0;

                if(draw)
                {
                    pixels.text(detail_x, y + (lines * line_step), line, colour);
                }

                ++lines;
                length = 0;

                for(const char* letter = word; letter < end && length < 39; ++letter)
                {
                    line[length++] = *letter;
                }
            }
            else
            {
                for(int index = 0; index < trial_length; ++index)
                {
                    line[index] = trial[index];
                }

                length = trial_length;
            }

            word = *end ? end + 1 : end;
        }

        if(length)
        {
            line[length] = 0;

            if(draw)
            {
                pixels.text(detail_x, y + (lines * line_step), line, colour);
            }

            ++lines;
        }

        return lines;
    }

    // One "needs" line: the name, and what is carried against what it takes.
    int need_line(canvas& pixels, int y, const char* name, const char* have, bool ok)
    {
        pixels.text(detail_x, y, name, colour::text);
        pixels.text_right(detail_right, y, have, ok ? colour::ui_green : colour::ui_red);
        return y + 7;
    }

    int need_item(game& owner, canvas& pixels, int y, const char* name, item_id id, bool counted)
    {
        int have = owner.carries(id);
        char text[12];

        if(counted)
        {
            ui::append(text, ui::append_int(text, 0, have), "/1");
        }
        else
        {
            ui::append(text, 0, have > 0 ? "OK" : "NO");
        }

        return need_line(pixels, y, name, text, have > 0);
    }

    int takes_line(game& owner, canvas& pixels, int y, int base_minutes)
    {
        char text[24];
        int length = ui::append(text, 0, "TAKES ");
        length = ui::append_int(text, length, owner.vehicles().minutes(owner, base_minutes));
        ui::append(text, length, " MIN");
        pixels.text(detail_x, y, text, colour::quiet);
        return y + 7;
    }
}

void car_screen::open(game& owner, int vehicle)
{
    _vehicle = vehicle;
    _active = true;
    _dirty = true;
    owner.hud_ref().begin_screen();
}

void car_screen::close(game& owner)
{
    if(_active)
    {
        _active = false;
        owner.hud_ref().end_screen();
    }
}

void car_screen::update(game& owner, const input_frame& input)
{
    vehicle_manager& vehicles = owner.vehicles();

    if(input.b_pressed || input.start_pressed)
    {
        close(owner);
        return;
    }

    if(input.up_pressed || input.down_pressed)
    {
        _row = step_list(_row, row_count, input);
        sfx::play(sfx::id::MOODLE, milli(300));
        _dirty = true;
    }

    if(_row < part_count)
    {
        // A fits, fills or pours; R1 takes the part out. Both take time, so the screen closes while it passes.
        if(input.a_pressed)
        {
            vehicles.fit(owner, _vehicle, car_part(_row));
            _dirty = true;
        }
        else if(input.r_pressed)
        {
            vehicles.remove(owner, _vehicle, car_part(_row));
            _dirty = true;
        }
    }
    else if(input.a_pressed)
    {
        car_action action = car_action(_row - part_count);
        const char* problem;

        if(action == car_action::TRUNK || action == car_action::GLOVE_BOX)
        {
            int container = vehicles.container_of(owner, _vehicle, action == car_action::TRUNK);

            if(container >= 0)
            {
                // B on the search comes back here (playtest 4).
                close(owner);
                owner.search_car_container(container, _vehicle);
                return;
            }

            sfx::play(sfx::id::DENIED, milli(600));
        }
        else if(action == car_action::DRIVE && vehicles.can_do(owner, _vehicle, action, problem))
        {
            close(owner);
            vehicles.act(owner, _vehicle, action);
            return;
        }
        else
        {
            vehicles.act(owner, _vehicle, action);
            _dirty = true;
        }
    }

    if(! _active)
    {
        return;
    }

    // A redraw over two frames: the header and the list, then the selected line's panel and the footer. Each
    // panel fills its own area (the screen was cleared when it opened), so nothing blanks between the two.
    if(_dirty)
    {
        _dirty = false;
        _halves = 2;
    }

    if(_halves == 2)
    {
        _draw_left(owner);
        _halves = 1;
    }
    else if(_halves == 1)
    {
        _draw_right(owner);
        _halves = 0;
    }
}

void car_screen::_draw_left(game& owner)
{
    hud& screen = owner.hud_ref();
    canvas& pixels = screen.pixels();
    vehicle_manager& vehicles = owner.vehicles();
    bool wreck = vehicles.kind(_vehicle) == vehicle_kind::WRECK;
    char text[32];
    int length;
    const char* problem;

    // Header: what it is, and whether it would run.
    bool drives = vehicles.can_do(owner, _vehicle, car_action::DRIVE, problem);
    pixels.panel(header_left, header_top, header_width, header_height, colour::fill, colour::border);
    int x = pixels.text(header_left + 5, header_text_y, wreck ? "WRECK" : "CAR ` ", colour::quiet);

    if(! wreck)
    {
        pixels.text(x, header_text_y, vehicles.name(_vehicle), colour::accent);
    }

    pixels.text_right(header_left + header_width - 6, header_text_y,
                      wreck ? "PARTS ONLY" : drives ? "READY TO DRIVE" : "WILL NOT START",
                      drives ? colour::ui_green : colour::ui_red);

    // Left: the checklist.
    int ready = 0;

    for(int part = 0; part < part_count; ++part)
    {
        ready += vehicles.part_ok(_vehicle, car_part(part));
    }

    pixels.panel(list_left, panels_top, list_width, panels_height, colour::fill, colour::border);
    pixels.text(list_left + 5, panels_top + 4, "PARTS", colour::quiet);
    length = ui::append_int(text, 0, ready);
    ui::append(text, length, "/5");
    pixels.text_right(list_left + list_width - 5, panels_top + 4, text,
                      ready == part_count ? colour::ui_green : colour::ui_orange);
    pixels.rect(list_left + 4, panels_top + 11, list_width - 8, 1, colour::border);

    for(int part = 0; part < part_count; ++part)
    {
        int y = row_y(part);
        bool selected = part == _row;
        bool ok = vehicles.part_ok(_vehicle, car_part(part));

        if(selected)
        {
            pixels.rect(list_left + 2, y - 1, list_width - 4, row_step, colour::accent);
        }

        int mark = selected ? colour::fill : ok ? colour::ui_green : colour::ui_red;

        if(ok)
        {
            tick(pixels, list_left + 5, y, mark);
        }
        else
        {
            cross(pixels, list_left + 5, y, mark);
        }

        pixels.text(list_left + 15, y, part_titles[part], selected ? colour::fill : colour::text);
        vehicles.part_note(_vehicle, car_part(part), text);
        pixels.text_right(list_left + list_width - 6, y, text,
                          selected ? colour::fill : ok ? colour::quiet : colour::ui_red);
    }

    // Then what can be done with the car.
    pixels.rect(list_left + 4, actions_title_y - 5, list_width - 8, 1, colour::border);
    pixels.text(list_left + 5, actions_title_y, "ACTIONS", colour::quiet);

    for(int action = 0; action < action_count; ++action)
    {
        int row = part_count + action;
        int y = row_y(row);
        bool selected = row == _row;
        bool can = vehicles.can_do(owner, _vehicle, car_action(action), problem);
        const char* note = "";

        if(car_action(action) == car_action::HOTWIRE && ! wreck)
        {
            note = vehicles.hotwired(_vehicle) ? "DONE" : vehicles.has_key(owner, _vehicle) ? "HAVE KEY" : "NO KEY";
        }
        else if(car_action(action) == car_action::TRUNK)
        {
            note = "60 KG";
        }
        else if(car_action(action) == car_action::REPAIR && ! wreck)
        {
            length = ui::append_int(text, 0, vehicles.condition(_vehicle));
            ui::append(text, length, "%");
            note = text;
        }

        if(selected)
        {
            pixels.rect(list_left + 2, y - 1, list_width - 4, row_step, colour::accent);
        }

        int ink = selected ? colour::fill : can ? colour::text : colour::body_limb;
        pixels.text(list_left + 5, y, action_titles[action], ink);
        pixels.text_right(list_left + list_width - 6, y, note, selected ? colour::fill : can ? colour::quiet : ink);
    }

}

void car_screen::_draw_right(game& owner)
{
    hud& screen = owner.hud_ref();
    canvas& pixels = screen.pixels();
    vehicle_manager& vehicles = owner.vehicles();
    char text[32];
    int length;
    const char* problem;
    int ready = 0;

    for(int part = 0; part < part_count; ++part)
    {
        ready += vehicles.part_ok(_vehicle, car_part(part));
    }

    // Right: the selected line.
    pixels.panel(detail_left, panels_top, detail_width, panels_height, colour::fill, colour::border);
    int y = panels_top + 23;
    const char* fit_label = nullptr;
    bool can_remove = false;

    if(_row < part_count)
    {
        car_part part = car_part(_row);
        bool ok = vehicles.part_ok(_vehicle, part);
        bool can_fit = vehicles.can_fit(owner, _vehicle, part, problem);
        const char* fit_problem = problem;
        can_remove = vehicles.can_remove(owner, _vehicle, part, problem);
        vehicles.part_note(_vehicle, part, text);
        pixels.text(detail_x, panels_top + 5, part_titles[_row], colour::accent);
        pixels.text(detail_x, panels_top + 15, text[0] ? text : "FITTED", ok ? colour::ui_green : colour::ui_red);
        pixels.rect(detail_x - 1, y + 1, detail_right - detail_x + 2, 1, colour::border);
        pixels.text(detail_x, y + 5, "NEEDS", colour::quiet);
        y += 13;

        // What fitting it takes while it is missing, else what taking it out does.
        bool fitted = vehicles.part_full(_vehicle, part);

        switch(part)
        {

        case car_part::BATTERY:
            y = fitted ? y : need_item(owner, pixels, y, "CAR BATTERY", item_id::CAR_BATTERY, true);
            y = need_item(owner, pixels, y, "WRENCH", item_id::WRENCH, false);
            fit_label = "FIT";
            break;

        case car_part::SPARK_PLUG:
            y = fitted ? y : need_item(owner, pixels, y, "SPARK PLUG", item_id::SPARK_PLUG, true);
            y = need_item(owner, pixels, y, "WRENCH", item_id::WRENCH, false);
            fit_label = "FIT";
            break;

        case car_part::RADIATOR:
            y = need_item(owner, pixels, y, "WATER BOTTLE", item_id::WATER_BOTTLE, true);
            fit_label = "FILL";
            break;

        case car_part::TYRES:
            y = fitted ? y : need_item(owner, pixels, y, "TYRE", item_id::TYRE, true);
            y = need_item(owner, pixels, y, "WRENCH", item_id::WRENCH, false);
            fit_label = "FIT";
            break;

        default:
            y = need_item(owner, pixels, y, "FUEL CAN", item_id::FUEL_CAN, false);
            fit_label = "ADD FUEL";
            break;
        }

        pixels.rect(detail_x - 1, y + 1, detail_right - detail_x + 2, 1, colour::border);
        y = takes_line(owner, pixels, y + 5, part == car_part::FUEL ? vehicle_tuning::fuel_minutes :
                                                                      vehicles.part_work_minutes(owner, part));

        if(part != car_part::FUEL && part != car_part::RADIATOR)
        {
            pixels.text(detail_x, y, "TRAINS MECHANICS", colour::quiet);
            y += 7;
        }

        if(! can_fit && ! can_remove && fit_problem)
        {
            wrap(pixels, y + 7, fit_problem, colour::ui_red);
        }

        if(! can_fit)
        {
            fit_label = nullptr;
        }
    }
    else
    {
        car_action action = car_action(_row - part_count);
        bool can = vehicles.can_do(owner, _vehicle, action, problem);
        const char* action_problem = problem;
        const character& skills = owner.hero().skills();
        pixels.text(detail_x, panels_top + 5, action_titles[_row - part_count], colour::accent);
        y = panels_top + 15;

        switch(action)
        {

        case car_action::TRUNK:
            y += wrap(pixels, y, "STORAGE THAT MOVES WITH THE CAR", colour::quiet) * line_step;
            pixels.text(detail_x, y + 3, "HOLDS 60 KG", colour::text);
            fit_label = "OPEN";
            break;

        case car_action::GLOVE_BOX:
            wrap(pixels, y, "SMALL THINGS: MAPS, TORCHES, MAYBE THE KEY", colour::quiet);
            fit_label = "OPEN";
            break;

        case car_action::SIPHON:
            pixels.text(detail_x, y, "NEEDS", colour::quiet);
            y = need_item(owner, pixels, y + 8, "HOSE", item_id::HOSE, false);
            y = need_item(owner, pixels, y, "FUEL CAN", item_id::FUEL_CAN, false);
            pixels.rect(detail_x - 1, y + 1, detail_right - detail_x + 2, 1, colour::border);
            takes_line(owner, pixels, y + 5, vehicle_tuning::siphon_minutes);
            fit_label = "SIPHON";
            break;

        case car_action::HOTWIRE:
        {
            bool trade = skills.job() == occupation::MECHANIC || skills.job() == occupation::BURGLAR;
            int level = skills.level(skill::MECHANICS);
            pixels.text(detail_x, y, "NEEDS", colour::quiet);
            length = ui::append_int(text, 0, level);
            ui::append(text, length, "/2");
            y = need_line(pixels, y + 8, trade ? "YOUR TRADE" : "MECHANICS", trade ? "OK" : text,
                          trade || level >= vehicle_tuning::hotwire_level);
            pixels.rect(detail_x - 1, y + 1, detail_right - detail_x + 2, 1, colour::border);
            y = takes_line(owner, pixels, y + 5, vehicle_tuning::hotwire_minutes);
            length = ui::append(text, 0, "WORKS ");
            length = ui::append_int(text, length, bn::min(vehicle_tuning::hotwire_percent +
                                                          (level * vehicle_tuning::hotwire_percent_per_level) +
                                                          (trade ? vehicle_tuning::hotwire_job_percent : 0), 100));
            ui::append(text, length, "% ` NOISY");
            pixels.text(detail_x, y, text, colour::quiet);
            fit_label = "HOTWIRE";
            break;
        }

        case car_action::REPAIR:
            y += wrap(pixels, y, "A RADIATOR (+40%) OR BRAKE PADS (+25%)", colour::quiet) * line_step;
            y = need_item(owner, pixels, y + 3, "RADIATOR", item_id::RADIATOR, false);
            y = need_item(owner, pixels, y, "BRAKE PADS", item_id::BRAKE_PADS, false);
            y = need_item(owner, pixels, y, "WRENCH", item_id::WRENCH, false);
            pixels.rect(detail_x - 1, y + 1, detail_right - detail_x + 2, 1, colour::border);
            takes_line(owner, pixels, y + 5, vehicle_tuning::part_minutes);
            fit_label = "REPAIR";
            break;

        default:
            pixels.text(detail_x, y, "NEEDS", colour::quiet);
            length = ui::append_int(text, 0, ready);
            ui::append(text, length, "/5");
            y = need_line(pixels, y + 8, "PARTS", text, ready == part_count);
            y = need_line(pixels, y, "KEY OR HOTWIRE",
                          vehicles.hotwired(_vehicle) || vehicles.has_key(owner, _vehicle) ? "OK" : "NO",
                          vehicles.hotwired(_vehicle) || vehicles.has_key(owner, _vehicle));
            length = ui::append_int(text, 0, vehicles.condition(_vehicle));
            ui::append(text, length, "%");
            y = need_line(pixels, y, "CONDITION", text, vehicles.condition(_vehicle) > 0);
            pixels.rect(detail_x - 1, y + 1, detail_right - detail_x + 2, 1, colour::border);
            pixels.text(detail_x, y + 5, "D-PAD STEER ` A GO", colour::quiet);
            pixels.text(detail_x, y + 12, "B BRAKE, REVERSE", colour::quiet);
            pixels.text(detail_x, y + 19, "L1 OUT ` R1 LIGHTS", colour::quiet);
            y += 19;
            fit_label = "DRIVE";
            break;
        }

        if(! can && action_problem)
        {
            // At the panel's foot; a second line goes above the first.
            int lines = wrap(pixels, 0, action_problem, colour::ui_red, false);
            wrap(pixels, panels_top + panels_height - 12 - ((lines - 1) * line_step), action_problem, colour::ui_red);
            fit_label = nullptr;
        }
    }

    // Footer: what the buttons do on this line; the tracked count when it is nearly full (spec: caps show).
    pixels.rect(0, footer_top, canvas::width, canvas::height - footer_top, canvas::backdrop);
    hud::hint hints[3];
    int count = 0;

    if(fit_label)
    {
        hints[count++] = { "A", fit_label };
    }

    if(can_remove)
    {
        hints[count++] = { ui::button::r, "TAKE OUT" };
    }

    hints[count++] = { "B", "CLOSE" };
    screen.hints(footer_y, hints, count, false);

    if(vehicles.tracked_count() >= tracked_warning)
    {
        length = ui::append(text, 0, "TRACKED ");
        length = ui::append_int(text, length, vehicles.tracked_count());
        length = ui::append(text, length, "/");
        ui::append_int(text, length, vehicle_tuning::max_tracked);
        pixels.text_right(canvas::width - 12, footer_y, text, colour::ui_orange);
    }
}

}
