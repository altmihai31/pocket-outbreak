#ifndef KX_ITEMS_H
#define KX_ITEMS_H

#include "kx_combat_tuning.h"
#include "kx_item_icons.h"

namespace kx
{

// ---------------------------------------------------------------------------------------------------------------------
// Item table (spec: Looting, items and inventory). One ROM record per item type; ~45 representative types for
// milestone 3, filled out to 200 in milestone 14.
// ---------------------------------------------------------------------------------------------------------------------

enum class item_category : uint8_t
{
    NONE,
    FOOD,
    MEDICAL,
    WEAPON,
    TOOL,
    MATERIAL,
    CLOTHING,
    BAG,
    MISC,
    BOOK            // skill books: the volume (1-5) is the stack's extra
};

enum class item_rarity : uint8_t
{
    COMMON,
    UNCOMMON,
    RARE
};

// Equipped slots (spec: primary hand, secondary hand, back, and the clothing slots). The off hand (milestone
// 12.6) holds a flashlight or a throwable next to a one-handed weapon; a two-handed weapon empties it. The spec's
// belt is cut: the hotbar refers to carried items directly.
enum class equip_slot : uint8_t
{
    HAND,
    BACK,
    HEAD,
    TORSO,
    JACKET,
    LEGS,
    FEET,
    HANDS,
    OFF_HAND,
    COUNT,
    NONE = COUNT
};

namespace item_flag
{
    constexpr uint8_t edible = 1;
    constexpr uint8_t drink = 2;
    constexpr uint8_t spoils = 4;
    constexpr uint8_t tool = 8;
    constexpr uint8_t fuel = 16;
    constexpr uint8_t raw = 32;         // raw meat and fish: may make you sick (spec)
    constexpr uint8_t poison = 64;      // pale mushrooms
    constexpr uint8_t cooked = 128;     // cooked food lifts morale (spec)
}

enum class item_id : uint8_t
{
    NONE,
    CANNED_BEANS,
    CANNED_TUNA,
    CHIPS,
    CHOCOLATE,
    CRACKERS,
    APPLE,
    BREAD,
    PEANUT_BUTTER,
    SODA,
    WATER_BOTTLE,
    RAW_MEAT,
    BANDAGE,
    RAG,
    DISINFECTANT,
    PAINKILLERS,
    ANTIBIOTICS,
    SPLINT,
    KITCHEN_KNIFE,
    HUNTING_KNIFE,
    BASEBALL_BAT,
    CROWBAR,
    PIPE,
    HAMMER,
    AXE,
    SCREWDRIVER,
    SAW,
    NAILS,
    PLANKS,
    DUCT_TAPE,
    GLUE,
    ROPE,
    HOODIE,
    LEATHER_JACKET,
    RAINCOAT,
    JEANS,
    BOOTS,
    GLOVES,
    BEANIE,
    SCHOOL_BAG,
    HIKING_BAG,
    MILITARY_BAG,
    MAP,
    COMPASS,
    FLASHLIGHT,
    BATTERY,
    MATCHES,
    HOUSE_KEY,
    BOOK_BLUNT,         // skill books, in the order of kx::skill from BLUNT
    BOOK_BLADE,
    BOOK_AXE,
    BOOK_SPEAR,
    BOOK_AIMING,
    BOOK_RELOADING,
    BOOK_FIRST_AID,
    BOOK_SNEAKING,
    BOOK_LIGHTFOOT,
    BOOK_FORAGING,
    BOOK_COOKING,
    BOOK_FARMING,
    BOOK_CARPENTRY,
    BOOK_MECHANICS,
    GARBAGE_BAG,        // base building materials
    STICKS,
    STONES,
    TAP_WATER,          // not a real item: the tap in a sink cabinet (search screen) while the water is on
    RADIO,
    BERRIES,            // foraging (spec: berries, mushrooms (some poisonous), sticks, stones and herbs)
    MUSHROOMS,
    PALE_MUSHROOMS,
    HERBS,
    FISH,
    COOKING_POT,        // cooking (spec: a base (water, pot) with ingredients)
    FRYING_PAN,
    JAR,
    GAS_STOVE,
    PROPANE,
    SOUP,
    STEW,
    FISH_SOUP,
    ROAST_MEAT,
    MUSHROOM_FRY,
    FRIED_FISH,
    BEANS_ON_TOAST,
    TOAST,
    ROAST_FISH,
    HERBAL_TEA,
    HOT_CHOCOLATE,
    CANNED_FOOD,        // home canned: keeps
    MAGAZINE_CANNING,   // spec: magazines teach single recipes
    MAGAZINE_TRAPPING,
    MAGAZINE_GENERATOR,
    SHOVEL,             // farming (spec: plough a plot with a shovel, plant seeds, water it)
    WATERING_CAN,
    SEEDS_CARROT,
    SEEDS_POTATO,
    SEEDS_CABBAGE,
    CARROT,
    POTATO,
    CABBAGE,
    GENERATOR,          // power (spec: a generator, fuel 1 l per ~3 in-game hours)
    FUEL_CAN,
    CAR_BATTERY,        // vehicles (spec: battery, spark plug, radiator water, tyres, fuel; a hose to siphon)
    SPARK_PLUG,
    TYRE,
    HOSE,
    WRENCH,
    CAR_KEY,            // the car it starts is the stack's extra (0: not cut for a car yet)
    SUTURE_NEEDLE,      // milestone 12.5: stitches a deep wound (First Aid 2)
    WHETSTONE,          // sharpens a blade (knives, the axe) back to full, 5 times
    PISTOL,             // milestone 12.6: guns (the rounds loaded are the stack's extra), in the order of kx::gun
    SHOTGUN,
    HUNTING_RIFLE,
    ASSAULT_RIFLE,
    AMMO_9MM,           // their ammunition, in the same order
    AMMO_SHELLS,
    AMMO_RIFLE,
    AMMO_556,
    EMPTY_BOTTLE,       // milestone 12.6 stage 5: throwables, held in the off hand (thrown with R1 and A)
    ALARM_CLOCK,
    MOLOTOV,
    SPEAR,              // crafted from sticks and a kitchen knife; the Spear skill
    NOTE_SAFE,          // milestone 12.7 stage 4: notes, READ in the Gear tab; extra: the safe's house
    NOTE_STASH,         // extra: the settlement whose stash it tells of
    NOTE_LETTER,        // extra: the settlement it names (low byte) and which letter (high byte)
    LOGS,               // milestone 12.7 stage 5: from chopping trees; the log wall
    HINGE,              // the wooden door
    DOORKNOB,
    HIDE,               // milestone 13: from butchering a deer; made into rope
    CIGARETTES,         // the Smoker's (packs of 20)
    LIGHTER,            // lights campfires and cigarettes like matches; 50 uses
    // Milestone 14 stage 5 (docs/milestone-14-plan.md). Food and drink, then the dishes they cook into.
    CANNED_SOUP,
    CANNED_PEACHES,
    PASTA,
    RICE,
    CEREAL,
    COOKIES,
    JUICE,
    BEER,
    ONION,
    TOMATO,
    EGGS,
    HONEY,
    PASTA_DINNER,
    RICE_BOWL,
    FRIED_EGGS,
    STERILE_BANDAGE,    // medical: bandages and disinfects at once
    ALCOHOL_WIPES,      // disinfect, one wipe each
    VITAMINS,           // a little less sick
    SLEEPING_PILLS,     // tired enough to sleep
    COLD_PILLS,         // less sick
    FIRST_AID_KIT,      // opened: bandages, wipes, painkillers, a needle
    MACHETE,            // melee weapons in the existing classes
    HATCHET,            // chops trees like the axe
    GOLF_CLUB,
    SLEDGEHAMMER,       // forces locked doors like the crowbar
    PITCHFORK,
    CLEAVER,
    HOCKEY_STICK,
    POLICE_BATON,
    REVOLVER,           // kx::gun::REVOLVER
    AMMO_38,
    PLIERS,             // tools and materials
    WIRE,
    SCRAP_METAL,
    SHEET_METAL,        // reinforces a barricade without the Carpentry level
    TARP,               // a rain collector's sheet instead of a garbage bag
    CLOTH,
    BINOCULARS,         // scouts the map round you
    SEWING_KIT,         // mends worn clothing with cloth or a rag
    LOCKPICK,           // picks locks faster and surer than a screwdriver
    BUCKET,             // waters plots like the watering can, holds more
    ZIP_TIES,           // mends a weapon like duct tape
    TOOLBOX,            // opened: a few tools and fixings
    FISHING_ROD,        // more finds when foraging by water
    T_SHIRT,            // clothing and bags
    SHIRT,
    SWEATER,
    POLICE_JACKET,
    ARMY_JACKET,
    FIREFIGHTER_JACKET,
    PADDED_JACKET,
    CARGO_PANTS,
    SUIT_TROUSERS,
    OVERALLS,
    SNEAKERS,
    WELLIES,
    ARMY_BOOTS,
    WORK_GLOVES,
    CAP,
    HELMET,
    BANDANA,
    DUFFEL_BAG,
    SATCHEL,
    LARGE_BACKPACK,
    CAR_JACK,           // car parts: tyres in half the time
    RADIATOR,           // repairs a car's body (REPAIR in the car screen)
    BRAKE_PADS,
    SEEDS_TOMATO,       // farming
    SEEDS_ONION,
    KETTLE,             // boils water and makes hot drinks instead of the pot
    MILK,               // animals plan stage 4: a goat milked into an empty bottle
    COUNT
};

static_assert(int(item_id::COUNT) <= 255);

[[nodiscard]] constexpr bool is_note(item_id id)
{
    return id == item_id::NOTE_SAFE || id == item_id::NOTE_STASH || id == item_id::NOTE_LETTER;
}

// Guns (spec: Combat > Guns): fired while aiming (kx_guns.h). In the hand without aiming they are clubs.
[[nodiscard]] constexpr bool is_gun(item_id id)
{
    return (id >= item_id::PISTOL && id <= item_id::ASSAULT_RIFLE) || id == item_id::REVOLVER;
}

[[nodiscard]] constexpr bool is_ammo(item_id id)
{
    return (id >= item_id::AMMO_9MM && id <= item_id::AMMO_556) || id == item_id::AMMO_38;
}

// Milestone 14: what chops trees, forces doors and waters plots.
[[nodiscard]] constexpr bool chops_trees(item_id id)
{
    return id == item_id::AXE || id == item_id::HATCHET;
}

[[nodiscard]] constexpr bool forces_doors(item_id id)
{
    return id == item_id::CROWBAR || id == item_id::SLEDGEHAMMER;
}

[[nodiscard]] constexpr bool waters_plots(item_id id)
{
    return id == item_id::WATERING_CAN || id == item_id::BUCKET;
}

// Throwables (spec: Combat > Throwables): held in the off hand, thrown while aiming without a gun.
[[nodiscard]] constexpr bool is_throwable(item_id id)
{
    return id == item_id::EMPTY_BOTTLE || id == item_id::ALARM_CLOCK || id == item_id::MOLOTOV;
}

struct item_info
{
    const char* name;
    item_icon icon;
    item_category category;
    uint16_t weight;            // per unit, in 10 g (spec uses 0.1 kg; nails need finer)
    uint8_t stack;              // max quantity per slot; 1 = not stackable
    uint8_t durability;         // max condition (hits for weapons, wear for clothing); 0 = no condition
    int8_t food;                // nutrition (applied from milestone 4)
    int8_t water;
    uint8_t flags;
    item_rarity rarity;
    weapon melee;               // weapon class for melee items, FISTS otherwise
    equip_slot slot;            // where it is worn or held; NONE for carried-only items
    uint8_t scratch_defense;    // % chance to stop a scratch or laceration on covered parts
    uint8_t bite_defense;       // % chance to turn a bite into a scratch
    uint8_t insulation;         // used from milestone 4
    uint8_t capacity;           // bags: extra carry capacity in kg
    uint8_t fresh_days = 0;     // perishable food: days fresh, then stale; rotten after twice that
};

namespace detail
{
    constexpr item_info make_food(const char* name, item_icon icon, int weight, int food, int water, uint8_t flags,
                                  item_rarity rarity = item_rarity::COMMON, int fresh_days = 0)
    {
        return item_info{ name, icon, item_category::FOOD, uint16_t(weight), 1, 0, int8_t(food), int8_t(water),
                          uint8_t(item_flag::edible | flags), rarity, weapon::FISTS, equip_slot::NONE, 0, 0, 0, 0,
                          uint8_t(fresh_days) };
    }

