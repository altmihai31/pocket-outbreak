#include "kx_creation.h"
#include "kx_music.h"

#include "bn_algorithm.h"
#include "bn_core.h"
#include "bn_keypad.h"

#include "kx_bench.h"
#include "kx_canvas.h"
#include "kx_sfx.h"
#include "kx_ui.h"

namespace kx
{

namespace
{
    // Mockup 08 layout in screen pixels.
    constexpr int left = 12;
    constexpr int width = 216;
    constexpr int right = left + width - 5;
    constexpr int text_x = left + 5;
    constexpr int center_x = canvas::width / 2;
    constexpr int header_top = 4;
    constexpr int job_top = 22;
    constexpr int traits_top = 50;
    constexpr int traits_height = 70;
    constexpr int rows_top = 63;
    constexpr int row_step = 7;
    constexpr int pairs = (trait_count + 1) / 2;   // the Smoker's row has no good trait
    constexpr int visible_pairs = 8;            // more scroll (arrows show there are more)
    constexpr int column_left[2] = { 17, 123 };
    constexpr int column_right[2] = { 116, 223 };
    constexpr int info_top = 124;
    constexpr int footer_y = 147;

    // The focus: the occupation row, or a trait (column 0 good, 1 bad; row = pair).
    struct cursor
    {
        bool on_job = true;
        int column = 0;
        int row = 0;
        int scroll = 0;                 // first pair shown

        void follow()
        {
            scroll = bn::clamp(scroll, row - visible_pairs + 1, row);
            scroll = bn::clamp(scroll, 0, bn::max(pairs - visible_pairs, 0));
        }

        [[nodiscard]] trait focused() const
        {
            // A lone trait's row: whichever column, its one trait.
            int index = (row * 2) + column;
            return index >= trait_count || lone_trait(trait((row * 2))) ? trait(row * 2) : trait(index);
        }

        // A row holding one trait keeps the focus in that trait's column.
        void settle()
        {
            if(lone_trait(trait(row * 2)))
            {
                column = good_trait(trait(row * 2)) ? 0 : 1;
            }
        }
    };

    [[nodiscard]] int points(const character_choice& choice)
    {
        int result = info(choice.job).bonus_points;

        for(int index = 0; index < trait_count; ++index)
        {
            if(choice.traits & (1u << index))
            {
                int cost = info(trait(index)).points;
                result += good_trait(trait(index)) ? -cost : cost;
            }
        }

        return result;
    }

    // "AXE 1 ` FITNESS 3 ` STRENGTH 3": what the occupation starts with (body stats as their resulting level).
    void job_skills(char* output, occupation job)
    {
        const occupation_info& job_info = info(job);
        int length = 0;

        if(job_info.bonus_points)
        {
            length = ui::append(output, length, "+");
            length = ui::append_int(output, length, job_info.bonus_points);
            ui::append(output, length, " TRAIT POINTS");
            return;
        }

        for(int index = 0; index < job_info.skill_count; ++index)
        {
            const starting_skill& value = job_info.skills[index];

            if(index)
            {
                length = ui::append(output, length, " ` ");
            }

            length = ui::append(output, length, info(value.value).name);
            length = ui::append(output, length, " ");
            length = ui::append_int(output, length, value.levels + (body_stat(value.value) ? character_tuning::body_base : 0));
        }

        output[length] = 0;
    }

