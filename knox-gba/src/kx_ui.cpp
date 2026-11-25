#include "kx_ui.h"

#include "bn_algorithm.h"

#include "kx_guns.h"
#include "kx_light.h"
#include "kx_survival.h"
#include "kx_world.h"
#include "kx_world_map.h"

namespace kx::ui
{

int append(char* output, int length, const char* text)
{
    while(*text)
    {
        output[length++] = *text++;
    }

    output[length] = 0;
    return length;
}

int append_int(char* output, int length, int value)
{
    if(value < 0)
    {
        output[length++] = '-';
        value = -value;
    }

    char digits[12];
    int count = 0;

    do
    {
        digits[count++] = char('0' + (value % 10));
        value /= 10;
    }
    while(value > 0 && count < 11);

    while(count > 0)
    {
        output[length++] = digits[--count];
    }

    output[length] = 0;
    return length;
}

int append_signed(char* output, int length, int value)
{
    if(value >= 0)
    {
        output[length++] = '+';
    }

    return append_int(output, length, value);
}

int append_weight(char* output, int length, int weight)
{
    int tenths = (weight + 5) / 10;
    length = append_int(output, length, tenths / 10);
    output[length++] = '.';
    output[length++] = char('0' + (tenths % 10));
    output[length] = 0;
    return length;
}

void item_stats(char* output, const item_stack& stack, int today)
{
    int length = 0;
    output[0] = 0;

    if(stack.empty())
    {
        return;
    }

    const item_info& item = info(stack.id);

    switch(item.category)
    {

    case item_category::FOOD:
        if(const char* freshness = freshness_name(stack, today))
        {
            length = append(output, length, freshness);
            length = append(output, length, " ` ");
        }

        if(item.food)
        {
            length = append(output, length, "FOOD ");
            length = append_signed(output, length, item.food);
            length = append(output, length, " ` ");
        }

        if(item.water)
        {
            length = append(output, length, "WATER ");
            length = append_signed(output, length, item.water);
            length = append(output, length, " ` ");
        }
        break;

    case item_category::WEAPON:
        if(is_gun(stack.id))
        {
            length = append(output, length, "LOADED ");
            length = append_int(output, length, gun_loaded(stack));
            length = append(output, length, "/");
            length = append_int(output, length, info(gun_of(stack.id)).capacity);
            length = append(output, length, " ` ");
            break;
        }

        length = append(output, length, "COND ");
        length = append_int(output, length, stack.amount);
        length = append(output, length, "/");
        length = append_int(output, length, item.durability);
        length = append(output, length, " ` ");
        break;

    case item_category::CLOTHING:
        length = append(output, length, "SCR ");
        length = append_int(output, length, item.scratch_defense);
        length = append(output, length, "% BITE ");
        length = append_int(output, length, item.bite_defense);
        length = append(output, length, "% ` ");
        break;

    case item_category::BOOK:
        if(stack.id >= item_id::MAGAZINE_CANNING && stack.id <= item_id::MAGAZINE_GENERATOR)
        {
            length = append(output, length, "MAGAZINE ` ");
        }
        else
        {
            length = append(output, length, "VOL ");
            length = append_int(output, length, bn::max(int(stack.extra), 1));
            length = append(output, length, " ` ");
        }
        break;

    case item_category::TOOL:
        if(item.durability > 0)
        {
            length = append(output, length, "USES ");
            length = append_int(output, length, stack.amount);
            length = append(output, length, "/");
            length = append_int(output, length, item.durability);
            length = append(output, length, " ` ");
        }
        break;

    case item_category::BAG:
        length = append(output, length, "CAPACITY +");
        length = append_int(output, length, item.capacity);
        length = append(output, length, " KG ` ");
        break;

    default:
        // A flashlight: its battery, and whether it is on.
        if(stack.id == item_id::FLASHLIGHT)
        {
            int full = bn::max(tuning(light_param::BATTERY_MINUTES), 1);
            length = append(output, length, light::flashlight_on(stack) ? "ON ` BATTERY " : "BATTERY ");
            length = append_int(output, length, bn::min((light::flashlight_charge(stack) * 100) / full, 100));
            length = append(output, length, "% ` ");
        }
        else if(stack.id == item_id::HOUSE_KEY)
        {
            // Milestone 12.7: which town's house it opens.
            int settlement = world::house_settlement(stack.extra);
            length = append(output, length, settlement >= 0 ? "KEY: HOUSE IN " : "KEY: NO DOOR KNOWN");

            if(settlement >= 0)
            {
                length = append(output, length, world_map::settlement_name(settlement));
            }

            length = append(output, length, " ` ");
        }
        else if(is_throwable(stack.id))
        {
            // Held in the off hand, thrown with R1 and A.
            length = append(output, length, "THROWN ` ");

            if(stack.quantity() > 1)
            {
                length = append(output, length, "X");
                length = append_int(output, length, stack.quantity());
                length = append(output, length, " ` ");
            }
        }
        else if(stack.quantity() > 1)
        {
            length = append(output, length, "X");
            length = append_int(output, length, stack.quantity());
            length = append(output, length, " ` ");
        }
        break;
    }

    length = append_weight(output, length, stack.weight());
    append(output, length, " KG");
}

}
