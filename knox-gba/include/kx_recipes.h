#ifndef KX_RECIPES_H
#define KX_RECIPES_H

#include "kx_items.h"

namespace kx
{

// Cooking (spec: about 15 recipes combining a base (water, pot) with ingredients; a heat source: a campfire, a
// stove while the power is on, or a gas stove with a propane tank). Cooked food restores more and lifts morale.
// Since milestone 12.6 the Craft tab also makes things by hand that need no heat.

// What cooks it. NONE: no heat nearby.
enum class heat_kind : uint8_t
{
    NONE,
    CAMPFIRE,
    STOVE,
    GAS_STOVE
};

// Magazines teach single recipes (spec): canning now, trapping and generator use with their systems.
enum class magazine : uint8_t
{
    NONE,
    CANNING,
    TRAPPING,
    GENERATOR
};

// "Any produce": berries, mushrooms, herbs, apples and crops.
constexpr item_id any_produce = item_id::COUNT;

// Litres of fuel, poured from the fuel cans carried (a Molotov).
constexpr item_id fuel_litres = item_id(int(item_id::COUNT) + 1);

[[nodiscard]] constexpr bool produce(item_id id)
{
    return id == item_id::BERRIES || id == item_id::MUSHROOMS || id == item_id::HERBS || id == item_id::APPLE ||
           id == item_id::CARROT || id == item_id::POTATO || id == item_id::CABBAGE || id == item_id::ONION ||
           id == item_id::TOMATO;
}

struct ingredient
{
    item_id item;
    uint8_t amount;
};

struct recipe_info
{
    const char* name;
    item_id result;
    ingredient ingredients[3];
    uint8_t ingredient_count;
    item_id tool;               // COOKING_POT, FRYING_PAN, or NONE
    bool water;                 // a water source nearby: a rain collector, a river or pond, a working tap
    bool fire_only;             // over an open fire (a campfire)
    uint8_t minutes;
    magazine needs;
    uint8_t xp;                 // Cooking XP
    bool no_heat = false;       // made by hand (milestone 12.6: the spear, the Molotov); no heat, no Cooking XP
    uint8_t result_count = 1;   // how many of the result it makes (milestone 13: rope from a hide)
};

constexpr recipe_info recipes[] = {
    { "BOIL WATER", item_id::WATER_BOTTLE, {}, 0, item_id::COOKING_POT, true, false, 20, magazine::NONE, 4 },
    { "HERBAL TEA", item_id::HERBAL_TEA, { { item_id::HERBS, 1 } }, 1, item_id::COOKING_POT, true, false, 15,
      magazine::NONE, 6 },
    { "VEGETABLE SOUP", item_id::SOUP, { { any_produce, 2 }, { item_id::WATER_BOTTLE, 1 } }, 2, item_id::COOKING_POT,
      false, false, 30, magazine::NONE, 10 },
    { "ROAST MEAT", item_id::ROAST_MEAT, { { item_id::RAW_MEAT, 1 } }, 1, item_id::NONE, false, true, 30,
      magazine::NONE, 8 },
    { "MUSHROOM FRY", item_id::MUSHROOM_FRY, { { item_id::MUSHROOMS, 2 } }, 1, item_id::FRYING_PAN, false, false, 15,
      magazine::NONE, 8 },
    { "FRIED FISH", item_id::FRIED_FISH, { { item_id::FISH, 1 } }, 1, item_id::FRYING_PAN, false, false, 20,
      magazine::NONE, 8 },
    { "FISH SOUP", item_id::FISH_SOUP, { { item_id::FISH, 1 }, { item_id::HERBS, 1 }, { item_id::WATER_BOTTLE, 1 } }, 3,
      item_id::COOKING_POT, false, false, 30, magazine::NONE, 12 },
    { "STEW", item_id::STEW, { { item_id::RAW_MEAT, 1 }, { any_produce, 1 }, { item_id::WATER_BOTTLE, 1 } }, 3,
      item_id::COOKING_POT, false, false, 45, magazine::NONE, 14 },
    { "BEANS ON TOAST", item_id::BEANS_ON_TOAST, { { item_id::CANNED_BEANS, 1 }, { item_id::BREAD, 1 } }, 2,
      item_id::FRYING_PAN, false, false, 15, magazine::NONE, 8 },
    { "CANNED FOOD", item_id::CANNED_FOOD, { { item_id::JAR, 1 }, { any_produce, 2 }, { item_id::WATER_BOTTLE, 1 } }, 3,
      item_id::COOKING_POT, false, false, 60, magazine::CANNING, 15 },
    { "TOAST", item_id::TOAST, { { item_id::BREAD, 1 } }, 1, item_id::FRYING_PAN, false, false, 10, magazine::NONE, 4 },
    { "HOT CHOCOLATE", item_id::HOT_CHOCOLATE, { { item_id::CHOCOLATE, 1 }, { item_id::WATER_BOTTLE, 1 } }, 2,
      item_id::COOKING_POT, false, false, 10, magazine::NONE, 4 },
    { "ROAST FISH", item_id::ROAST_FISH, { { item_id::FISH, 1 } }, 1, item_id::NONE, false, true, 25, magazine::NONE,
      8 },
    // Made by hand (milestone 12.6): a kitchen knife lashed to two sticks; a bottle of fuel with a rag in its neck.
    { "SPEAR", item_id::SPEAR, { { item_id::STICKS, 2 }, { item_id::KITCHEN_KNIFE, 1 } }, 2, item_id::NONE, false, false,
      20, magazine::NONE, 0, true },
    { "MOLOTOV", item_id::MOLOTOV, { { item_id::EMPTY_BOTTLE, 1 }, { item_id::RAG, 1 }, { fuel_litres, 1 } }, 3,
      item_id::NONE, false, false, 5, magazine::NONE, 0, true },
    // Milestone 13: a deer hide cut into strips and twisted into two ropes (until milestone 14 gives hides more uses).
    { "ROPE FROM HIDE", item_id::ROPE, { { item_id::HIDE, 1 } }, 1, item_id::NONE, false, false, 30, magazine::NONE, 0,
      true, 2 },
    // Milestone 14: the new food cooked; rags boiled clean; things made by hand from the new materials.
    { "PASTA DINNER", item_id::PASTA_DINNER, { { item_id::PASTA, 1 }, { item_id::TOMATO, 1 }, { item_id::WATER_BOTTLE, 1 } },
      3, item_id::COOKING_POT, false, false, 30, magazine::NONE, 12 },
    { "RICE BOWL", item_id::RICE_BOWL, { { item_id::RICE, 1 }, { any_produce, 1 }, { item_id::WATER_BOTTLE, 1 } }, 3,
      item_id::COOKING_POT, false, false, 25, magazine::NONE, 10 },
    { "FRIED EGGS", item_id::FRIED_EGGS, { { item_id::EGGS, 1 } }, 1, item_id::FRYING_PAN, false, false, 10,
      magazine::NONE, 6 },
    { "STERILE BANDAGES", item_id::STERILE_BANDAGE, { { item_id::RAG, 2 } }, 1, item_id::COOKING_POT, true, false, 20,
      magazine::NONE, 2, false, 2 },
    { "RAGS FROM CLOTH", item_id::RAG, { { item_id::CLOTH, 1 } }, 1, item_id::NONE, false, false, 5, magazine::NONE, 0,
      true, 3 },
    { "LOCKPICK", item_id::LOCKPICK, { { item_id::WIRE, 1 } }, 1, item_id::PLIERS, false, false, 15, magazine::NONE, 0,
      true },
    { "SHEET METAL", item_id::SHEET_METAL, { { item_id::SCRAP_METAL, 3 } }, 1, item_id::HAMMER, false, false, 30,
      magazine::NONE, 0, true },
};

constexpr int recipe_count = int(sizeof(recipes) / sizeof(recipe_info));

// A kettle (milestone 14) does instead of the pot for what is only water: boiled water and the hot drinks.
[[nodiscard]] constexpr bool kettle_does(const recipe_info& recipe)
{
    return recipe.tool == item_id::COOKING_POT &&
           (recipe.result == item_id::WATER_BOTTLE || recipe.result == item_id::HERBAL_TEA ||
            recipe.result == item_id::HOT_CHOCOLATE);
}

// What a recipe's tool is called ("COOKING POT").
[[nodiscard]] constexpr const char* tool_name(item_id tool)
{
    return tool == item_id::COOKING_POT ? "COOKING POT" : tool == item_id::FRYING_PAN ? "FRYING PAN" :
           tool == item_id::PLIERS ? "PLIERS" : "HAMMER";
}

namespace cooking_tuning
{
    constexpr int heat_tiles = 2;               // a campfire or stove this close cooks
    constexpr int water_tiles = 3;              // a water source this close boils
    constexpr int faster_percent_per_level = 10;
    constexpr int cooked_cheer = 15;            // spec: cooked food lifts morale
    constexpr int magazine_minutes = 30;        // reading a magazine
}

[[nodiscard]] const char* heat_name(heat_kind value);

}

#endif