    constexpr item_info make_item(const char* name, item_icon icon, item_category category, int weight, int stack,
                                  item_rarity rarity = item_rarity::COMMON, uint8_t flags = 0)
    {
        return item_info{ name, icon, category, uint16_t(weight), uint8_t(stack), 0, 0, 0, flags, rarity,
                          weapon::FISTS, equip_slot::NONE, 0, 0, 0, 0 };
    }

    // An item held in the off hand (a flashlight, a throwable).
    constexpr item_info off_hand(item_info value)
    {
        value.slot = equip_slot::OFF_HAND;
        return value;
    }

    constexpr item_info make_weapon(const char* name, item_icon icon, int weight, weapon melee, int durability,
                                    item_rarity rarity = item_rarity::COMMON, uint8_t flags = 0)
    {
        return item_info{ name, icon, item_category::WEAPON, uint16_t(weight), 1, uint8_t(durability), 0, 0, flags,
                          rarity, melee, equip_slot::HAND, 0, 0, 0, 0 };
    }

    constexpr item_info make_clothing(const char* name, item_icon icon, int weight, equip_slot slot, int scratch,
                                      int bite, int insulation, item_rarity rarity = item_rarity::COMMON)
    {
        return item_info{ name, icon, item_category::CLOTHING, uint16_t(weight), 1, 100, 0, 0, 0, rarity,
                          weapon::FISTS, slot, uint8_t(scratch), uint8_t(bite), uint8_t(insulation), 0 };
    }

