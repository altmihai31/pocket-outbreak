#include "kx_craft_screen.h"

#include "kx_game.h"
#include "kx_hud_tiles.h"
#include "kx_input.h"
#include "kx_recipes.h"
#include "kx_sfx.h"
#include "kx_ui.h"

namespace kx
{

namespace
{
    // Mockup 09 layout in screen pixels.
    constexpr int panels_top = 22;
    constexpr int panels_height = 120;
    constexpr int list_left = 12;
    constexpr int list_width = 120;
    constexpr int rows_top = 37;
    constexpr int row_step = 7;
    constexpr int visible_rows = 14;            // milestone 14: more recipes than fit; the list scrolls
    constexpr int detail_left = 136;
    constexpr int detail_width = 92;
    constexpr int detail_x = detail_left + 5;
    constexpr int detail_right = detail_left + detail_width - 6;  // 4 px clear inside each border
    constexpr int footer_top = 144;
    constexpr int footer_y = 147;

    void need_line(canvas& pixels, int y, const char* name, const char* have, bool ok)
    {
        pixels.text(detail_x, y, name, colour::text);
        pixels.text_right(detail_right, y, have, ok ? colour::ui_green : colour::ui_red);
    }
}

void craft_screen::open(game& owner)
{
    _active = true;
    _dirty = true;
    _selected = 0;
    _sort(owner);
    owner.hud_ref().begin_screen();
}

void craft_screen::close(game& owner)
{
    if(_active)
    {
        _active = false;
        owner.hud_ref().end_screen();
    }
}

void craft_screen::_sort(game& owner)
{
    int count = 0;

    for(int ready = 1; ready >= 0; --ready)
    {
        for(int index = 0; index < recipe_count; ++index)
        {
            const char* problem;

            if(owner.recipe_ready(index, problem) == bool(ready))
            {
                _order[count++] = index;
            }
        }
    }
}

void craft_screen::update(game& owner, const input_frame& input)
{
    if(input.b_pressed || input.start_pressed)
    {
        close(owner);
        return;
    }

    if(input.up_pressed || input.down_pressed)
    {
        _selected = step_list(_selected, recipe_count, input);
        sfx::play(sfx::id::MOODLE, milli(300));
        _dirty = true;
    }

    if(input.a_pressed)
    {
        const char* problem;

        if(owner.recipe_ready(_order[_selected], problem))
        {
            close(owner);
            owner.cook(_order[_selected]);
            return;
        }

        owner.toast(problem, hud_tiles::bank_danger, 60);
        sfx::play(sfx::id::DENIED, milli(600));
    }

    if(_dirty)
    {
        _draw(owner);
    }
}

void craft_screen::_draw(game& owner)
{
    hud& screen = owner.hud_ref();
    canvas& pixels = screen.pixels();
    char text[32];
    int length;
    _dirty = false;

    pixels.rect(0, 0, canvas::width, canvas::height, canvas::backdrop);
    screen.tab_bar(2);

    // The heat source nearby, then the recipes.
    heat_kind heat = owner.heat_source();
    pixels.panel(list_left, panels_top, list_width, panels_height, colour::fill, colour::border);
    pixels.text(list_left + 5, panels_top + 4, "HEAT", colour::quiet);
    pixels.text_right(list_left + list_width - 5, panels_top + 4, heat_name(heat),
                      heat == heat_kind::NONE ? colour::ui_red : colour::ui_green);
    pixels.rect(list_left + 4, panels_top + 11, list_width - 8, 1, colour::border);

    // Scrolled so the selected recipe shows, with marks when more lie above or below.
    int first = bn::clamp(_selected - (visible_rows / 2), 0, bn::max(recipe_count - visible_rows, 0));

    static_assert(recipe_count <= int(sizeof(_order) / sizeof(_order[0])));
    int arrow_x = list_left + list_width - 8;

    for(int step = 0; step < 3; ++step)
    {
        if(first > 0)
        {
            pixels.rect(arrow_x - step, rows_top - 4 + step, 1 + (step * 2), 1, colour::quiet);
        }

        if(first + visible_rows < recipe_count)
        {
            pixels.rect(arrow_x - step, rows_top + (visible_rows * row_step) + 1 - step, 1 + (step * 2), 1,
                        colour::quiet);
        }
    }

    for(int row = first; row < recipe_count && row < first + visible_rows; ++row)
    {
        int index = _order[row];
        int y = rows_top + ((row - first) * row_step);
        bool selected = row == _selected;
        const char* problem;
        bool ready = owner.recipe_ready(index, problem);

        if(selected)
        {
            pixels.rect(list_left + 2, y - 1, list_width - 4, row_step, colour::accent);
        }

        pixels.text(list_left + 5, y, recipes[index].name,
                    selected ? colour::fill : ready ? colour::text : colour::body_limb);

        if(ready)
        {
            pixels.rect(list_left + list_width - 9, y + 1, 3, 3, selected ? colour::fill : colour::ui_green);
        }
    }

    // The selected recipe: what it needs, then what it makes.
    const recipe_info& recipe = recipes[_order[_selected]];
    pixels.panel(detail_left, panels_top, detail_width, panels_height, colour::fill, colour::border);
    pixels.text(detail_x, panels_top + 5, recipe.name, colour::accent);
    pixels.text(detail_x, panels_top + 15, "NEEDS", colour::quiet);
    int y = panels_top + 23;

    if(recipe.tool != item_id::NONE)
    {
        bool kettle = kettle_does(recipe) && owner.carries(recipe.tool) <= 0 && owner.carries(item_id::KETTLE) > 0;
        bool has = kettle || owner.carries(recipe.tool) > 0;
        need_line(pixels, y, kettle ? "KETTLE" : tool_name(recipe.tool), has ? "OK" : "NO", has);
        y += 7;
    }

    for(int which = 0; which < recipe.ingredient_count; ++which)
    {
        const ingredient& need = recipe.ingredients[which];
        int have = owner.carries(need.item);
        length = ui::append_int(text, 0, have);
        length = ui::append(text, length, "/");
        ui::append_int(text, length, need.amount);
        char name[24];
        int name_length = 0;

        // Item names are mixed case in the table; the screen shows capitals.
        const char* need_name = need.item == any_produce ? "Produce" : need.item == fuel_litres ? "Fuel (litres)" :
                                info(need.item).name;

        for(const char* c = need_name; *c && name_length < 20; ++c)
        {
            name[name_length++] = (*c >= 'a' && *c <= 'z') ? char(*c - 32) : *c;
        }

        name[name_length] = 0;
        need_line(pixels, y, name, text, have >= need.amount);
        y += 7;
    }

    if(recipe.water)
    {
        bool has = owner.water_source() > 0;
        need_line(pixels, y, "WATER NEARBY", has ? "OK" : "NO", has);
        y += 7;
    }

    if(! recipe.no_heat)
    {
        bool heated = recipe.fire_only ? heat == heat_kind::CAMPFIRE : heat != heat_kind::NONE;
        need_line(pixels, y, recipe.fire_only ? "OPEN FIRE" : "HEAT", heated ? "OK" : "NO", heated);
        y += 7;
    }

    if(recipe.needs != magazine::NONE)
    {
        bool known = owner.hero().skills().knows(recipe.needs);
        need_line(pixels, y, "MAGAZINE", known ? "READ" : "NO", known);
        y += 7;
    }

    pixels.rect(detail_x - 1, y + 1, detail_right - detail_x + 2, 1, colour::border);
    pixels.text(detail_x, y + 5, "MAKES", colour::quiet);

    const item_info& made = info(recipe.result);
    length = 0;

    for(const char* c = made.name; *c && length < 24; ++c)
    {
        text[length++] = (*c >= 'a' && *c <= 'z') ? char(*c - 32) : *c;
    }

    text[length] = 0;
    pixels.text(detail_x, y + 13, text, colour::text);

    if(made.category == item_category::FOOD)
    {
        length = ui::append(text, 0, "FOOD ");
        ui::append_signed(text, length, made.food);
        pixels.text(detail_x, y + 20, text, colour::quiet);
        length = ui::append(text, 0, "WATER ");
        length = ui::append_signed(text, length, made.water);
        ui::append(text, length, (made.flags & item_flag::cooked) ? " ` MOOD UP" : "");
        pixels.text(detail_x, y + 27, text, colour::quiet);
    }
    else
    {
        // What it is for: a spear to fight with, a throwable for the off hand.
        pixels.text(detail_x, y + 20, made.category == item_category::WEAPON ? "TWO-HANDED WEAPON" : "OFF HAND, THROWN",
                    colour::quiet);
        pixels.text(detail_x, y + 27, made.category == item_category::WEAPON ? "SPEAR SKILL" : "WITH R1 AND A",
                    colour::quiet);
    }
    length = ui::append(text, 0, "TAKES ");
    length = ui::append_int(text, length, owner.cook_minutes(_order[_selected]));
    ui::append(text, length, " MIN");
    pixels.text(detail_x, y + 34, text, colour::quiet);

    pixels.rect(0, footer_top, canvas::width, canvas::height - footer_top, canvas::backdrop);
    constexpr hud::hint cook_hints[] = { { "A", "COOK" }, { "B", "CLOSE" } };
    constexpr hud::hint make_hints[] = { { "A", "MAKE" }, { "B", "CLOSE" } };
    screen.hints(footer_y, recipe.no_heat ? make_hints : cook_hints, 2, false);
}

}
