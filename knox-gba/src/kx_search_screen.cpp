#include "kx_search_screen.h"

#include "bn_bg_palettes.h"

#include "kx_base.h"
#include "kx_fixtures.h"
#include "kx_game.h"
#include "kx_hud_tiles.h"
#include "kx_input.h"
#include "kx_sfx.h"
#include "kx_timeline.h"

namespace kx
{

namespace
{
    // Layout in pixels (mockup 03): a header strip, the container and carried panels side by side, each fitting
    // its grid of 16 px slots (2 px apart) with 7 px of padding, then the info box. Titles sit in the cell row
    // above the slots, so the danger colour's palette bank never reaches an icon.
    constexpr int container_columns = 4;
    constexpr int carried_columns = 6;
    constexpr int container_rows = container::capacity / container_columns;
    constexpr int carried_rows = inventory::slots / carried_columns;
    constexpr int slot_step = 18;
    constexpr int padding = 7;
    constexpr int panel_gap = 8;
    constexpr int screen_left = 12;
    constexpr int screen_width = canvas::width - (screen_left * 2);
    constexpr int header_top = 4;
    constexpr int header_height = 14;
    constexpr int header_text_y = header_top + 5;
    constexpr int header_text_x = screen_left + 6;
    constexpr int header_right = screen_left + screen_width - 7;  // right edge of the time
    constexpr int panels_top = 21;
    constexpr int title_y = panels_top + 5;

    // A take blocked by weight blinks the carried weight red: 4 blinks of this many frames (milestone 13).
    constexpr int bag_flash_blink = 6;
    constexpr int slots_top = panels_top + 12;
    constexpr int info_top = 115;
    constexpr int info_height = 26;
    constexpr int info_text_x = screen_left + 6;
    constexpr int info_right = screen_left + screen_width - 7;
    constexpr int info_line_1 = info_top + 6;
    constexpr int info_line_2 = info_top + 15;
    constexpr int footer_top = 144;
    constexpr int footer_y = footer_top + 3;                      // pixel row of the button hints

    [[nodiscard]] constexpr int panel_width(int columns)
    {
        return (columns * slot_step) + (padding * 2);
    }

    [[nodiscard]] constexpr int panel_height(int rows)
    {
        return (slots_top - panels_top) + (rows * slot_step) - 2 + padding + 1;
    }

    constexpr int container_left = screen_left;
    constexpr int carried_left = container_left + panel_width(container_columns) + panel_gap;
    static_assert(carried_left + panel_width(carried_columns) == screen_left + screen_width);
    static_assert(panels_top + panel_height(carried_rows) < info_top);

    // Top-left pixel of a slot.
    [[nodiscard]] int slot_x(int panel, int index)
    {
        int columns = panel == 0 ? container_columns : carried_columns;
        return (panel == 0 ? container_left : carried_left) + 1 + padding + ((index % columns) * slot_step);
    }

    [[nodiscard]] int slot_y(int panel, int index)
    {
        int columns = panel == 0 ? container_columns : carried_columns;
        return slots_top + ((index / columns) * slot_step);
    }

    [[nodiscard]] bool rare(item_id id)
    {
        return info(id).rarity != item_rarity::COMMON;
    }
}

namespace
{
    // A building's sinks have the tap (kx_fixtures); while the water is on it shows as an entry to drink from.
    [[nodiscard]] bool sink(const container& value)
    {
        return value.kind == container_kind::FURNITURE && fixtures::sink(value.tile_x, value.tile_y);
    }

