#include "kx_health_screen.h"

#include "bn_algorithm.h"

#include "kx_game.h"
#include "kx_input.h"
#include "kx_sfx.h"
#include "kx_ui.h"

namespace kx
{

namespace
{
    // Mockup 04 layout in screen pixels.
    constexpr int panels_top = 22;
    constexpr int panels_height = 120;               // playtest 2: ends at 141 like the other tabs' panels
    constexpr int body_panel_left = 12;
    constexpr int body_panel_width = 72;
    constexpr int stats_panel_left = 88;
    constexpr int stats_panel_width = 140;
    constexpr int unit = 3;                          // body diagram: 16x28 units of 3 px
    constexpr int body_left = body_panel_left + ((body_panel_width - (16 * unit)) / 2);
    // Playtest 2: the figure and the part's two label lines (99 px) centred in the panel (it sat 5 px from the top
    // and 14 from the bottom).
    constexpr int body_block_height = (28 * unit) + 4 + 11;
    constexpr int body_top = panels_top + ((panels_height - body_block_height) / 2);
    constexpr int label_top = body_top + (28 * unit) + 4;
    constexpr int stats_left = stats_panel_left + 6;
    constexpr int stats_right = stats_panel_left + stats_panel_width - 7;
    constexpr int stats_top = panels_top + 5;
    constexpr int stat_step = 18;                    // label line, 2 px, 6 px bar, 4 px gap
    constexpr int bar_offset = 8;
    constexpr int bar_height = 6;
    constexpr int chips_top = stats_top + (5 * stat_step) - 1;
    constexpr int chip_height = 10;
    constexpr int chip_rows = 2;
    constexpr int footer_y = 147;
    constexpr int blink_frames = 16;

    // Body parts in the order Up/Down walks through them.
    constexpr body_part part_order[] = { body_part::HEAD, body_part::TORSO, body_part::LEFT_ARM, body_part::RIGHT_ARM,
                                         body_part::LEFT_LEG, body_part::RIGHT_LEG };

    // Body diagram rectangles in units, per body_part (the figure faces you: its left arm is on the right).
    struct body_rect
    {
        body_part part;
        uint8_t x, y, width, height;
    };

    constexpr body_rect body_rects[] = {
        { body_part::HEAD, 6, 0, 4, 4 }, { body_part::HEAD, 7, 4, 2, 1 },
        { body_part::TORSO, 4, 5, 8, 9 },
        { body_part::RIGHT_ARM, 2, 5, 2, 9 }, { body_part::RIGHT_ARM, 2, 14, 2, 2 },
        { body_part::LEFT_ARM, 12, 5, 2, 9 }, { body_part::LEFT_ARM, 12, 14, 2, 2 },
        { body_part::RIGHT_LEG, 5, 14, 3, 12 }, { body_part::RIGHT_LEG, 4, 26, 4, 2 },
        { body_part::LEFT_LEG, 8, 14, 3, 12 }, { body_part::LEFT_LEG, 8, 26, 4, 2 },
    };

    // Blood drops under a bleeding part.
    constexpr body_rect drop_rects[] = {
        { body_part::LEFT_ARM, 13, 17, 1, 1 }, { body_part::LEFT_ARM, 12, 19, 1, 1 },
        { body_part::RIGHT_ARM, 2, 17, 1, 1 }, { body_part::RIGHT_ARM, 3, 19, 1, 1 },
        { body_part::LEFT_LEG, 12, 22, 1, 1 }, { body_part::LEFT_LEG, 12, 24, 1, 1 },
        { body_part::RIGHT_LEG, 3, 22, 1, 1 }, { body_part::RIGHT_LEG, 3, 24, 1, 1 },
    };

    [[nodiscard]] bool bleeding(const body_wound& wound)
    {
        return wound.type != wound_type::NONE && wound.type != wound_type::FRACTURE &&
               wound.bandage == bandage_state::NONE;
    }

    void stat(canvas& pixels, int index, const char* label, const char* value, int value_colour, int bar_colour,
              int amount, int maximum)
    {
        int y = stats_top + (index * stat_step);
        pixels.text(stats_left, y, label, colour::text);
        pixels.text_right(stats_right, y, value, value_colour);

        int bar_y = y + bar_offset;
        int inner = stats_right - stats_left - 1;
        int permille = maximum > 0 ? bn::clamp((amount * 1000) / maximum, 0, 1000) : 0;
        pixels.panel(stats_left, bar_y, inner + 2, bar_height, colour::panel, colour::border);
        pixels.rect(stats_left + 1, bar_y + 1, (inner * permille) / 1000, bar_height - 2, bar_colour);
    }

