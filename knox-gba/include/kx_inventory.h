#ifndef KX_INVENTORY_H
#define KX_INVENTORY_H

#include "bn_array.h"

#include "kx_items.h"

namespace kx
{

// The survivor's carried items, equipped slots and hotbar (spec: Inventory). Weight-based: carry capacity is
// the base capacity plus the equipped bag; going over gives Heavy load. Worn clothing and the bag on the back
// do not count towards the carried weight; the item in hand does.
class inventory
{

public:
    static constexpr int slots = 24;            // 6x4 grid (mockup 03)
    static constexpr int hotbar_slots = 4;

    [[nodiscard]] const item_stack& slot(int index) const
    {
        return _slots[index];
    }

    // A carried slot to change in place (a propane tank used up a little).
    [[nodiscard]] item_stack& slot(int index)
    {
        return _slots[index];
    }

    [[nodiscard]] const item_stack& equipped(equip_slot value) const
    {
        return _equipped[int(value)];
    }

    [[nodiscard]] item_stack& equipped(equip_slot value)
    {
        return _equipped[int(value)];
    }

    // Weights in 10 g units.
    [[nodiscard]] int weight() const;
    [[nodiscard]] int capacity() const;

    [[nodiscard]] bool heavy() const
    {
        return weight() > capacity();
    }

    [[nodiscard]] bool overloaded() const
    {
        return weight() * 2 > capacity() * 3;
    }

    [[nodiscard]] int used_slots() const;

    // Merges into matching stacks, then fills empty slots. Whatever does not fit stays in `stack`; returns true
    // if everything fitted.
    bool add(item_stack& stack);

    // Removes up to `amount` units (the whole stack for items with condition) from a slot.
    item_stack take(int index, int amount = 255);

    // Moves a slot's stack onto another slot: it tops up a matching stack (what does not fit stays behind), or
    // the two slots swap.
    void move(int from, int to);

    // Equips the item in a slot (weapons to the hand, a flashlight to the off hand, clothing and bags to their
    // slot); whatever was equipped there takes the item's place. A two-handed weapon and an off-hand item push
    // each other back into the bag. Returns false if the item cannot be equipped (or there is no room for that).
    bool equip_from(int index);

    // Moves an equipped item into the carried slots; false if there is no room.
    bool unequip(equip_slot value);

    // Removes one unit of a carried item (eating, using a bandage).
    void consume_one(int index);

    [[nodiscard]] int find(item_id id) const;

    // Units of an item carried (not counting the equipped ones), and taking them out (building materials).
    [[nodiscard]] int count(item_id id) const;

    // Matches, or a lighter with uses left (milestone 13): lights campfires and cigarettes.
    [[nodiscard]] bool fire_starter() const;

    // Lighting something: matches are kept (as before); without them a lighter loses a use.
    void use_fire_starter();
    void remove(item_id id, int amount);

    // ----- hotbar (references carried or equipped item types) -----

    [[nodiscard]] item_id hotbar(int index) const
    {
        return _hotbar[index];
    }

    [[nodiscard]] bool available(item_id id) const;

    // Adds the type to the first free hotbar slot, or removes it if it is already there. Returns the slot used,
    // or -1 when removed or when the hotbar is full.
    int toggle_hotbar(item_id id);

    void set_hotbar(int index, item_id id)
    {
        _hotbar[index] = id;
    }

    void clear();

    // Strength's share of the carry capacity (10 g units), kept up to date by the player.
    void set_capacity_bonus(int bonus)
    {
        _capacity_bonus = int16_t(bonus);
    }

private:
    bn::array<item_stack, slots> _slots;
    bn::array<item_stack, int(equip_slot::COUNT)> _equipped;
    bn::array<item_id, hotbar_slots> _hotbar = {};
    int16_t _capacity_bonus = 0;
};

[[nodiscard]] item_stack make_stack(item_id id, int amount = 1);

}

#endif
