#include "kx_gear_screen.h"

#include "bn_algorithm.h"

#include "kx_game.h"
#include "kx_hud_tiles.h"
#include "kx_input.h"
#include "kx_sfx.h"

namespace kx
{

namespace
{
    // Layout in pixels: three panels (worn, carried, hotbar) spanning the tab bar's width, each fitting its
    // grid of 16 px slots (2 px apart) with 4 px of padding. Titles sit in the cell row above the slots, so
    // the danger colour's palette bank never reaches an icon.
    constexpr int worn_columns = 3;
    constexpr int carried_columns = 6;
    constexpr int hotbar_columns = 1;
    constexpr int grid_rows = 4;
    constexpr int slot_step = 18;
    constexpr int padding = 4;
    constexpr int panel_gap = 4;                // playtest 2: as the other tabs (it was 6, and 4 before the hotbar)
    constexpr int panels_top = 22;
    constexpr int title_y = panels_top + 5;
    constexpr int slots_top = panels_top + 12;
    constexpr int panels_height = (slots_top - panels_top) + (grid_rows * slot_step) - 2 + padding + 1;
    constexpr int info_left = 12;
    constexpr int info_top = 116;               // ends at 141 like the other tabs' panels
    constexpr int info_width = canvas::width - (info_left * 2);
    constexpr int info_height = 26;
    constexpr int info_text_x = info_left + 6;
    constexpr int info_line_1 = info_top + 6;
    constexpr int info_line_2 = info_top + 15;
    constexpr int footer_top = 144;
    constexpr int footer_y = footer_top + 3;                      // pixel row of the button hints
    constexpr int worn_count = int(equip_slot::COUNT);

    [[nodiscard]] constexpr int panel_width(int columns)
    {
        return (columns * slot_step) + (padding * 2);
    }

    // The hotbar panel starts on an 8 px cell edge (milestone 12.5: then the screen colour showed in the gaps and
    // an icon switched its cells to the icons' palette bank, where that colour's index is orange; the backdrop is
    // see-through since playtest 1, but the edge costs nothing). The worn and hotbar panels are 2 px wider than
    // their slots need, the slots shifted 1 px to stay centred, so all the gaps are 4 px (playtest 2).
    constexpr int hotbar_gap = panel_gap;
    constexpr int hotbar_width = panel_width(hotbar_columns) + 2;
    constexpr int hotbar_slot_shift = 1;        // centres the slots in the wider panel
    constexpr int worn_width = panel_width(worn_columns) + 2;
    constexpr int worn_slot_shift = 1;
    constexpr int worn_left = info_left;
    constexpr int carried_left = worn_left + worn_width + panel_gap;
    constexpr int hotbar_left = carried_left + panel_width(carried_columns) + hotbar_gap;
    static_assert(hotbar_left + hotbar_width == info_left + info_width);
    static_assert(hotbar_left % 8 == 0);

    // The worn panel's slots, three to a row: what is held and the bag, then the clothes from the head down.
    constexpr equip_slot worn_order[] = { equip_slot::HAND, equip_slot::OFF_HAND, equip_slot::BACK,
                                          equip_slot::HEAD, equip_slot::TORSO, equip_slot::JACKET,
                                          equip_slot::LEGS, equip_slot::FEET, equip_slot::HANDS };
    constexpr const char* slot_names[] = { "HAND", "OFF HAND", "BACK", "HEAD", "TORSO", "JACKET", "LEGS", "FEET",
                                           "HANDS" };
    static_assert(sizeof(worn_order) / sizeof(equip_slot) == worn_count);
    static_assert(worn_count % worn_columns == 0);

    enum gear_action
    {
        ACTION_USE,
        ACTION_MOVE,
        ACTION_HOTBAR,
        ACTION_DROP,
        ACTION_REPAIR,
        ACTIONS_COUNT
    };

    // The actions offered for an item: worn ones cannot move between carried slots; a worn weapon with something
    // to mend it with can be repaired.
    struct action_list
    {
        gear_action actions[ACTIONS_COUNT];
        int count = 0;
    };

