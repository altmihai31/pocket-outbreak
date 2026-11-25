#include "kx_inventory.h"

#include "bn_algorithm.h"

#include "kx_guns.h"
#include "kx_light.h"
#include "kx_loot.h"

namespace kx
{

item_stack make_stack(item_id id, int amount)
{
    const item_info& item = info(id);
    item_stack result;
    result.id = id;
    result.amount = uint8_t(item.durability > 0 ? item.durability : bn::clamp(amount, 1, int(item.stack)));

    // A new gun comes loaded (found ones are rolled in kx_containers).
    if(is_gun(id))
    {
        set_gun_loaded(result, info(gun_of(id)).capacity);
    }

    // A new flashlight comes with a full battery, switched off (found ones are rolled lower in kx_containers).
    if(id == item_id::FLASHLIGHT)
    {
        light::set_flashlight(result, false, tuning(light_param::BATTERY_MINUTES));
    }

    return result;
}

namespace
{
    // An item going from a hand into the bag: a flashlight is switched off (it only shines while held).
    [[nodiscard]] item_stack stowed(item_stack stack)
    {
        if(stack.id == item_id::FLASHLIGHT)
        {
            light::set_flashlight(stack, false, light::flashlight_charge(stack));
        }

        return stack;
    }
}

int inventory::weight() const
{
    // What is held in either hand is carried; worn clothing and the bag are not.
    int result = 0;

    for(equip_slot held : { equip_slot::HAND, equip_slot::OFF_HAND })
    {
        const item_stack& stack = _equipped[int(held)];
        result += stack.empty() ? 0 : stack.weight();
    }

    for(const item_stack& stack : _slots)
    {
        if(! stack.empty())
        {
            result += stack.weight();
        }
    }

    return result;
}

int inventory::capacity() const
{
    int kilograms = tuning(inventory_param::BASE_CAPACITY).integer();
    const item_stack& bag = _equipped[int(equip_slot::BACK)];

    if(! bag.empty())
    {
        kilograms += info(bag.id).capacity;
    }

    return bn::max((kilograms * 100) + _capacity_bonus, 100);
}

int inventory::used_slots() const
{
    int result = 0;

    for(const item_stack& stack : _slots)
    {
        result += ! stack.empty();
    }

    return result;
}

bool inventory::fire_starter() const
{
    if(count(item_id::MATCHES) > 0)
    {
        return true;
    }

    for(const item_stack& stack : _slots)
    {
        if(stack.id == item_id::LIGHTER && stack.amount > 0)
        {
            return true;
        }
    }

    return false;
}

void inventory::use_fire_starter()
{
    if(count(item_id::MATCHES) > 0)
    {
        return;
    }

    for(item_stack& stack : _slots)
    {
        if(stack.id == item_id::LIGHTER && stack.amount > 0)
        {
            // An empty lighter is thrown away.
            if(--stack.amount == 0)
            {
                stack = item_stack();
            }

            return;
        }
    }
}

bool inventory::add(item_stack& stack)
{
    if(stack.empty())
    {
        return true;
    }

    const item_info& item = info(stack.id);

    // Merge into existing stacks first.
    if(item.stack > 1 && item.durability == 0)
    {
        for(item_stack& existing : _slots)
        {
            if(existing.id == stack.id && existing.amount < item.stack)
            {
                int moved = bn::min(item.stack - existing.amount, int(stack.amount));
                existing.amount = uint8_t(existing.amount + moved);
                stack.amount = uint8_t(stack.amount - moved);

                if(stack.amount == 0)
                {
                    stack = item_stack();
                    return true;
                }
            }
        }
    }

    for(item_stack& existing : _slots)
    {
        if(existing.empty())
        {
            existing = stack;
            stack = item_stack();
            return true;
        }
    }

    return false;
}

item_stack inventory::take(int index, int amount)
{
    item_stack& stack = _slots[index];
    item_stack result = stack;

    if(stack.empty())
    {
        return result;
    }

    if(info(stack.id).durability > 0 || amount >= stack.amount)
    {
        stack = item_stack();
        return result;
    }

    result.amount = uint8_t(amount);
    stack.amount = uint8_t(stack.amount - amount);
    return result;
}

void inventory::move(int from, int to)
{
    if(from == to)
    {
        return;
    }

    item_stack& source = _slots[from];
    item_stack& target = _slots[to];
    const item_info& item = info(source.id);

    if(! source.empty() && target.id == source.id && item.stack > 1 && item.durability == 0 &&
       target.amount < item.stack)
    {
        int moved = bn::min(int(item.stack) - int(target.amount), int(source.amount));
        target.amount = uint8_t(target.amount + moved);
        source.amount = uint8_t(source.amount - moved);

        if(source.amount == 0)
        {
            source = item_stack();
        }

        return;
    }

    item_stack swapped = target;
    target = source;
    source = swapped;
}

bool inventory::equip_from(int index)
{
    item_stack& stack = _slots[index];

    if(stack.empty())
    {
        return false;
    }

    equip_slot target = info(stack.id).slot;

    if(target == equip_slot::NONE)
    {
        return false;
    }

    // Both hands: a two-handed weapon puts the off hand's item back in the bag, and taking something in the off
    // hand puts a two-handed weapon away. Either needs room for it.
    if(target == equip_slot::HAND && two_handed(stack.id) && ! unequip(equip_slot::OFF_HAND))
    {
        return false;
    }

    if(target == equip_slot::OFF_HAND && two_handed(_equipped[int(equip_slot::HAND)].id) &&
            ! unequip(equip_slot::HAND))
    {
        return false;
    }

    item_stack previous = stowed(_equipped[int(target)]);

    if(info(stack.id).stack > 1 && stack.amount > 1)
    {
        // Equip one unit of a stack; the previous item needs a free slot of its own.
        if(! previous.empty() && used_slots() >= slots)
        {
            return false;
        }

        _equipped[int(target)] = take(index, 1);

        if(! previous.empty())
        {
            add(previous);
        }

        return true;
    }

    _equipped[int(target)] = stack;
    stack = previous;
    return true;
}

bool inventory::unequip(equip_slot value)
{
    item_stack& stack = _equipped[int(value)];

    if(stack.empty())
    {
        return true;
    }

    item_stack moving = stowed(stack);

    if(! add(moving))
    {
        return false;
    }

    stack = item_stack();
    return true;
}

void inventory::consume_one(int index)
{
    item_stack& stack = _slots[index];

    if(! stack.empty())
    {
        if(info(stack.id).durability > 0 || stack.amount <= 1)
        {
            stack = item_stack();
        }
        else
        {
            --stack.amount;
        }
    }
}

int inventory::find(item_id id) const
{
    for(int index = 0; index < slots; ++index)
    {
        if(_slots[index].id == id)
        {
            return index;
        }
    }

    return -1;
}

bool inventory::available(item_id id) const
{
    if(id == item_id::NONE)
    {
        return false;
    }

    for(const item_stack& stack : _equipped)
    {
        if(stack.id == id)
        {
            return true;
        }
    }

    return find(id) >= 0;
}

int inventory::toggle_hotbar(item_id id)
{
    for(int index = 0; index < hotbar_slots; ++index)
    {
        if(_hotbar[index] == id)
        {
            _hotbar[index] = item_id::NONE;
            return -1;
        }
    }

    for(int index = 0; index < hotbar_slots; ++index)
    {
        if(_hotbar[index] == item_id::NONE)
        {
            _hotbar[index] = id;
            return index;
        }
    }

    return -1;
}

void inventory::clear()
{
    _slots.fill(item_stack());
    _equipped.fill(item_stack());
    _hotbar.fill(item_id::NONE);
}

int inventory::count(item_id id) const
{
    int result = 0;

    for(const item_stack& stack : _slots)
    {
        if(stack.id == id)
        {
            result += stack.quantity();
        }
    }

    return result;
}

void inventory::remove(item_id id, int amount)
{
    for(int index = 0; index < slots && amount > 0; ++index)
    {
        if(_slots[index].id == id)
        {
            int taken = bn::min(amount, _slots[index].quantity());
            static_cast<void>(take(index, taken));
            amount -= taken;
        }
    }
}

}
