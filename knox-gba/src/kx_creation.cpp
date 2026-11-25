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
    constexpr int info_top = 123;               // playtest 6: the box ends on the standard frame's line (141)
    constexpr int info_height = 19;
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

    // What Start asks about before it begins (playtest 6): points left unspent, the saved world it replaces.
    struct begin_check
    {
        bool replaces = false;      // a new world, with a saved one it replaces
        bool asking = false;        // Start was pressed once; again begins
    };

    void draw(canvas& pixels, const character_choice& choice, const cursor& focus, const begin_check& check)
    {
        char text[48];
        int length;
        pixels.rect(0, 0, canvas::width, canvas::height, colour::ui_screen);

        // Header and points.
        int left_points = points(choice);
        pixels.panel(left, header_top, width, 14, colour::fill, colour::border);
        pixels.text(text_x, header_top + 5, "NEW SURVIVOR", colour::accent);
        ui::append_int(text, 0, left_points);
        // 4 px inside the border at both ends (playtest 6: 2 px on the right).
        pixels.text_right(right - 1, header_top + 5, text, left_points >= 0 ? colour::ui_green : colour::ui_red);
        pixels.text_right(right - 1 - canvas::text_width(text) - 4, header_top + 5, "POINTS", colour::quiet);

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

        // Scroll marks on the heading line (playtest 6: they sat on the panel's edges and a trait's cost).
        pixels.scroll_marks(column_right[1], traits_top + 4, focus.scroll > 0, focus.scroll + visible_pairs < pairs);

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

            // Playtest 6: an outline when not taken, a tick when taken, so a taken trait shows under the cursor too.
            pixels.tick_box(x - 1, y, chosen, focused ? colour::fill : chosen ? colour::ui_green : colour::border);
            pixels.text(x + 8, y, info(value).name, focused ? colour::fill : chosen ? colour::text : colour::quiet);
            length = ui::append(text, 0, column == 0 ? "-" : "+");
            ui::append_int(text, length, info(value).points);
            pixels.text_right(column_right[column], y, text,
                              focused ? colour::fill : column == 0 ? colour::ui_orange : colour::ui_green);
        }

        // What the focused choice does; while the points are negative (too many good traits, or an occupation
        // with fewer points), what to give back instead.
        bool over_spent = left_points < 0;
        pixels.panel(left, info_top, width, info_height, colour::fill,
                     over_spent || (check.asking && check.replaces) ? colour::ui_red : colour::accent);

        if(check.asking)
        {
            // Start once more begins; anything else goes back to choosing.
            if(left_points > 0)
            {
                length = ui::append_int(text, 0, left_points);
                ui::append(text, length, left_points == 1 ? " POINT LEFT TO SPEND" : " POINTS LEFT TO SPEND");
                pixels.text(text_x, info_top + 4, text, colour::accent);
            }

            if(check.replaces)
            {
                pixels.text(text_x, info_top + (left_points > 0 ? 11 : 4), "THIS REPLACES THE SAVED WORLD",
                            colour::ui_red);
            }

            pixels.text_right(right + 1, info_top + 4, "START AGAIN", colour::text);
            pixels.text_right(right + 1, info_top + 11, "TO BEGIN", colour::quiet);
        }
        else if(over_spent)
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
            // Taking it drops its opposite (playtest 6: the points jumped by both with no word why).
            bool replaces = ! lone_trait(value) && ! (choice.traits & (1u << int(value))) &&
                            (choice.traits & (1u << int(opposite(value))));

            if(replaces)
            {
                length = ui::append(text, 0, "REPLACES ");
                ui::append(text, length, info(opposite(value)).name);
                pixels.text_right(right + 1, info_top + 4, text, colour::text);
            }
            else
            {
                length = ui::append(text, 0, good_trait(value) ? "COSTS " : "GIVES ");
                ui::append_int(text, length, value_info.points);
                pixels.text_right(right + 1, info_top + 4, text,
                                  good_trait(value) ? colour::ui_orange : colour::ui_green);
            }
            pixels.text(text_x, info_top + 11, value_info.effect, colour::quiet);
        }

        // The footer of every screen (playtest 6: B BACK sat at the right edge): the main action first, B after it.
        int x = pixels.text(12, footer_y, "A", colour::text) + 4;
        x = pixels.text(x, footer_y, "PICK", colour::quiet) + 10;
        // Start is greyed out until the points add up.
        x = pixels.text(x, footer_y, "START", over_spent ? colour::border : colour::text) + 4;
        x = pixels.text(x, footer_y, "BEGIN", over_spent ? colour::border : colour::quiet) + 10;
        x = pixels.text(x, footer_y, "B", colour::text) + 4;
        x = pixels.text(x, footer_y, "BACK", colour::quiet) + 10;
        x = pixels.text(x, footer_y, "D-PAD", colour::text) + 4;
        pixels.text(x, footer_y, "CHOOSE", colour::quiet);
    }
}

bool character_creation(character_choice& result, bool replaces_save)
{
    music::title();

    canvas pixels;
    character_choice choice;
    cursor focus;
    begin_check check;
    check.replaces = replaces_save;
    draw(pixels, choice, focus, check);

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
                // Rows wrap within the column (playtest 6: Down from Eagle-Eyed went across to the Smoker, the bad
                // column's lone last row); up from the first goes to the occupation.
                do
                {
                    focus.row = (focus.row + (down ? 1 : pairs - 1)) % pairs;
                }
                while(lone_trait(trait(focus.row * 2)) && focus.column != (good_trait(trait(focus.row * 2)) ? 0 : 1));

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

        if(check.asking && (changed || up || down || left_pressed || right_pressed || pick))
        {
            check.asking = false;
            changed = true;
        }

        if(begin)
        {
            int left_points = points(choice);

            if(left_points < 0)
            {
                sfx::play(sfx::id::DENIED, milli(600));
            }
            else if(check.asking || (left_points == 0 && ! check.replaces))
            {
                result = choice;
                return true;
            }
            else
            {
                // Once more begins (playtest 6: unspent points, the saved world replaced).
                check.asking = true;
                sfx::play(sfx::id::MOODLE, milli(500));
                changed = true;
            }
        }

        if(bn::keypad::b_pressed())
        {
            return false;
        }

        if(changed)
        {
            draw(pixels, choice, focus, check);
        }

        bn::core::update();
    }
}

}