    [[nodiscard]] action_list actions_for(int panel, bool repairable)
    {
        action_list result;
        result.actions[result.count++] = ACTION_USE;

        if(repairable)
        {
            result.actions[result.count++] = ACTION_REPAIR;
        }

        if(panel != 0)
        {
            result.actions[result.count++] = ACTION_MOVE;
        }

        result.actions[result.count++] = ACTION_HOTBAR;
        result.actions[result.count++] = ACTION_DROP;
        return result;
    }

    // Top-left pixel of a slot in the grid of the panel starting at panel_left.
    [[nodiscard]] int slot_x(int panel_left, int columns, int index)
    {
        return panel_left + 1 + padding + ((index % columns) * slot_step);
    }

    [[nodiscard]] int slot_y(int columns, int index)
    {
        return slots_top + ((index / columns) * slot_step);
    }

    [[nodiscard]] int hotbar_index(const inventory& items, item_id id)
    {
        for(int slot = 0; slot < inventory::hotbar_slots; ++slot)
        {
            if(id != item_id::NONE && items.hotbar(slot) == id)
            {
                return slot;
            }
        }

        return -1;
    }

    // What the first action does for an item: equip, wear, eat, take (medicine), remove.
    [[nodiscard]] const char* use_label(const item_stack& stack, bool worn)
    {
        if(worn)
        {
            return "REMOVE";
        }

        const item_info& item = info(stack.id);

        if(item.slot != equip_slot::NONE)
        {
            return item.category == item_category::WEAPON ? "EQUIP" : item.slot == equip_slot::OFF_HAND ? "HOLD" :
                                                                                                          "WEAR";
        }

        if(item.flags & item_flag::edible)
        {
            return (item.flags & item_flag::drink) ? "DRINK" : "EAT";
        }

        return item.category == item_category::BOOK || is_note(stack.id) ? "READ" : "USE";
    }
}

void gear_screen::open(game& owner)
{
    _active = true;
    _dirty = true;
    _choosing = false;
    _moving = -1;

    // On a bag slot that has emptied (the last of a stack eaten or used), the nearest one that holds something
    // (playtest 6).
    const inventory& items = owner.hero().items();

    if(_panel == 1 && items.slot(_cursor[1]).empty())
    {
        for(int distance = 1; distance < inventory::slots; ++distance)
        {
            int before = _cursor[1] - distance;
            int after = _cursor[1] + distance;

            if(before >= 0 && ! items.slot(before).empty())
            {
                _cursor[1] = before;
                break;
            }

            if(after < inventory::slots && ! items.slot(after).empty())
            {
                _cursor[1] = after;
                break;
            }
        }
    }

    owner.hud_ref().begin_screen();
    sfx::play(sfx::id::ZIP, milli(500));
}

void gear_screen::close(game& owner)
{
    if(_active)
    {
        _active = false;
        _choosing = false;
        _moving = -1;
        owner.hud_ref().end_screen();
    }
}

void gear_screen::update(game& owner, const input_frame& input)
{
    player& hero = owner.hero();
    inventory& items = hero.items();
    equip_slot worn_slot = worn_order[_cursor[0]];
    const item_stack& selected = _panel == 0 ? items.equipped(worn_slot) : items.slot(_cursor[1]);

    if(_moving >= 0)
    {
        // Moving an item: the cursor picks a carried slot; A puts it there (swapping or topping up), B cancels.
        int dx = int(input.right_pressed) - int(input.left_pressed);
        int dy = int(input.down_pressed) - int(input.up_pressed);

        if(input.b_pressed)
        {
            _moving = -1;
            _dirty = true;
        }
        else if(input.a_pressed)
        {
            items.move(_moving, _cursor[1]);
            _moving = -1;
            _dirty = true;
            sfx::play(sfx::id::SWAP, milli(500));
        }
        else if(dx || dy)
        {
            _move_cursor(dx, dy, false);
            _dirty = true;
        }
    }
    else if(_choosing)
    {
        // Action list: Left/Right pick, A does it, B (or moving the cursor) cancels.
        if(input.b_pressed || input.up_pressed || input.down_pressed || selected.empty())
        {
            _choosing = false;
            _dirty = true;
        }
        else if(input.left_pressed || input.right_pressed)
        {
            int count = actions_for(_panel, hero.repair_material(selected) != item_id::NONE).count;
            _choice = (_choice + (input.right_pressed ? 1 : count - 1)) % count;
            _dirty = true;
        }
        else if(input.a_pressed)
        {
            _choosing = false;
            _dirty = true;
            item_stack chosen = selected;

            action_list offered = actions_for(_panel, hero.repair_material(selected) != item_id::NONE);

            switch(offered.actions[bn::min(_choice, offered.count - 1)])
            {

            case ACTION_REPAIR:
                if(_panel == 0)
                {
                    static_cast<void>(hero.repair(owner, items.equipped(worn_slot)));
                }
                else
                {
                    static_cast<void>(hero.repair(owner, items.slot(_cursor[1])));
                }
                break;

            case ACTION_MOVE:
                _moving = _cursor[1];
                sfx::play(sfx::id::SWAP, milli(400));
                break;

            case ACTION_USE:
                if(_panel == 0)
                {
                    hero.unequip(owner, worn_slot);
                }
                else
                {
                    hero.use_item(owner, _cursor[1]);

                    if(! _active)
                    {
                        return;     // eating or reading closed the menu
                    }
                }
                break;

            case ACTION_HOTBAR:
            {
                bool was_on_hotbar = hotbar_index(items, chosen.id) >= 0;

                if(items.toggle_hotbar(chosen.id) < 0 && ! was_on_hotbar)
                {
                    owner.toast("HOTBAR FULL", hud_tiles::bank_danger, 60);
                    sfx::play(sfx::id::DENIED, milli(600));
                }
                else
                {
                    sfx::play(sfx::id::SWAP, milli(500));
                }
                break;
            }

            default:
                // Drop at your feet (a floor pile you can search later). A flashlight goes dark as it leaves the
                // hand.
                if(chosen.id == item_id::FLASHLIGHT)
                {
                    light::set_flashlight(chosen, false, light::flashlight_charge(chosen));
                }

                if(owner.drop_item(chosen))
                {
                    if(_panel == 0)
                    {
                        items.equipped(worn_slot) = item_stack();
                    }
                    else
                    {
                        items.take(_cursor[1]);
                    }

                    sfx::play(sfx::id::DROP, milli(700));
                }
                else
                {
                    owner.toast("NO ROOM HERE", hud_tiles::bank_danger, 60);
                    sfx::play(sfx::id::DENIED, milli(600));
                }
                break;
            }
        }
    }
    else
    {
        if(input.b_pressed || input.start_pressed)
        {
            close(owner);
            return;
        }

        int dx = int(input.right_pressed) - int(input.left_pressed);
        int dy = int(input.down_pressed) - int(input.up_pressed);

        if(dx || dy)
        {
            _move_cursor(dx, dy, true);
            _dirty = true;
        }

        if(input.a_pressed && ! selected.empty())
        {
            _choosing = true;
            _choice = 0;
            _dirty = true;
        }
    }

    if(_dirty && _active)
    {
        _draw(owner);
    }
}

void gear_screen::_move_cursor(int dx, int dy, bool change_panel)
{
    int columns = _panel == 0 ? worn_columns : carried_columns;
    int count = _panel == 0 ? worn_count : inventory::slots;
    int column = (_cursor[_panel] % columns) + dx;
    int row = bn::clamp((_cursor[_panel] / columns) + dy, 0, (count / columns) - 1);

    if(column >= columns && _panel == 0 && change_panel)
    {
        _panel = 1;
        _cursor[1] = bn::min(row, (inventory::slots / carried_columns) - 1) * carried_columns;
        return;
    }

    if(column < 0 && _panel == 1 && change_panel)
    {
        _panel = 0;
        _cursor[0] = (bn::min(row, (worn_count / worn_columns) - 1) * worn_columns) + worn_columns - 1;
        return;
    }

    _cursor[_panel] = (row * columns) + bn::clamp(column, 0, columns - 1);
}

void gear_screen::_draw(game& owner)
{
    hud& screen = owner.hud_ref();
    canvas& pixels = screen.pixels();
    const inventory& items = owner.hero().items();
    char text[48];
    _dirty = false;

    screen.tab_bar(0);

    // Worn and held.
    pixels.panel(worn_left, panels_top, worn_width, panels_height, colour::fill, colour::border);
    pixels.text(worn_left + 1 + padding + worn_slot_shift, title_y, "WORN", colour::quiet);

    for(int slot = 0; slot < worn_count; ++slot)
    {
        const item_stack& stack = items.equipped(worn_order[slot]);
        int x = slot_x(worn_left, worn_columns, slot) + worn_slot_shift;
        int y = slot_y(worn_columns, slot);
        item_icon icon = stack.empty() ? (slot == 0 ? item_icon::FISTS : item_icon::EMPTY) : info(stack.id).icon;
        screen.slot(x, y, icon, _panel == 0 && _cursor[0] == slot);

        int hotbar = hotbar_index(items, stack.id);

        if(hotbar >= 0)
        {
            char digit[2] = { char('1' + hotbar), 0 };
            screen.slot_corner(x, y, digit, true);
        }
    }

    // Carried: PACK, and the weight against capacity flush right (playtest 6; danger colour when over).
    pixels.panel(carried_left, panels_top, panel_width(carried_columns), panels_height, colour::fill,
                 colour::border);
    pixels.text(carried_left + 1 + padding, title_y, "PACK", colour::quiet);
    int length = ui::append_weight(text, 0, items.weight());
    length = ui::append(text, length, " / ");       // spaced like the Health tab and the search screen
    length = ui::append_weight(text, length, items.capacity());
    ui::append(text, length, " KG");
    pixels.text_right(carried_left + panel_width(carried_columns) - 2 - padding, title_y, text,
                      items.heavy() ? colour::ui_red : colour::quiet);

    for(int slot = 0; slot < inventory::slots; ++slot)
    {
        const item_stack& stack = items.slot(slot);
        int x = slot_x(carried_left, carried_columns, slot);
        int y = slot_y(carried_columns, slot);
        screen.slot(x, y, stack.empty() ? item_icon::EMPTY : info(stack.id).icon, _panel == 1 && _cursor[1] == slot);

        // The item being moved keeps a light frame until it is put down.
        if(slot == _moving && slot != _cursor[1])
        {
            pixels.frame(x, y, 16, 16, colour::text);
        }

        if(! stack.empty())
        {
            if(stack.quantity() > 1)
            {
                screen.slot_count(x, y, stack.quantity());
            }

            int hotbar = hotbar_index(items, stack.id);

            if(hotbar >= 0)
            {
                char digit[2] = { char('1' + hotbar), 0 };
                screen.slot_corner(x, y, digit, true);
            }
        }
    }

    // Hotbar.
    pixels.panel(hotbar_left, panels_top, hotbar_width, panels_height, colour::fill, colour::border);
    pixels.text(hotbar_left + 1 + padding, title_y, "BAR", colour::quiet);      // the hotbar (one slot wide)

    for(int slot = 0; slot < inventory::hotbar_slots; ++slot)
    {
        item_id id = items.hotbar(slot);
        int x = slot_x(hotbar_left, hotbar_columns, slot) + hotbar_slot_shift;
        int y = slot_y(hotbar_columns, slot);
        screen.slot(x, y, items.available(id) ? info(id).icon : item_icon::EMPTY,
                    slot == owner.hero().selected_slot());
        char digit[2] = { char('1' + slot), 0 };
        screen.slot_corner(x, y, digit, false);
    }

    // Info box: name, then stats, or the action list while choosing, or the item being moved.
    const item_stack& selected = _panel == 0 ? items.equipped(worn_order[_cursor[0]]) : items.slot(_cursor[1]);
    pixels.panel(info_left, info_top, info_width, info_height, colour::fill, colour::accent);
    int name_x = info_text_x;

    if(_moving >= 0)
    {
        length = ui::append(text, 0, "MOVING ");
        ui::append(text, length, info(items.slot(_moving).id).name);
        pixels.text(info_text_x, info_line_1, text, colour::accent);
        const item_stack& moving = items.slot(_moving);
        const item_info& moving_info = info(moving.id);
        bool stacks = selected.id == moving.id && moving_info.stack > 1 && moving_info.durability == 0;
        pixels.text(info_text_x, info_line_2, _cursor[1] == _moving ? "PICK ANOTHER SLOT" :
                                              selected.empty() ? "PUT IT HERE" :
                                              stacks ? "STACK IT HERE" : "SWAP WITH THIS", colour::quiet);
    }
    else if(_panel == 0)
    {
        length = ui::append(text, 0, slot_names[_cursor[0]]);
        ui::append(text, length, selected.empty() ? ": EMPTY" : ":");
        name_x = pixels.text(info_text_x, info_line_1, text, colour::quiet) + 4;
    }
    else if(selected.empty())
    {
        // An empty bag slot (playtest 4: the box was blank): the bag's free slots and the weight carried of the limit
        // (in the danger colour when over it).
        int free = 0;

        for(int slot = 0; slot < inventory::slots; ++slot)
        {
            free += items.slot(slot).empty();
        }

        pixels.text(info_text_x, info_line_1, "EMPTY SLOT", colour::quiet);
        length = ui::append_int(text, 0, free);
        length = ui::append(text, length, " OF ");
        length = ui::append_int(text, length, inventory::slots);
        length = ui::append(text, length, " SLOTS FREE ` ");
        int x = pixels.text(info_text_x, info_line_2, text, colour::quiet);
        length = ui::append_weight(text, 0, items.weight());
        length = ui::append(text, length, " / ");
        length = ui::append_weight(text, length, items.capacity());
        ui::append(text, length, " KG");
        pixels.text(x, info_line_2, text, items.weight() > items.capacity() ? colour::ui_red : colour::quiet);
    }

    if(! selected.empty() && _moving < 0)
    {
        pixels.text(name_x, info_line_1, info(selected.id).name, colour::accent);

        if(_choosing)
        {
            constexpr const char* action_labels[] = { nullptr, "MOVE", "HOTBAR", "DROP", "REPAIR" };
            int x = info_text_x;
            action_list offered = actions_for(_panel, owner.hero().repair_material(selected) != item_id::NONE);

            for(int index = 0; index < offered.count; ++index)
            {
                gear_action action = offered.actions[index];
                // The chosen one on the highlight bar, as in every list (playtest 6: it was ">DRINK").
                bool chosen = index == _choice;
                const char* label = action == ACTION_USE ? use_label(selected, _panel == 0) : action_labels[action];

                if(chosen)
                {
                    pixels.rect(x - 2, info_line_2 - 1, canvas::text_width(label) + 4, 7, colour::accent);
                }

                x = pixels.text(x, info_line_2, label, chosen ? colour::fill : colour::text) + 8;
            }
        }
        else
        {
            ui::item_stats(text, selected, owner.day());
            pixels.text(info_text_x, info_line_2, text, colour::quiet);
        }
    }

    pixels.rect(0, footer_top, canvas::width, canvas::height - footer_top, canvas::backdrop);

    if(_moving >= 0)
    {
        constexpr hud::hint move_hints[] = { { "A", "PUT HERE" }, { "B", "CANCEL" }, { "D-PAD", "PICK SLOT" } };
        screen.hints(footer_y, move_hints, 3, false);
    }
    else if(_choosing)
    {
        constexpr hud::hint choose_hints[] = { { "A", "OK" }, { "B", "CANCEL" }, { "D-PAD", "CHOOSE" } };
        screen.hints(footer_y, choose_hints, 3, false);
    }
    else
    {
        // As mockup 04: the tab bar shows L1/R1; the footer's right side holds the save and quit hint.
        // Playtest 5: an empty slot has no actions, so its footer offers none.
        constexpr hud::hint gear_hints[] = { { "A", "ACTIONS" }, { "B", "CLOSE" }, { "D-PAD", "CHOOSE" } };
        screen.hints(footer_y, selected.empty() ? gear_hints + 1 : gear_hints, selected.empty() ? 2 : 3, false);
    }
}

}