    // "42 %" plus the moodle name when it is active ("18 % ` THIRSTY", '`' draws a middle dot).
    int percent_text(char* text, int value, const survival& body, moodle related)
    {
        int length = ui::append_int(text, 0, value);
        length = ui::append(text, length, " %");

        if(body.level(related) > 0)
        {
            length = ui::append(text, length, " ` ");
            length = ui::append(text, length, moodle_name(related));
        }

        return length;
    }

    // Value colour: the moodle's chip text colour when it is active (mockup 04: injuries red, cold blue,
    // needs orange; anything at level 3 or worse red).
    [[nodiscard]] int warning_colour(const survival& body, moodle related)
    {
        int level = body.level(related);

        if(level == 0)
        {
            return colour::text;
        }

        switch(related)
        {

        case moodle::COLD:
        case moodle::WET:
            return level >= 3 ? colour::stats_pink : colour::stats_cold_text;

        case moodle::PAIN:
        case moodle::BLEEDING:
        case moodle::SICK:
        case moodle::FEVER:
        case moodle::PANIC:
            return colour::stats_pink;

        default:
            return level >= 3 ? colour::stats_pink : colour::stats_warm_text;
        }
    }
}

void health_screen::open(game& owner)
{
    _active = true;
    _dirty = true;
    owner.hud_ref().begin_screen();
}

void health_screen::close(game& owner)
{
    if(_active)
    {
        _active = false;
        owner.hud_ref().end_screen();
    }
}

void health_screen::update(game& owner, const input_frame& input)
{
    if(input.b_pressed || input.start_pressed)
    {
        close(owner);
        return;
    }

    if(input.up_pressed || input.down_pressed)
    {
        int count = int(sizeof(part_order) / sizeof(part_order[0]));
        _part = (_part + (input.down_pressed ? 1 : count - 1)) % count;
        _blink = 0;
        _dirty = true;
    }

    if(input.a_pressed)
    {
        player& hero = owner.hero();

        bool can_stitch = hero.skills().level(skill::FIRST_AID) >= character_tuning::stitch_level;

        if(hero.body().treat(part_order[_part], hero.items(), can_stitch))
        {
            sfx::play(sfx::id::BANDAGE, milli(800));
            hero.body().quicken_healing(part_order[_part], hero.skills().level(skill::FIRST_AID) *
                                        character_tuning::first_aid_heal_percent);
            hero.train(owner, skill::FIRST_AID, character_tuning::treat_xp);
        }
        else
        {
            sfx::play(sfx::id::DENIED, milli(600));
        }

        _dirty = true;
    }

    if(_dirty)
    {
        _draw(owner);
    }
    else if(++_blink % blink_frames == 0)
    {
        _draw_body(owner);
    }
}

void health_screen::_draw_body(game& owner)
{
    // The selected part blinks in the accent colour; wounded parts are red, bleeding ones drip (mockup 04).
    canvas& pixels = owner.hud_ref().pixels();
    const survival& body = owner.hero().body();
    body_part selected = part_order[_part];
    bool highlight = (_blink / blink_frames) % 2 == 0;
    pixels.rect(body_left, body_top, 16 * unit, 28 * unit, colour::fill);

    for(const body_rect& value : body_rects)
    {
        int part_colour = value.part == selected && highlight ? colour::accent :
                          body.wound(value.part).type != wound_type::NONE ? colour::body_wound : colour::body_limb;
        pixels.rect(body_left + (value.x * unit), body_top + (value.y * unit), value.width * unit,
                    value.height * unit, part_colour);
    }

    pixels.rect(body_left + (7 * unit), body_top + (5 * unit), 2 * unit, 9 * unit, colour::body_seam);

    for(const body_rect& value : drop_rects)
    {
        if(bleeding(body.wound(value.part)))
        {
            pixels.rect(body_left + (value.x * unit), body_top + (value.y * unit), value.width * unit,
                        value.height * unit, colour::body_wound);
        }
    }
}

void health_screen::_draw(game& owner)
{
    hud& screen = owner.hud_ref();
    canvas& pixels = screen.pixels();
    const player& hero = owner.hero();
    const survival& body = hero.body();
    const inventory& items = hero.items();
    body_part selected = part_order[_part];
    char text[48];
    int length;
    _dirty = false;

    pixels.rect(0, 0, canvas::width, canvas::height, canvas::backdrop);
    screen.tab_bar(1);

    // Body diagram and the selected part's state.
    pixels.panel(body_panel_left, panels_top, body_panel_width, panels_height, colour::fill, colour::border);
    _draw_body(owner);

    const body_wound& wound = body.wound(selected);
    constexpr int label_center = body_panel_left + (body_panel_width / 2);

    if(wound.type == wound_type::NONE)
    {
        pixels.text_centered(label_center, label_top, body_part_name(selected), colour::text);
        pixels.text_centered(label_center, label_top + 6, "OK", colour::quiet);
    }
    else
    {
        const char* state = wound.type == wound_type::FRACTURE ? (wound.splinted ? "SPLINTED" : "NO SPLINT") :
                            wound.infected ? "INFECTED" :
                            wound.bandage == bandage_state::CLEAN ? "BANDAGED" :
                            wound.bandage == bandage_state::DIRTY ? "DIRTY BAND." :
                            wound.stitched ? "STITCHED" : "BLEEDING";
        bool treated = (wound.bandage == bandage_state::CLEAN || wound.stitched) && ! wound.infected;
        pixels.text_centered(label_center, label_top, body_part_name(selected), colour::body_pink);
        pixels.text_centered(label_center, label_top + 6, wound_name(wound.type), colour::body_pink);
        pixels.text_centered(label_center, label_top + 12, state, treated ? colour::quiet : colour::body_pink);
    }

    // Vitals.
    pixels.panel(stats_panel_left, panels_top, stats_panel_width, panels_height, colour::fill, colour::border);

    length = ui::append_int(text, 0, body.blood());
    ui::append(text, length, " / 5000 ML");
    stat(pixels, 0, "BLOOD", text, body.blood() < 4000 ? colour::stats_pink : colour::text, colour::stats_blood,
         body.blood(), 5000);

    percent_text(text, body.food_percent(), body, moodle::HUNGRY);
    stat(pixels, 1, "FOOD", text, warning_colour(body, moodle::HUNGRY), colour::stats_food, body.food_percent(), 100);

    percent_text(text, body.water_percent(), body, moodle::THIRSTY);
    stat(pixels, 2, "WATER", text, warning_colour(body, moodle::THIRSTY), colour::stats_water, body.water_percent(),
         100);

    int temperature = body.body_temperature();
    length = ui::append_int(text, 0, temperature / 10);
    text[length++] = '.';
    text[length++] = char('0' + (temperature % 10));
    length = ui::append(text, length, " ^C");
    moodle temperature_moodle = body.level(moodle::FEVER) ? moodle::FEVER :
                                body.level(moodle::COLD) ? moodle::COLD : moodle::HOT;

    if(body.level(temperature_moodle))
    {
        length = ui::append(text, length, " ` ");
        ui::append(text, length, moodle_name(temperature_moodle));
    }

    stat(pixels, 3, "BODY TEMP", text, warning_colour(body, temperature_moodle), colour::stats_cold,
         temperature - 340, 60);

    length = ui::append_weight(text, 0, items.weight());
    length = ui::append(text, length, " / ");
    length = ui::append_weight(text, length, items.capacity());
    ui::append(text, length, " KG");
    stat(pixels, 4, "CARRY", text, items.heavy() ? colour::stats_warm_text : colour::text, colour::quiet,
         items.weight(), items.capacity());

    // Active moodles as chips, worst first: a 1 px border in the moodle colour around its name.
    int x = stats_left;
    int row = 0;

    for(int level = 4; level >= 1 && row < chip_rows; --level)
    {
        for(int index = 0; index < int(moodle::COUNT) && row < chip_rows; ++index)
        {
            moodle value = moodle(index);

            if(body.level(value) != level)
            {
                continue;
            }

            const char* name = moodle_name(value);
            int width = canvas::text_width(name) + 8;

            if(x + width > stats_right + 1)
            {
                if(++row >= chip_rows)
                {
                    break;
                }

                x = stats_left;
            }

            int text_colour = warning_colour(body, value);
            int border = text_colour == colour::stats_pink ? colour::stats_blood :
                         text_colour == colour::stats_cold_text ? colour::stats_cold : colour::stats_food;
            int y = chips_top + (row * (chip_height + 2));
            pixels.frame(x, y, width, chip_height, border);
            pixels.text(x + 4, y + 3, name, text_colour);
            x += width + 3;
        }
    }

    // Footer: the treatment A would apply, with how many of that item are carried.
    const char* label;
    item_id treatment = body.treatment(selected, items, &label,
                                       hero.skills().level(skill::FIRST_AID) >= character_tuning::stitch_level);
    length = ui::append(text, 0, label);

    if(treatment != item_id::NONE)
    {
        int count = 0;

        for(int slot = 0; slot < inventory::slots; ++slot)
        {
            if(items.slot(slot).id == treatment)
            {
                count += items.slot(slot).quantity();
            }
        }

        length = ui::append(text, length, " (");
        length = ui::append_int(text, length, count);
        ui::append(text, length, ")");
    }

    // As mockup 04: the tab bar shows L1/R1; the footer's right side holds the save and quit hint.
    hud::hint hints[] = { { "A", text }, { "B", "BACK" } };
    screen.hints(footer_y, hints, 2, false);
}

}