    // Tools used up a little at a time (a propane tank): uses as the condition.
    constexpr item_info make_tool(const char* name, item_icon icon, int weight, int uses,
                                  item_rarity rarity = item_rarity::COMMON)
    {
        return item_info{ name, icon, item_category::TOOL, uint16_t(weight), 1, uint8_t(uses), 0, 0, item_flag::tool,
                          rarity, weapon::FISTS, equip_slot::NONE, 0, 0, 0, 0 };
    }

    constexpr item_info make_dish(const char* name, item_icon icon, int food, int water, int fresh_days)
    {
        return make_food(name, icon, 40, food, water,
                         uint8_t(item_flag::cooked | (fresh_days ? item_flag::spoils : 0)), item_rarity::COMMON,
                         fresh_days);
    }

    constexpr item_info make_bag(const char* name, item_icon icon, int weight, int capacity, item_rarity rarity)
    {
        return item_info{ name, icon, item_category::BAG, uint16_t(weight), 1, 0, 0, 0, 0, rarity, weapon::FISTS,
                          equip_slot::BACK, 0, 0, 0, uint8_t(capacity) };
    }
}

// Weights in 10 g. Clothing defense values are ours (the spec only says a leather jacket can turn a bite
// into a scratch).
constexpr item_info item_infos[] = {
    detail::make_item("", item_icon::EMPTY, item_category::NONE, 0, 0),
    detail::make_food("Canned beans", item_icon::CANNED_BEANS, 40, 35, -5, 0),
    detail::make_food("Canned tuna", item_icon::CANNED_TUNA, 30, 25, 0, 0),
    detail::make_food("Chips", item_icon::CHIPS, 20, 15, -5, 0),
    detail::make_food("Chocolate", item_icon::CHOCOLATE, 10, 20, 0, 0),
    detail::make_food("Crackers", item_icon::CRACKERS, 30, 15, -5, 0),
    detail::make_food("Apple", item_icon::APPLE, 20, 10, 5, item_flag::spoils, item_rarity::COMMON, 7),
    detail::make_food("Bread", item_icon::BREAD, 50, 25, 0, item_flag::spoils, item_rarity::COMMON, 5),
    detail::make_food("Peanut butter", item_icon::PEANUT_BUTTER, 50, 40, -5, 0, item_rarity::UNCOMMON),
    detail::make_food("Soda", item_icon::SODA, 40, 5, 25, item_flag::drink),
    detail::make_food("Water bottle", item_icon::WATER_BOTTLE, 60, 0, 40, item_flag::drink),
    detail::make_food("Raw meat", item_icon::RAW_MEAT, 50, 30, 0, item_flag::spoils | item_flag::raw,
                      item_rarity::UNCOMMON, 2),
    detail::make_item("Bandage", item_icon::BANDAGE, item_category::MEDICAL, 10, 4),
    detail::make_item("Rag", item_icon::RAG, item_category::MEDICAL, 10, 4),
    detail::make_item("Disinfectant", item_icon::DISINFECTANT, item_category::MEDICAL, 30, 1, item_rarity::UNCOMMON),
    detail::make_item("Painkillers", item_icon::PAINKILLERS, item_category::MEDICAL, 10, 1),
    detail::make_item("Antibiotics", item_icon::ANTIBIOTICS, item_category::MEDICAL, 10, 1, item_rarity::RARE),
    detail::make_item("Splint", item_icon::SPLINT, item_category::MEDICAL, 50, 1, item_rarity::UNCOMMON),
    detail::make_weapon("Kitchen knife", item_icon::KITCHEN_KNIFE, 30, weapon::KNIFE, 60),
    detail::make_weapon("Hunting knife", item_icon::HUNTING_KNIFE, 40, weapon::KNIFE, 90, item_rarity::UNCOMMON),
    detail::make_weapon("Baseball bat", item_icon::BASEBALL_BAT, 100, weapon::BAT, 50),
    detail::make_weapon("Crowbar", item_icon::CROWBAR, 150, weapon::BAT, 120, item_rarity::UNCOMMON, item_flag::tool),
    detail::make_weapon("Pipe", item_icon::PIPE, 120, weapon::BAT, 70),
    detail::make_weapon("Hammer", item_icon::HAMMER, 80, weapon::BAT, 60, item_rarity::COMMON, item_flag::tool),
    detail::make_weapon("Axe", item_icon::AXE, 200, weapon::AXE, 70, item_rarity::UNCOMMON, item_flag::tool),
    detail::make_item("Screwdriver", item_icon::SCREWDRIVER, item_category::TOOL, 20, 1, item_rarity::COMMON,
                      item_flag::tool),
    detail::make_item("Saw", item_icon::SAW, item_category::TOOL, 80, 1, item_rarity::UNCOMMON, item_flag::tool),
    detail::make_item("Nails", item_icon::NAILS, item_category::MATERIAL, 1, 50),
    detail::make_item("Planks", item_icon::PLANK, item_category::MATERIAL, 100, 4, item_rarity::COMMON,
                      item_flag::fuel),
    detail::make_item("Duct tape", item_icon::DUCT_TAPE, item_category::MATERIAL, 20, 1),
    detail::make_item("Glue", item_icon::GLUE, item_category::MATERIAL, 10, 1),
    detail::make_item("Rope", item_icon::ROPE, item_category::MATERIAL, 50, 1),
    detail::make_clothing("Hoodie", item_icon::HOODIE, 60, equip_slot::TORSO, 20, 0, 3),
    detail::make_clothing("Leather jacket", item_icon::LEATHER_JACKET, 150, equip_slot::JACKET, 60, 50, 3,
                          item_rarity::UNCOMMON),
    detail::make_clothing("Raincoat", item_icon::RAINCOAT, 50, equip_slot::JACKET, 10, 0, 1),
    detail::make_clothing("Jeans", item_icon::JEANS, 60, equip_slot::LEGS, 30, 10, 2),
    detail::make_clothing("Boots", item_icon::BOOTS, 80, equip_slot::FEET, 40, 20, 2),
    detail::make_clothing("Gloves", item_icon::GLOVES, 10, equip_slot::HANDS, 40, 10, 1),
    detail::make_clothing("Beanie", item_icon::BEANIE, 10, equip_slot::HEAD, 0, 0, 2),
    detail::make_bag("School bag", item_icon::SCHOOL_BAG, 50, 8, item_rarity::COMMON),        // spec: +8
    detail::make_bag("Hiking bag", item_icon::HIKING_BAG, 80, 14, item_rarity::UNCOMMON),     // spec: +14
    detail::make_bag("Military backpack", item_icon::MILITARY_BAG, 100, 18, item_rarity::RARE), // spec: +18
    detail::make_item("Map", item_icon::MAP, item_category::MISC, 10, 1, item_rarity::UNCOMMON),
    detail::make_item("Compass", item_icon::COMPASS, item_category::MISC, 10, 1, item_rarity::UNCOMMON),
    detail::off_hand(detail::make_item("Flashlight", item_icon::FLASHLIGHT, item_category::MISC, 40, 1,
                                       item_rarity::COMMON, item_flag::tool)),
    detail::make_item("Battery", item_icon::BATTERY, item_category::MISC, 10, 4),
    detail::make_item("Matches", item_icon::MATCHES, item_category::MISC, 10, 1),
    detail::make_item("House key", item_icon::HOUSE_KEY, item_category::MISC, 1, 1, item_rarity::UNCOMMON),
    detail::make_item("Blunt book", item_icon::BOOK_RED, item_category::BOOK, 40, 1, item_rarity::UNCOMMON),
    detail::make_item("Blade book", item_icon::BOOK_RED, item_category::BOOK, 40, 1, item_rarity::UNCOMMON),
    detail::make_item("Axe book", item_icon::BOOK_RED, item_category::BOOK, 40, 1, item_rarity::UNCOMMON),
    detail::make_item("Spear book", item_icon::BOOK_RED, item_category::BOOK, 40, 1, item_rarity::UNCOMMON),
    detail::make_item("Aiming book", item_icon::BOOK_RED, item_category::BOOK, 40, 1, item_rarity::UNCOMMON),
    detail::make_item("Reloading book", item_icon::BOOK_RED, item_category::BOOK, 40, 1, item_rarity::UNCOMMON),
    detail::make_item("First aid book", item_icon::BOOK_GREEN, item_category::BOOK, 40, 1, item_rarity::UNCOMMON),
    detail::make_item("Sneaking book", item_icon::BOOK_GREEN, item_category::BOOK, 40, 1, item_rarity::UNCOMMON),
    detail::make_item("Lightfoot book", item_icon::BOOK_GREEN, item_category::BOOK, 40, 1, item_rarity::UNCOMMON),
    detail::make_item("Foraging book", item_icon::BOOK_GREEN, item_category::BOOK, 40, 1, item_rarity::UNCOMMON),
    detail::make_item("Cooking book", item_icon::BOOK_GREEN, item_category::BOOK, 40, 1, item_rarity::UNCOMMON),
    detail::make_item("Farming book", item_icon::BOOK_GREEN, item_category::BOOK, 40, 1, item_rarity::UNCOMMON),
    detail::make_item("Carpentry book", item_icon::BOOK_BLUE, item_category::BOOK, 40, 1, item_rarity::UNCOMMON),
    detail::make_item("Mechanics book", item_icon::BOOK_BLUE, item_category::BOOK, 40, 1, item_rarity::UNCOMMON),
    detail::make_item("Garbage bag", item_icon::GARBAGE_BAG, item_category::MATERIAL, 5, 4),
    detail::make_item("Sticks", item_icon::STICKS, item_category::MATERIAL, 20, 10, item_rarity::COMMON,
                      item_flag::fuel),
    detail::make_item("Stones", item_icon::STONES, item_category::MATERIAL, 50, 10),
    detail::make_food("Tap water", item_icon::TAP_WATER, 0, 0, 25, item_flag::drink),
    detail::make_item("Radio", item_icon::RADIO, item_category::MISC, 60, 1, item_rarity::UNCOMMON),
    detail::make_food("Berries", item_icon::BERRIES, 10, 8, 4, item_flag::spoils, item_rarity::COMMON, 3),
    detail::make_food("Mushrooms", item_icon::MUSHROOMS, 10, 10, 0, item_flag::spoils, item_rarity::COMMON, 3),
    detail::make_food("Pale mushrooms", item_icon::PALE_MUSHROOMS, 10, 10, 0, item_flag::spoils | item_flag::poison,
                      item_rarity::COMMON, 3),
    detail::make_food("Herbs", item_icon::HERBS, 5, 2, 0, item_flag::spoils, item_rarity::COMMON, 5),
    detail::make_food("Fish", item_icon::FISH, 40, 25, 0, item_flag::spoils | item_flag::raw, item_rarity::UNCOMMON,
                      2),
    detail::make_item("Cooking pot", item_icon::COOKING_POT, item_category::TOOL, 100, 1, item_rarity::COMMON,
                      item_flag::tool),
    detail::make_weapon("Frying pan", item_icon::FRYING_PAN, 90, weapon::BAT, 60, item_rarity::COMMON,
                        item_flag::tool),
    detail::make_item("Jar", item_icon::JAR, item_category::MATERIAL, 30, 4),
    detail::make_item("Gas stove", item_icon::GAS_STOVE, item_category::TOOL, 150, 1, item_rarity::UNCOMMON,
                      item_flag::tool),
    detail::make_tool("Propane tank", item_icon::PROPANE, 300, 8, item_rarity::UNCOMMON),
    detail::make_dish("Vegetable soup", item_icon::SOUP, 40, 15, 3),
    detail::make_dish("Stew", item_icon::STEW, 60, 10, 3),
    detail::make_dish("Fish soup", item_icon::FISH_SOUP, 45, 15, 3),
    detail::make_dish("Roast meat", item_icon::ROAST_MEAT, 40, 0, 3),
    detail::make_dish("Mushroom fry", item_icon::MUSHROOM_FRY, 25, 0, 2),
    detail::make_dish("Fried fish", item_icon::FRIED_FISH, 35, 0, 2),
    detail::make_dish("Beans on toast", item_icon::BEANS_ON_TOAST, 55, -5, 2),
    detail::make_dish("Toast", item_icon::TOAST, 25, -5, 3),
    detail::make_dish("Roast fish", item_icon::ROAST_FISH, 30, 0, 2),
    detail::make_dish("Herbal tea", item_icon::HERBAL_TEA, 2, 30, 1),
    detail::make_dish("Hot chocolate", item_icon::HOT_CHOCOLATE, 15, 25, 1),
    detail::make_dish("Canned food", item_icon::CANNED_FOOD, 40, 5, 0),
    detail::make_item("Canning magazine", item_icon::MAGAZINE_CANNING, item_category::BOOK, 20, 1,
                      item_rarity::UNCOMMON),
    detail::make_item("Trapping magazine", item_icon::MAGAZINE_TRAPPING, item_category::BOOK, 20, 1,
                      item_rarity::UNCOMMON),
    detail::make_item("Generator magazine", item_icon::MAGAZINE_GENERATOR, item_category::BOOK, 20, 1,
                      item_rarity::UNCOMMON),
    detail::make_weapon("Shovel", item_icon::SHOVEL, 150, weapon::BAT, 80, item_rarity::COMMON, item_flag::tool),
    detail::make_tool("Watering can", item_icon::WATERING_CAN, 80, 10),
    detail::make_item("Carrot seeds", item_icon::SEEDS_CARROT, item_category::MISC, 1, 10),
    detail::make_item("Potato seeds", item_icon::SEEDS_POTATO, item_category::MISC, 1, 10),
    detail::make_item("Cabbage seeds", item_icon::SEEDS_CABBAGE, item_category::MISC, 1, 10, item_rarity::UNCOMMON),
    detail::make_food("Carrot", item_icon::CARROT, 10, 10, 2, item_flag::spoils, item_rarity::COMMON, 10),
    detail::make_food("Potato", item_icon::POTATO, 20, 18, 0, item_flag::spoils, item_rarity::COMMON, 20),
    detail::make_food("Cabbage", item_icon::CABBAGE, 40, 15, 5, item_flag::spoils, item_rarity::COMMON, 7),
    detail::make_item("Generator", item_icon::GENERATOR, item_category::TOOL, 2500, 1, item_rarity::RARE),
    detail::make_tool("Fuel can", item_icon::FUEL_CAN, 600, 10, item_rarity::UNCOMMON),
    detail::make_item("Car battery", item_icon::CAR_BATTERY, item_category::MATERIAL, 1200, 1, item_rarity::UNCOMMON),
    detail::make_item("Spark plug", item_icon::SPARK_PLUG, item_category::MATERIAL, 10, 1, item_rarity::UNCOMMON),
    detail::make_item("Tyre", item_icon::TYRE, item_category::MATERIAL, 800, 1, item_rarity::UNCOMMON),
    detail::make_item("Hose", item_icon::HOSE, item_category::TOOL, 50, 1, item_rarity::UNCOMMON, item_flag::tool),
    detail::make_item("Wrench", item_icon::WRENCH, item_category::TOOL, 80, 1, item_rarity::COMMON, item_flag::tool),
    detail::make_item("Car key", item_icon::CAR_KEY, item_category::MISC, 1, 1, item_rarity::UNCOMMON),
    detail::make_item("Suture needle", item_icon::SUTURE_NEEDLE, item_category::MEDICAL, 5, 4, item_rarity::UNCOMMON),
    detail::make_tool("Whetstone", item_icon::WHETSTONE, 40, 5, item_rarity::UNCOMMON),
    // Guns do not wear (durability 0). Swung, the pistol hits like a fist and the long guns like a bat.
    detail::make_weapon("Pistol", item_icon::PISTOL, 90, weapon::FISTS, 0, item_rarity::UNCOMMON),
    detail::make_weapon("Shotgun", item_icon::SHOTGUN, 330, weapon::BAT, 0, item_rarity::RARE),
    detail::make_weapon("Hunting rifle", item_icon::HUNTING_RIFLE, 350, weapon::BAT, 0, item_rarity::RARE),
    detail::make_weapon("Assault rifle", item_icon::ASSAULT_RIFLE, 320, weapon::BAT, 0, item_rarity::RARE),
    detail::make_item("9mm rounds", item_icon::AMMO_9MM, item_category::MISC, 1, 30, item_rarity::UNCOMMON),
    detail::make_item("Shotgun shells", item_icon::AMMO_SHELLS, item_category::MISC, 4, 12, item_rarity::UNCOMMON),
    detail::make_item("Rifle rounds", item_icon::AMMO_RIFLE, item_category::MISC, 2, 10, item_rarity::UNCOMMON),
    detail::make_item("5.56 rounds", item_icon::AMMO_556, item_category::MISC, 1, 30, item_rarity::RARE),
    // Throwables go in the off hand; stacks are taken up one at a time.
    detail::off_hand(detail::make_item("Empty bottle", item_icon::EMPTY_BOTTLE, item_category::MISC, 30, 4)),
    detail::off_hand(detail::make_item("Alarm clock", item_icon::ALARM_CLOCK, item_category::MISC, 30, 1)),
    detail::off_hand(detail::make_item("Molotov", item_icon::MOLOTOV, item_category::MISC, 60, 2,
                                       item_rarity::UNCOMMON)),
    detail::make_weapon("Spear", item_icon::SPEAR, 120, weapon::SPEAR, 40, item_rarity::UNCOMMON),
    detail::make_item("Note", item_icon::NOTE, item_category::MISC, 1, 1, item_rarity::UNCOMMON),
    detail::make_item("Note", item_icon::NOTE, item_category::MISC, 1, 1, item_rarity::UNCOMMON),
    detail::make_item("Note", item_icon::NOTE, item_category::MISC, 1, 1, item_rarity::UNCOMMON),
    detail::make_item("Logs", item_icon::LOGS, item_category::MATERIAL, 500, 4, item_rarity::COMMON, item_flag::fuel),
    detail::make_item("Hinge", item_icon::HINGE, item_category::MATERIAL, 20, 4, item_rarity::UNCOMMON),
    detail::make_item("Doorknob", item_icon::DOORKNOB, item_category::MATERIAL, 20, 4, item_rarity::UNCOMMON),
    detail::make_item("Deer hide", item_icon::HIDE, item_category::MATERIAL, 150, 2, item_rarity::UNCOMMON),
    detail::make_item("Cigarettes", item_icon::CIGARETTES, item_category::MISC, 1, 20),
    detail::make_tool("Lighter", item_icon::LIGHTER, 5, 50),
    // Milestone 14 stage 5.
    detail::make_food("Canned soup", item_icon::CANNED_SOUP, 40, 25, 10, 0),
    detail::make_food("Canned peaches", item_icon::CANNED_PEACHES, 40, 20, 10, 0),
    detail::make_food("Pasta", item_icon::PASTA, 50, 15, -10, 0),
    detail::make_food("Rice", item_icon::RICE, 50, 15, -10, 0),
    detail::make_food("Cereal", item_icon::CEREAL, 30, 20, -5, 0),
    detail::make_food("Cookies", item_icon::COOKIES, 20, 15, -5, 0),
    detail::make_food("Juice", item_icon::JUICE, 40, 5, 30, item_flag::drink),
    detail::make_food("Beer", item_icon::BEER, 50, 5, 15, item_flag::drink, item_rarity::UNCOMMON),
    detail::make_food("Onion", item_icon::ONION, 10, 6, 2, item_flag::spoils, item_rarity::COMMON, 20),
    detail::make_food("Tomato", item_icon::TOMATO, 15, 6, 6, item_flag::spoils, item_rarity::COMMON, 7),
    detail::make_food("Eggs", item_icon::EGGS, 10, 12, 0, item_flag::spoils | item_flag::raw, item_rarity::UNCOMMON,
                      10),
    detail::make_food("Honey", item_icon::HONEY, 40, 30, 0, 0, item_rarity::UNCOMMON),
    detail::make_dish("Pasta dinner", item_icon::PASTA_DINNER, 55, 5, 3),
    detail::make_dish("Rice bowl", item_icon::RICE_BOWL, 45, 5, 3),
    detail::make_dish("Fried eggs", item_icon::FRIED_EGGS, 25, 0, 2),
    detail::make_item("Sterile bandage", item_icon::STERILE_BANDAGE, item_category::MEDICAL, 10, 4,
                      item_rarity::UNCOMMON),
    detail::make_item("Alcohol wipes", item_icon::ALCOHOL_WIPES, item_category::MEDICAL, 5, 4),
    detail::make_item("Vitamins", item_icon::VITAMINS, item_category::MEDICAL, 10, 1),
    detail::make_item("Sleeping pills", item_icon::SLEEPING_PILLS, item_category::MEDICAL, 10, 1,
                      item_rarity::UNCOMMON),
    detail::make_item("Cold pills", item_icon::COLD_PILLS, item_category::MEDICAL, 10, 1),
    detail::make_item("First aid kit", item_icon::FIRST_AID_KIT, item_category::MEDICAL, 60, 1, item_rarity::RARE),
    detail::make_weapon("Machete", item_icon::MACHETE, 90, weapon::AXE, 80, item_rarity::UNCOMMON),
    detail::make_weapon("Hatchet", item_icon::HATCHET, 80, weapon::AXE, 60, item_rarity::COMMON, item_flag::tool),
    detail::make_weapon("Golf club", item_icon::GOLF_CLUB, 70, weapon::BAT, 40),
    detail::make_weapon("Sledgehammer", item_icon::SLEDGEHAMMER, 400, weapon::AXE, 120, item_rarity::RARE,
                        item_flag::tool),
    detail::make_weapon("Pitchfork", item_icon::PITCHFORK, 150, weapon::SPEAR, 60, item_rarity::COMMON,
                        item_flag::tool),
    detail::make_weapon("Cleaver", item_icon::CLEAVER, 50, weapon::KNIFE, 70),
    detail::make_weapon("Hockey stick", item_icon::HOCKEY_STICK, 80, weapon::BAT, 45),
    detail::make_weapon("Police baton", item_icon::POLICE_BATON, 60, weapon::BAT, 100, item_rarity::UNCOMMON),
    detail::make_weapon("Revolver", item_icon::REVOLVER, 100, weapon::FISTS, 0, item_rarity::RARE),
    detail::make_item(".38 rounds", item_icon::AMMO_38, item_category::MISC, 1, 30, item_rarity::UNCOMMON),
    detail::make_item("Pliers", item_icon::PLIERS, item_category::TOOL, 20, 1, item_rarity::COMMON, item_flag::tool),
    detail::make_item("Wire", item_icon::WIRE, item_category::MATERIAL, 10, 5),
    detail::make_item("Scrap metal", item_icon::SCRAP_METAL, item_category::MATERIAL, 80, 4),
    detail::make_item("Sheet metal", item_icon::SHEET_METAL, item_category::MATERIAL, 300, 2, item_rarity::UNCOMMON),
    detail::make_item("Tarp", item_icon::TARP, item_category::MATERIAL, 100, 1),
    detail::make_item("Cloth", item_icon::CLOTH, item_category::MATERIAL, 10, 4),
    detail::make_item("Binoculars", item_icon::BINOCULARS, item_category::MISC, 60, 1, item_rarity::UNCOMMON),
    detail::make_tool("Sewing kit", item_icon::SEWING_KIT, 20, 10, item_rarity::UNCOMMON),
    detail::make_item("Lockpick", item_icon::LOCKPICK, item_category::TOOL, 1, 1, item_rarity::UNCOMMON,
                      item_flag::tool),
    detail::make_tool("Bucket", item_icon::BUCKET, 80, 20),
    detail::make_item("Zip ties", item_icon::ZIP_TIES, item_category::MATERIAL, 5, 4),
    detail::make_item("Toolbox", item_icon::TOOLBOX, item_category::TOOL, 300, 1, item_rarity::UNCOMMON),
    detail::make_item("Fishing rod", item_icon::FISHING_ROD, item_category::TOOL, 100, 1, item_rarity::UNCOMMON,
                      item_flag::tool),
    detail::make_clothing("T-shirt", item_icon::T_SHIRT, 20, equip_slot::TORSO, 5, 0, 1),
    detail::make_clothing("Shirt", item_icon::SHIRT, 30, equip_slot::TORSO, 10, 0, 2),
    detail::make_clothing("Sweater", item_icon::SWEATER, 60, equip_slot::TORSO, 15, 0, 4),
    detail::make_clothing("Police jacket", item_icon::POLICE_JACKET, 120, equip_slot::JACKET, 45, 30, 3,
                          item_rarity::UNCOMMON),
    detail::make_clothing("Army jacket", item_icon::ARMY_JACKET, 130, equip_slot::JACKET, 50, 35, 3,
                          item_rarity::UNCOMMON),
    detail::make_clothing("Firefighter jacket", item_icon::FIREFIGHTER_JACKET, 200, equip_slot::JACKET, 60, 45, 4,
                          item_rarity::RARE),
    detail::make_clothing("Padded jacket", item_icon::PADDED_JACKET, 100, equip_slot::JACKET, 25, 10, 5),
    detail::make_clothing("Cargo pants", item_icon::CARGO_PANTS, 70, equip_slot::LEGS, 35, 10, 2),
    detail::make_clothing("Suit trousers", item_icon::SUIT_TROUSERS, 50, equip_slot::LEGS, 15, 0, 1),
    detail::make_clothing("Overalls", item_icon::OVERALLS, 80, equip_slot::LEGS, 35, 15, 2),
    detail::make_clothing("Sneakers", item_icon::SNEAKERS, 50, equip_slot::FEET, 15, 0, 1),
    detail::make_clothing("Wellies", item_icon::WELLIES, 90, equip_slot::FEET, 30, 10, 1),
    detail::make_clothing("Army boots", item_icon::ARMY_BOOTS, 100, equip_slot::FEET, 50, 30, 2,
                          item_rarity::UNCOMMON),
    detail::make_clothing("Work gloves", item_icon::WORK_GLOVES, 15, equip_slot::HANDS, 50, 15, 1),
    detail::make_clothing("Cap", item_icon::CAP, 10, equip_slot::HEAD, 0, 0, 1),
    detail::make_clothing("Helmet", item_icon::HELMET, 150, equip_slot::HEAD, 30, 40, 1, item_rarity::RARE),
    detail::make_clothing("Bandana", item_icon::BANDANA, 5, equip_slot::HEAD, 0, 0, 1),
    detail::make_bag("Duffel bag", item_icon::DUFFEL_BAG, 90, 12, item_rarity::UNCOMMON),
    detail::make_bag("Satchel", item_icon::SATCHEL, 40, 5, item_rarity::COMMON),
    detail::make_bag("Large backpack", item_icon::LARGE_BACKPACK, 120, 22, item_rarity::RARE),
    detail::make_item("Car jack", item_icon::CAR_JACK, item_category::TOOL, 400, 1, item_rarity::UNCOMMON,
                      item_flag::tool),
    detail::make_item("Radiator", item_icon::RADIATOR, item_category::MATERIAL, 500, 1, item_rarity::UNCOMMON),
    detail::make_item("Brake pads", item_icon::BRAKE_PADS, item_category::MATERIAL, 80, 1, item_rarity::UNCOMMON),
    detail::make_item("Tomato seeds", item_icon::SEEDS_TOMATO, item_category::MISC, 1, 10),
    detail::make_item("Onion seeds", item_icon::SEEDS_ONION, item_category::MISC, 1, 10),
    detail::make_item("Kettle", item_icon::KETTLE, item_category::TOOL, 80, 1, item_rarity::COMMON, item_flag::tool),
    detail::make_food("Milk", item_icon::MILK, 60, 10, 30, item_flag::drink | item_flag::spoils, item_rarity::COMMON, 2),
};

static_assert(sizeof(item_infos) / sizeof(item_info) == int(item_id::COUNT));

// Weapons that need both hands, so nothing can be held in the off hand with them: everything swung except knives,
// the hammer, the frying pan, the hatchet, the baton and the handguns (the bat, crowbar, pipe, shovel, axe, spear,
// machete, sledgehammer and the long guns).
[[nodiscard]] constexpr bool two_handed(item_id id)
{
    const item_info& item = item_infos[int(id)];
    return item.category == item_category::WEAPON && item.melee != weapon::KNIFE && id != item_id::HAMMER &&
           id != item_id::FRYING_PAN && id != item_id::PISTOL && id != item_id::REVOLVER && id != item_id::HATCHET &&
           id != item_id::POLICE_BATON;
}

[[nodiscard]] constexpr const item_info& info(item_id id)
{
    return item_infos[int(id)];
}

// What the milestone 14 items do when used (docs/milestone-14-plan.md stage 5).
namespace item_tuning
{
    constexpr int vitamins_sickness = 10;       // sickness taken off (of 100)
    constexpr int cold_pills_sickness = 30;
    constexpr int sleeping_pills_fatigue = 52;  // fatigue raised to at least this % (Tired starts at 50)
    constexpr int scout_chunks = 3;             // binoculars: chunks each way revealed on the map
    constexpr int sew_condition = 40;           // a sewing kit's mend, on the worn piece most worn
    constexpr int lockpick_frames = 180;        // a lockpick: 3 s (a screwdriver takes 5)
    constexpr int lockpick_fail_percent = 20;   // before Sneaking (a screwdriver slips 40%)
    constexpr int zip_ties_percent = 40;        // of a weapon's condition mended (duct tape 50, glue 25)
    constexpr int jack_percent = 50;            // tyre work with a car jack takes this much of the time
    constexpr int radiator_repair = 40;         // car condition put back by a radiator
    constexpr int brake_pads_repair = 25;
    constexpr int fishing_rod_finds = 2;        // more finds foraging by water with a rod
    constexpr int toolbox_items = 3;            // what an opened toolbox holds
}

// An item in a slot: 4 bytes, as in the spec's item instance (type, condition or quantity, extra state).
struct item_stack
{
    item_id id = item_id::NONE;
    uint8_t amount = 0;         // quantity for stackables, condition for items with durability, 1 otherwise
    uint16_t extra = 0;         // freshness day, ammo loaded, house id (later milestones)

    [[nodiscard]] bool empty() const
    {
        return id == item_id::NONE;
    }

    [[nodiscard]] int quantity() const
    {
        return info(id).durability > 0 ? 1 : amount;
    }

    // Total weight in 10 g units.
    [[nodiscard]] int weight() const
    {
        return info(id).weight * quantity();
    }
};

static_assert(sizeof(item_stack) == 4);

}

#endif