    void draw(canvas& pixels, const character_choice& choice, const cursor& focus)
    {
        char text[48];
        int length;
        pixels.rect(0, 0, canvas::width, canvas::height, colour::ui_screen);

        // Header and points.
        int left_points = points(choice);
        pixels.panel(left, header_top, width, 14, colour::fill, colour::border);
        pixels.text(text_x, header_top + 5, "NEW SURVIVOR", colour::accent);
        ui::append_int(text, 0, left_points);
        pixels.text_right(right + 1, header_top + 5, text, left_points >= 0 ? colour::ui_green : colour::ui_red);
        pixels.text_right(right + 1 - canvas::text_width(text) - 4, header_top + 5, "POINTS", colour::quiet);

        // Occupation, centred over what it starts with.
        pixels.panel(left, job_top, width, 24, colour::fill, colour::border);
        length = ui::append(text, 0, "<  ");
        length = ui::append(text, length, info(choice.job).name);
        ui::append(text, length, "  >");
        pixels.text_centered(center_x, job_top + 5, text, focus.on_job ? colour::accent : colour::text);
        job_skills(text, choice.job);
        pixels.text_centered(center_x, job_top + 14, text, colour::quiet);

        // Traits.
        pixels.panel(left, traits_top, width, traits_height, colour::fill, colour::border);
        pixels.text(column_left[0], traits_top + 4, "GOOD TRAITS", colour::quiet);
        pixels.text(column_left[1] - 1, traits_top + 4, "BAD TRAITS", colour::quiet);
        pixels.rect(center_x - 1, traits_top + 4, 1, traits_height - 8, colour::border);

        // Small arrows on the panel's top and bottom edges, over the divider, when more pairs are above or below.
        if(focus.scroll > 0)
        {
            pixels.rect(center_x - 1, traits_top, 1, 1, colour::text);
            pixels.rect(center_x - 2, traits_top + 1, 3, 1, colour::text);
            pixels.rect(center_x - 3, traits_top + 2, 5, 1, colour::text);
        }

        if(focus.scroll + visible_pairs < pairs)
        {
            int bottom = traits_top + traits_height - 1;
            pixels.rect(center_x - 3, bottom - 2, 5, 1, colour::text);
            pixels.rect(center_x - 2, bottom - 1, 3, 1, colour::text);
            pixels.rect(center_x - 1, bottom, 1, 1, colour::text);
        }

        for(int index = 0; index < trait_count; ++index)
        {
            trait value = trait(index);
            int column = good_trait(value) ? 0 : 1;
            int row = index / 2;

            if(row < focus.scroll || row >= focus.scroll + visible_pairs)
            {
                continue;
            }

            int y = rows_top + ((row - focus.scroll) * row_step);
            int x = column_left[column];
            bool focused = ! focus.on_job && focus.column == column && focus.row == row;
            bool chosen = choice.traits & (1u << index);

            if(focused)
            {
                pixels.rect(x - 2, y - 1, column_right[column] - x + 4, row_step, colour::accent);
            }

            pixels.rect(x, y, 5, 5, chosen ? (focused ? colour::fill : colour::accent) : colour::empty);
            pixels.text(x + 8, y, info(value).name, focused ? colour::fill : chosen ? colour::text : colour::quiet);
            length = ui::append(text, 0, column == 0 ? "-" : "+");
            ui::append_int(text, length, info(value).points);
            pixels.text_right(column_right[column], y, text,
                              focused ? colour::fill : column == 0 ? colour::ui_orange : colour::ui_green);
        }

        // What the focused choice does; while the points are negative (too many good traits, or an occupation
        // with fewer points), what to give back instead.
        bool over_spent = left_points < 0;
        pixels.panel(left, info_top, width, 19, colour::fill, over_spent ? colour::ui_red : colour::accent);

        if(over_spent)
        {
            pixels.text(text_x, info_top + 4, "TOO MANY TRAITS", colour::ui_red);
            length = ui::append(text, 0, "GIVE BACK ");
            ui::append_int(text, length, -left_points);
            pixels.text_right(right + 1, info_top + 4, text, colour::ui_red);
            pixels.text(text_x, info_top + 11, "UNTICK GOOD TRAITS OR TICK BAD ONES", colour::quiet);
        }
        else if(focus.on_job)
        {
            const occupation_info& job_info = info(choice.job);
            pixels.text(text_x, info_top + 4, job_info.name, colour::accent);
            pixels.text(text_x, info_top + 11, job_info.perk ? job_info.perk : "NO PERK", colour::quiet);
        }
        else
        {
            trait value = focus.focused();
            const trait_info& value_info = info(value);
            pixels.text(text_x, info_top + 4, value_info.name, colour::accent);
            length = ui::append(text, 0, good_trait(value) ? "COSTS " : "GIVES ");
            ui::append_int(text, length, value_info.points);
            pixels.text_right(right + 1, info_top + 4, text, good_trait(value) ? colour::ui_orange : colour::ui_green);
            pixels.text(text_x, info_top + 11, value_info.effect, colour::quiet);
        }

        int x = pixels.text(12, footer_y, "A", colour::text) + 4;
        x = pixels.text(x, footer_y, "PICK", colour::quiet) + 10;
        x = pixels.text(x, footer_y, "D-PAD", colour::text) + 4;
        x = pixels.text(x, footer_y, "MOVE", colour::quiet) + 10;
        // Start is greyed out until the points add up.
        x = pixels.text(x, footer_y, "START", over_spent ? colour::border : colour::text) + 4;
        pixels.text(x, footer_y, "BEGIN", over_spent ? colour::border : colour::quiet);
        pixels.text_right(228, footer_y, "BACK", colour::quiet);
        pixels.text_right(228 - canvas::text_width("BACK") - 4, footer_y, "B", colour::text);
    }
}

bool character_creation(character_choice& result)
{
    music::title();

    canvas pixels;
    character_choice choice;
    cursor focus;
    draw(pixels, choice, focus);

    #if KX_DUMP
        int frames = 0;
    #endif

    while(true)
    {
        bool changed = false;
        bool up = bn::keypad::up_pressed();
        bool down = bn::keypad::down_pressed();
        bool left_pressed = bn::keypad::left_pressed();
        bool right_pressed = bn::keypad::right_pressed();
        bool pick = bn::keypad::a_pressed();
        bool begin = bn::keypad::start_pressed();

        #if KX_DUMP
            // DUMP=1: a Fire officer, Strong, Clumsy and Slow Healer, as in the mockup; then an occupation change
            // with the points spent; log each screen, then begin.
            ++frames;

            if(frames == 2)
            {
                choice.job = occupation::FIRE_OFFICER;
                choice.traits = (1u << int(trait::STRONG)) | (1u << int(trait::CLUMSY)) |
                                (1u << int(trait::SLOW_HEALER));
                focus.on_job = false;
                changed = true;
            }
            else if(frames == 30)
            {
                pixels.dump(3);

                // Then Unemployed with its 8 points spent on Strong and Athletic, on the occupation row...
                choice.job = occupation::UNEMPLOYED;
                choice.traits = (1u << int(trait::STRONG)) | (1u << int(trait::ATHLETIC));
                focus.on_job = true;
                changed = true;
            }
            else if(frames == 40)
            {
                pixels.dump(4);
            }
            else if(frames == 42)
            {
                right_pressed = true;           // ...switched to Mechanic (no bonus points)
            }
            else if(frames == 50)
            {
                pixels.dump(5);
            }
            else if(frames == 52)
            {
                begin = true;                   // refused while the points are negative
            }
            else if(frames == 54)
            {
                // The trait list scrolled to its last row (milestone 13: the Smoker alone), Eagle-eyed and the
                // Smoker ticked.
                choice.traits = (1u << int(trait::EAGLE_EYED)) | (1u << int(trait::SMOKER));
                focus.on_job = false;
                focus.column = 0;
                focus.row = pairs - 1;
                focus.settle();
                focus.follow();
                changed = true;
            }
            else if(frames == 58)
            {
                pixels.dump(7);
            }
            else if(frames == 60)
            {
                pixels.dump(6);
                choice.traits = 0;
                begin = true;
            }
        #endif

        if(focus.on_job)
        {
            if(left_pressed || right_pressed)
            {
                choice.job = occupation((int(choice.job) + (right_pressed ? 1 : occupation_count - 1)) %
                                        occupation_count);
                changed = true;
            }
            else if(down)
            {
                focus.on_job = false;
                changed = true;
            }
        }
        else
        {
            if(up && focus.row == 0)
            {
                focus.on_job = true;
                changed = true;
            }
            else if(up || down)
            {
                focus.row = (focus.row + (down ? 1 : pairs - 1)) % pairs;
                focus.settle();
                focus.follow();
                changed = true;
            }
            else if(left_pressed || right_pressed)
            {
                focus.column = 1 - focus.column;
                focus.settle();
                changed = true;
            }
            else if(pick)
            {
                // A trait and its opposite can't both be taken.
                trait value = focus.focused();
                uint32_t bit = 1u << int(value);
                choice.traits = lone_trait(value) ? choice.traits ^ bit :
                                                    (choice.traits ^ bit) & ~(1u << int(opposite(value)));
                sfx::play(sfx::id::MOODLE, milli(500));
                changed = true;
            }
        }

        if(begin)
        {
            if(points(choice) >= 0)
            {
                result = choice;
                return true;
            }

            sfx::play(sfx::id::DENIED, milli(600));
        }

        if(bn::keypad::b_pressed())
        {
            return false;
        }

        if(changed)
        {
            draw(pixels, choice, focus);
        }

        bn::core::update();
    }
}

}