    void remove_tap(container& value)
    {
        for(item_stack& stack : value.items)
        {
            if(stack.id == item_id::TAP_WATER)
            {
                stack = item_stack();
            }
        }
    }
}

void search_screen::open(game& owner, int container_index, int extra_items)
{
    container_store& containers = owner.containers();
    bool first_time = ! containers.get(container_index).opened;
    containers.open(container_index, owner.day(), extra_items);

    container& value = containers.get(container_index);
    remove_tap(value);

    if(sink(value))
    {
        if(timeline::water_on(owner.day()))
        {
            item_stack tap = make_stack(item_id::TAP_WATER);
            static_cast<void>(value.add(tap));
        }
        else
        {
            owner.toast("THE TAP IS DRY", hud_tiles::bank_quiet, 90);
        }
    }

    _container = container_index;
    _revealed = first_time ? 0 : container::capacity;
    _timer = tuning(inventory_param::SEARCH_START).integer();
    _frames = 0;
    _shown_seconds = -1;
    _panel = 0;
    _cursor[0] = 0;
    _cursor[1] = 0;
    _active = true;
    _dirty = true;
    _opening = 2;
    owner.hud_ref().begin_screen();
    sfx::play(sfx::id::RUMMAGE, milli(700));
}

void search_screen::close(game& owner)
{
    if(! _active)
    {
        return;
    }

    _active = false;
    remove_tap(owner.containers().get(_container));

    // Foraging: what was not taken stays on the ground, unseen.
    if(owner.containers().record(_container).kind == container_kind::FORAGE)
    {
        owner.containers().remove(_container);
        bn::bg_palettes::set_brightness(0);
        owner.hud_ref().end_screen();
        return;
    }

    bn::bg_palettes::set_brightness(0);
    owner.remove_empty_pile(_container);
    owner.hud_ref().end_screen();
}

void search_screen::update(game& owner, const input_frame& input)
{
    if(_flash > 0 && --_flash == 0)
    {
        bn::bg_palettes::set_brightness(0);
    }

    ++_frames;
    _advance_reveal(owner);

    // A full bag blinks the carried weight red (milestone 13).
    if(_bag_flash > 0)
    {
        --_bag_flash;

        if(! _opening && ! _dirty && _bag_flash % bag_flash_blink == 0)
        {
            _draw_weight(owner);
        }
    }

    if(input.b_pressed || input.start_pressed)
    {
        close(owner);
        return;
    }

    int dx = int(input.right_pressed) - int(input.left_pressed);
    int dy = int(input.down_pressed) - int(input.up_pressed);

    if(dx || dy)
    {
        _move_cursor(dx, dy);
        _dirty = true;
    }

    if(_panel == 0)
    {
        if(input.a_pressed)
        {
            _take(owner, _cursor[0], false);
        }
        else if(input.select_pressed)
        {
            _take(owner, _cursor[0], true);
        }
        else if(input.r_pressed)
        {
            for(int slot = 0; slot < bn::min(_revealed, int(container::capacity)); ++slot)
            {
                const item_stack& stack = owner.containers().get(_container).items[slot];

                if(! stack.empty() && stack.id != item_id::TAP_WATER)
                {
                    _take(owner, slot, false);
                }
            }
        }
    }
    else if(input.a_pressed)
    {
        _store(owner, _cursor[1]);
    }

    if(_opening)
    {
        // The first draw is split over two frames (the container side, then the carried side), so opening a
        // search never overruns a frame.
        if(--_opening)
        {
            _draw_left(owner);
            return;
        }

        _draw_right(owner);
    }
    else if(_dirty)
    {
        _draw(owner);
    }
    else
    {
        if(_revealed != _revealed_shown)
        {
            _draw_reveals(owner);
        }

        if(_frames / 60 != _shown_seconds)
        {
            _draw_time(owner);
        }
    }
}

void search_screen::_advance_reveal(game& owner)
{
    if(_revealed >= container::capacity)
    {
        return;
    }

    if(--_timer > 0)
    {
        return;
    }

    const container& value = owner.containers().get(_container);

    // Empty slots are skipped; each item pops in after its own delay.
    while(_revealed < container::capacity && value.items[_revealed].empty())
    {
        ++_revealed;
    }

    if(_revealed < container::capacity)
    {
        item_id id = value.items[_revealed].id;
        ++_revealed;

        if(rare(id))
        {
            sfx::play(sfx::id::CHIME, milli(800));
            bn::bg_palettes::set_brightness(milli(350));
            _flash = 1;
        }
        else
        {
            sfx::play(sfx::id::POP, milli(600));
        }
    }

    while(_revealed < container::capacity && value.items[_revealed].empty())
    {
        ++_revealed;
    }

    _timer = owner.hero().body().action_time(tuning(inventory_param::SEARCH_PER_ITEM).integer());
}

void search_screen::_take(game& owner, int slot, bool half)
{
    item_stack& stack = owner.containers().get(_container).items[slot];

    if(slot >= _revealed || stack.empty())
    {
        return;
    }

    // The tap: a drink, nothing to carry.
    if(stack.id == item_id::TAP_WATER)
    {
        if(! half)
        {
            owner.hero().body().drink(timeline_tuning::tap_water, 0, 0, owner.random());
            sfx::play(sfx::id::GULP, milli(600));
            _dirty = true;
        }

        return;
    }

    item_stack moving = stack;
    const item_stack taken_stack = stack;

    if(half && info(stack.id).durability == 0 && stack.amount > 1)
    {
        moving.amount = uint8_t((stack.amount + 1) / 2);
    }

    int before = moving.amount;
    bool fitted = owner.hero().items().add(moving);
    int taken = fitted ? before : before - moving.amount;

    if(taken <= 0 && ! fitted)
    {
        owner.toast("BAG FULL", hud_tiles::bank_danger, 60);
        sfx::play(sfx::id::DENIED, milli(600));
        _bag_flash = bag_flash_blink * 4;
        return;
    }

    if(info(stack.id).durability > 0 || taken >= stack.amount)
    {
        stack = item_stack();
    }
    else
    {
        stack.amount = uint8_t(stack.amount - taken);
    }

    sfx::play(sfx::id::PICKUP, milli(700));
    owner.item_taken(taken_stack);
    _dirty = true;
}

void search_screen::_store(game& owner, int slot)
{
    inventory& items = owner.hero().items();

    if(items.slot(slot).empty())
    {
        return;
    }

    item_stack moving = items.slot(slot);
    int before = moving.quantity();

    // The save's stack budget (milestone 14): a world this full takes nothing more.
    if(owner.containers().room() <= 0)
    {
        owner.toast("WORLD STORAGE FULL", hud_tiles::bank_danger, 90);
        sfx::play(sfx::id::DENIED, milli(600));
        return;
    }

    container& target = owner.containers().get(_container);

    // A built crate holds 40 kg (spec).
    if(target.kind == container_kind::CRATE)
    {
        int held = 0;

        for(const item_stack& stack : target.items)
        {
            held += stack.empty() ? 0 : stack.weight();
        }

        if(held + moving.weight() > base_tuning::crate_capacity)
        {
            owner.toast("CRATE FULL (40 KG)", hud_tiles::bank_danger, 60);
            sfx::play(sfx::id::DENIED, milli(600));
            return;
        }
    }

    // A car's trunk holds 60 kg (spec).
    if(target.kind == container_kind::VEHICLE && target.table == loot_table::CAR_TRUNK)
    {
        int held = 0;

        for(const item_stack& stack : target.items)
        {
            held += stack.empty() ? 0 : stack.weight();
        }

        if(held + moving.weight() > vehicle_tuning::trunk_capacity)
        {
            owner.toast("TRUNK FULL (60 KG)", hud_tiles::bank_danger, 60);
            sfx::play(sfx::id::DENIED, milli(600));
            return;
        }
    }

    target.add(moving);
    int stored = moving.empty() ? before : before - moving.amount;

    if(stored <= 0)
    {
        owner.toast("NO ROOM", hud_tiles::bank_danger, 60);
        sfx::play(sfx::id::DENIED, milli(600));
        return;
    }

    items.take(slot, stored);
    sfx::play(sfx::id::DROP, milli(600));
    _dirty = true;
}

void search_screen::_move_cursor(int dx, int dy)
{
    int columns = _panel == 0 ? container_columns : carried_columns;
    int count = _panel == 0 ? container::capacity : inventory::slots;
    int column = _cursor[_panel] % columns;
    int row = _cursor[_panel] / columns;
    int rows = count / columns;

    column += dx;
    row = bn::clamp(row + dy, 0, rows - 1);

    // Crossing a panel edge moves to the other grid, keeping the row where possible.
    if(column >= columns && _panel == 0)
    {
        _panel = 1;
        _cursor[1] = bn::min(row, (inventory::slots / carried_columns) - 1) * carried_columns;
        return;
    }

    if(column < 0 && _panel == 1)
    {
        _panel = 0;
        _cursor[0] = (bn::min(row, (container::capacity / container_columns) - 1) * container_columns) +
                     container_columns - 1;
        return;
    }

    column = bn::clamp(column, 0, columns - 1);
    _cursor[_panel] = (row * columns) + column;
}

void search_screen::_draw_time(game& owner)
{
    // Time spent searching, top-right; redrawn on its own every second.
    canvas& pixels = owner.hud_ref().pixels();
    char text[16];
    _shown_seconds = _frames / 60;
    int length = ui::append(text, 0, "TIME ");
    text[length++] = char('0' + ((_shown_seconds / 600) % 6));
    text[length++] = char('0' + ((_shown_seconds / 60) % 10));
    text[length++] = ':';
    text[length++] = char('0' + ((_shown_seconds % 60) / 10));
    text[length++] = char('0' + (_shown_seconds % 10));
    text[length] = 0;

    int widest = canvas::text_width("TIME 00:00");
    pixels.rect(header_right - widest, header_text_y, widest + 1, small_font::height, colour::fill);
    pixels.text_right(header_right, header_text_y, text, colour::quiet);
}

void search_screen::_draw_header(game& owner)
{
    // What is being searched, and for how long.
    canvas& pixels = owner.hud_ref().pixels();
    const loot_table_info& table = info(owner.containers().get(_container).table);
    pixels.panel(screen_left, header_top, screen_width, header_height, colour::fill, colour::border);
    int x = header_text_x;

    if(_revealed < container::capacity)
    {
        x = pixels.text(x, header_text_y, "SEARCHING ` ", colour::text);
    }

    const container& value = owner.containers().get(_container);
    const char* fixture = value.kind == container_kind::FURNITURE ?
                          fixtures::name(fixtures::at(value.tile_x, value.tile_y)) : nullptr;
    const char* name = fixture ? fixture : table.container_name;
    pixels.text(x, header_text_y, name, colour::accent);
    _draw_time(owner);
}

void search_screen::_draw_container_slot(game& owner, int slot)
{
    const item_stack& stack = owner.containers().get(_container).items[slot];
    bool visible = slot < _revealed && ! stack.empty();
    int x = slot_x(0, slot);
    int y = slot_y(0, slot);
    owner.hud_ref().slot(x, y, visible ? info(stack.id).icon : item_icon::EMPTY, _panel == 0 && _cursor[0] == slot);

    if(visible && stack.quantity() > 1)
    {
        owner.hud_ref().slot_count(x, y, stack.quantity());
    }
}

void search_screen::_draw_reveals(game& owner)
{
    // Items popping in redraw only their slots, the info box and (when the search ends) the header.
    for(int slot = _revealed_shown; slot < _revealed; ++slot)
    {
        _draw_container_slot(owner, slot);
    }

    _revealed_shown = _revealed;
    _draw_info(owner);

    if(_revealed >= container::capacity)
    {
        _draw_header(owner);
    }
}

void search_screen::_draw_info(game& owner)
{
    // Info box for the selected item.
    canvas& pixels = owner.hud_ref().pixels();
    const container& value = owner.containers().get(_container);
    const inventory& items = owner.hero().items();
    char text[48];
    const item_stack& selected = _panel == 0 ? (_cursor[0] < _revealed ? value.items[_cursor[0]] : item_stack()) :
                                               items.slot(_cursor[1]);
    pixels.panel(screen_left, info_top, screen_width, info_height, colour::fill, colour::accent);

    if(! selected.empty())
    {
        pixels.text(info_text_x, info_line_1, info(selected.id).name, colour::accent);
        ui::item_stats(text, selected, owner.day());
        pixels.text(info_text_x, info_line_2, text, colour::quiet);
        pixels.text_right(info_right, info_line_1, _panel == 1 ? "[A] STORE" :
                          selected.id == item_id::TAP_WATER ? "[A] DRINK" : "[A] TAKE", colour::text);
    }
    else if(_panel == 0 && _revealed < container::capacity)
    {
        pixels.text(info_text_x, info_line_1, "SEARCHING...", colour::quiet);
    }
}

void search_screen::_draw(game& owner)
{
    _draw_left(owner);
    _draw_right(owner);
}

void search_screen::_draw_left(game& owner)
{
    hud& screen = owner.hud_ref();
    canvas& pixels = screen.pixels();
    const container& value = owner.containers().get(_container);
    const loot_table_info& table = info(value.table);
    _dirty = false;
    _draw_header(owner);

    // Container panel.
    pixels.panel(container_left, panels_top, panel_width(container_columns), panel_height(container_rows),
                 colour::fill, colour::border);
    pixels.text(container_left + 1 + padding, title_y, table.short_name, colour::quiet);

    for(int slot = 0; slot < container::capacity; ++slot)
    {
        _draw_container_slot(owner, slot);
    }

    _revealed_shown = _revealed;
}

void search_screen::_draw_weight(game& owner)
{
    canvas& pixels = owner.hud_ref().pixels();
    const inventory& items = owner.hero().items();
    char text[24];
    int length = ui::append_weight(text, 0, items.weight());
    length = ui::append(text, length, " / ");
    length = ui::append_weight(text, length, items.capacity());
    ui::append(text, length, " KG");

    // The weight against capacity: danger colour when over, blinking red while a take is blocked by weight.
    int right = carried_left + panel_width(carried_columns) - 2 - padding;
    int width = canvas::text_width(text);
    bool blink = _bag_flash > 0 && (_bag_flash / bag_flash_blink) % 2 == 1;
    pixels.rect(right - width - 6, title_y, width + 7, small_font::height, colour::fill);
    pixels.text_right(right, title_y, text, blink || items.heavy() ? colour::ui_red : colour::text);
}

void search_screen::_draw_right(game& owner)
{
    hud& screen = owner.hud_ref();
    canvas& pixels = screen.pixels();
    const inventory& items = owner.hero().items();

    // Carried panel with the weight against capacity (danger colour when over).
    int carried_width = panel_width(carried_columns);
    pixels.panel(carried_left, panels_top, carried_width, panel_height(carried_rows), colour::fill, colour::border);
    pixels.text(carried_left + 1 + padding, title_y, "CARRIED", colour::quiet);
    _draw_weight(owner);

    for(int slot = 0; slot < inventory::slots; ++slot)
    {
        const item_stack& stack = items.slot(slot);
        int x = slot_x(1, slot);
        int y = slot_y(1, slot);
        screen.slot(x, y, stack.empty() ? item_icon::EMPTY : info(stack.id).icon, _panel == 1 && _cursor[1] == slot);

        if(! stack.empty() && stack.quantity() > 1)
        {
            screen.slot_count(x, y, stack.quantity());
        }
    }

    _draw_info(owner);

    pixels.rect(0, footer_top, canvas::width, canvas::height - footer_top, canvas::backdrop);

    constexpr hud::hint take_hints[] = { { "A", "TAKE" }, { ui::button::r, "TAKE ALL" }, { "SELECT", "SPLIT" },
                                                 { "B", "CLOSE" } };
    constexpr hud::hint store_hints[] = { { "A", "STORE" }, { "B", "CLOSE" } };

    if(_panel == 0)
    {
        screen.hints(footer_y, take_hints, 4, false);
    }
    else
    {
        screen.hints(footer_y, store_hints, 2, false);
    }
}

}
