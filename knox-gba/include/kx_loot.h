#ifndef KX_LOOT_H
#define KX_LOOT_H

#include "bn_span.h"

#include "kx_items.h"
#include "kx_tuning.h"

namespace kx
{

// Loot tables by room type (spec: Containers and loot tables). A container rolls 0-8 items from its table the
// first time it is opened, weighted per entry (common entries carry higher weights than rare ones).
enum class loot_table : uint8_t
{
    KITCHEN,
    FRIDGE,
    BATHROOM,
    BEDROOM,
    GARAGE,
    SHOP_SHELF,
    POLICE_LOCKER,
    CAR_TRUNK,
    ZOMBIE_CORPSE,
    MEDICINE,       // clinic cabinets
    COUNTER,        // shop and petrol station tills
    CRATE,          // barns and farm storage
    DESK,           // police and clinic offices
    CAMP,           // abandoned tents in the wild
    BODY,           // dead survivors in the wild
    MILITARY,       // crash sites (world events)
    FORAGE_FOREST,  // foraging (spec: search mode in forests and fields)
    FORAGE_FIELD,
    FORAGE_WATER,
    POLICE_CORPSE,  // zombies by outfit (spec: Zombies > Outfit)
    SOLDIER_CORPSE,
    DOCTOR_CORPSE,
    FARMER_CORPSE,
    GLOVE_BOX,      // cars (spec: wrecks are containers: trunk, glove box)
    FLOOR,          // dropped items; never rolled
    GUN_SAFE,       // milestone 12.7: opened with its code
    STASH,          // left in the wild, told of by a note
    FREEZER,        // milestone 13: chest freezers in shops, petrol stations and some kitchens
    SCHOOL,         // milestone 14: school desks and school lockers
    OFFICE,         // filing cabinets; post office counters
    WAREHOUSE,      // warehouse racks
    ARMORY,         // gun racks on the military base
    VENDING,        // vending machines
    FIRE_STATION,   // fire station lockers
    PUB,            // pub and diner counters
    HUNTER,         // a hunter's cabin's gun rack
    BUS,            // stage 4: the crashed coach's luggage
    HUNTER_STAND,   // hunter stands in the wild
    FOOTLOCKER,     // stage 7: lockers on the military base (the crash sites' crate table was too rich for them)
    COUNT
};

struct loot_entry
{
    item_id item;
    uint8_t weight;
    uint8_t min_amount;     // stackables
    uint8_t max_amount;
};

struct loot_table_info
{
    const char* container_name;     // shown when searching
    const char* short_name;         // container panel title
    uint8_t min_items;
    uint8_t max_items;
    bn::span<const loot_entry> entries;
};

namespace detail
{
    constexpr loot_entry kitchen_loot[] = {
        { item_id::CANNED_BEANS, 12, 1, 1 }, { item_id::CANNED_TUNA, 10, 1, 1 }, { item_id::CRACKERS, 8, 1, 1 },
        { item_id::CHIPS, 8, 1, 1 }, { item_id::PEANUT_BUTTER, 4, 1, 1 }, { item_id::BREAD, 5, 1, 1 },
        { item_id::SODA, 6, 1, 1 }, { item_id::WATER_BOTTLE, 6, 1, 1 }, { item_id::KITCHEN_KNIFE, 6, 1, 1 },
        { item_id::MATCHES, 4, 1, 1 }, { item_id::RAG, 4, 1, 2 }, { item_id::FLASHLIGHT, 2, 1, 1 },
        { item_id::BATTERY, 3, 1, 2 }, { item_id::HUNTING_KNIFE, 1, 1, 1 }, { item_id::GARBAGE_BAG, 4, 1, 2 },
        { item_id::COOKING_POT, 4, 1, 1 }, { item_id::FRYING_PAN, 4, 1, 1 }, { item_id::JAR, 3, 1, 3 },
        { item_id::EMPTY_BOTTLE, 6, 1, 2 },
        // Milestone 14:
        { item_id::CANNED_SOUP, 5, 1, 1 }, { item_id::CANNED_PEACHES, 3, 1, 1 },
        { item_id::PASTA, 4, 1, 1 }, { item_id::RICE, 4, 1, 1 }, { item_id::CEREAL, 3, 1, 1 },
        { item_id::COOKIES, 3, 1, 1 }, { item_id::HONEY, 2, 1, 1 }, { item_id::KETTLE, 3, 1, 1 },
        { item_id::CLEAVER, 2, 1, 1 },
    };
    constexpr loot_entry fridge_loot[] = {
        { item_id::SODA, 12, 1, 1 }, { item_id::WATER_BOTTLE, 10, 1, 1 }, { item_id::APPLE, 10, 1, 1 },
        { item_id::BREAD, 6, 1, 1 }, { item_id::RAW_MEAT, 6, 1, 1 }, { item_id::CHOCOLATE, 4, 1, 1 },
        // Milestone 14:
        { item_id::EGGS, 6, 1, 1 }, { item_id::TOMATO, 5, 1, 1 }, { item_id::ONION, 4, 1, 1 },
        { item_id::JUICE, 5, 1, 1 }, { item_id::BEER, 4, 1, 1 },
    };
    // Milestone 14: the new buildings' rooms (items from stage 5 join them).
    constexpr loot_entry school_loot[] = {
        { item_id::SCHOOL_BAG, 8, 1, 1 }, { item_id::CHIPS, 8, 1, 1 }, { item_id::CHOCOLATE, 8, 1, 1 },
        { item_id::SODA, 6, 1, 1 }, { item_id::APPLE, 5, 1, 1 }, { item_id::BOOK_FIRST_AID, 2, 1, 1 },
        { item_id::BOOK_FARMING, 2, 1, 1 }, { item_id::BOOK_COOKING, 2, 1, 1 }, { item_id::BOOK_CARPENTRY, 1, 1, 1 },
        { item_id::DUCT_TAPE, 3, 1, 1 }, { item_id::GLUE, 4, 1, 1 }, { item_id::RAG, 3, 1, 2 },
        { item_id::BATTERY, 3, 1, 2 }, { item_id::HOODIE, 3, 1, 1 }, { item_id::BASEBALL_BAT, 2, 1, 1 },
        { item_id::NOTE_LETTER, 2, 1, 1 },
        // Milestone 14:
        { item_id::COOKIES, 4, 1, 1 }, { item_id::JUICE, 4, 1, 1 }, { item_id::T_SHIRT, 3, 1, 1 },
        { item_id::SNEAKERS, 2, 1, 1 }, { item_id::HOCKEY_STICK, 3, 1, 1 },
    };
    constexpr loot_entry office_loot[] = {
        { item_id::NOTE_LETTER, 6, 1, 1 }, { item_id::HOUSE_KEY, 5, 1, 1 }, { item_id::CAR_KEY, 3, 1, 1 },
        { item_id::MAP, 5, 1, 1 }, { item_id::BATTERY, 6, 1, 2 }, { item_id::FLASHLIGHT, 3, 1, 1 },
        { item_id::DUCT_TAPE, 3, 1, 1 }, { item_id::GLUE, 3, 1, 1 }, { item_id::PAINKILLERS, 3, 1, 1 },
        { item_id::CIGARETTES, 3, 2, 12 }, { item_id::LIGHTER, 2, 1, 1 }, { item_id::RADIO, 1, 1, 1 },
        { item_id::BOOK_MECHANICS, 1, 1, 1 }, { item_id::BOOK_SNEAKING, 1, 1, 1 },
        // Milestone 14:
        { item_id::SHIRT, 2, 1, 1 }, { item_id::SUIT_TROUSERS, 2, 1, 1 },
        { item_id::COLD_PILLS, 2, 1, 1 },
    };
    constexpr loot_entry warehouse_loot[] = {
        { item_id::PLANKS, 10, 2, 4 }, { item_id::NAILS, 10, 10, 30 }, { item_id::ROPE, 6, 1, 1 },
        { item_id::GARBAGE_BAG, 6, 1, 3 }, { item_id::DUCT_TAPE, 5, 1, 1 }, { item_id::CANNED_BEANS, 6, 1, 3 },
        { item_id::CANNED_TUNA, 5, 1, 2 }, { item_id::WATER_BOTTLE, 6, 1, 2 }, { item_id::HAMMER, 3, 1, 1 },
        { item_id::SAW, 2, 1, 1 }, { item_id::CROWBAR, 2, 1, 1 }, { item_id::FUEL_CAN, 2, 1, 1 },
        { item_id::PROPANE, 2, 1, 1 }, { item_id::GENERATOR, 1, 1, 1 }, { item_id::TYRE, 2, 1, 1 },
        { item_id::JAR, 3, 1, 4 },
        // Milestone 14:
        { item_id::SCRAP_METAL, 6, 1, 3 }, { item_id::SHEET_METAL, 4, 1, 1 },
        { item_id::TARP, 5, 1, 1 }, { item_id::WIRE, 5, 1, 3 }, { item_id::ZIP_TIES, 4, 1, 4 },
        { item_id::CAR_JACK, 2, 1, 1 }, { item_id::RADIATOR, 2, 1, 1 }, { item_id::BRAKE_PADS, 3, 1, 1 },
        { item_id::TOOLBOX, 2, 1, 1 }, { item_id::BUCKET, 3, 1, 1 }, { item_id::CLOTH, 4, 1, 3 },
    };
    constexpr loot_entry armory_loot[] = {
        { item_id::ASSAULT_RIFLE, 6, 1, 1 }, { item_id::AMMO_556, 10, 10, 30 }, { item_id::PISTOL, 5, 1, 1 },
        { item_id::AMMO_9MM, 8, 6, 20 }, { item_id::SHOTGUN, 3, 1, 1 }, { item_id::AMMO_SHELLS, 5, 4, 10 },
        { item_id::HUNTING_RIFLE, 2, 1, 1 }, { item_id::AMMO_RIFLE, 4, 4, 10 }, { item_id::HUNTING_KNIFE, 4, 1, 1 },
        { item_id::MILITARY_BAG, 2, 1, 1 }, { item_id::MOLOTOV, 1, 1, 1 },
        // Milestone 14:
        { item_id::REVOLVER, 2, 1, 1 }, { item_id::AMMO_38, 4, 6, 18 }, { item_id::HELMET, 3, 1, 1 },
        { item_id::ARMY_JACKET, 2, 1, 1 }, { item_id::LARGE_BACKPACK, 2, 1, 1 }, { item_id::MACHETE, 2, 1, 1 },
    };
    constexpr loot_entry vending_loot[] = {
        { item_id::SODA, 14, 1, 2 }, { item_id::WATER_BOTTLE, 10, 1, 2 }, { item_id::CHIPS, 10, 1, 2 },
        { item_id::CHOCOLATE, 10, 1, 2 }, { item_id::CRACKERS, 6, 1, 1 },
        // Milestone 14:
        { item_id::COOKIES, 8, 1, 1 }, { item_id::JUICE, 8, 1, 1 },
    };
    constexpr loot_entry fire_station_loot[] = {
        { item_id::AXE, 8, 1, 1 }, { item_id::CROWBAR, 5, 1, 1 }, { item_id::BANDAGE, 8, 1, 3 },
        { item_id::DISINFECTANT, 5, 1, 1 }, { item_id::SPLINT, 4, 1, 1 }, { item_id::FLASHLIGHT, 6, 1, 1 },
        { item_id::BATTERY, 6, 1, 3 }, { item_id::RAINCOAT, 5, 1, 1 }, { item_id::BOOTS, 5, 1, 1 },
        { item_id::GLOVES, 5, 1, 1 }, { item_id::ROPE, 4, 1, 1 }, { item_id::WATER_BOTTLE, 5, 1, 1 },
        { item_id::HOSE, 2, 1, 1 }, { item_id::FUEL_CAN, 2, 1, 1 },
        // Milestone 14:
        { item_id::FIREFIGHTER_JACKET, 6, 1, 1 }, { item_id::HELMET, 3, 1, 1 },
        { item_id::SLEDGEHAMMER, 3, 1, 1 }, { item_id::HATCHET, 4, 1, 1 }, { item_id::FIRST_AID_KIT, 3, 1, 1 },
        { item_id::WORK_GLOVES, 4, 1, 1 }, { item_id::WELLIES, 3, 1, 1 },
    };
    constexpr loot_entry pub_loot[] = {
        { item_id::SODA, 10, 1, 2 }, { item_id::CHIPS, 10, 1, 2 }, { item_id::CRACKERS, 6, 1, 1 },
        { item_id::PEANUT_BUTTER, 3, 1, 1 }, { item_id::EMPTY_BOTTLE, 10, 1, 3 }, { item_id::RAG, 6, 1, 2 },
        { item_id::CIGARETTES, 8, 3, 20 }, { item_id::LIGHTER, 4, 1, 1 }, { item_id::MATCHES, 4, 1, 1 },
        { item_id::KITCHEN_KNIFE, 3, 1, 1 }, { item_id::BASEBALL_BAT, 2, 1, 1 }, { item_id::NOTE_LETTER, 2, 1, 1 },
        // Milestone 14:
        { item_id::BEER, 12, 1, 1 }, { item_id::JUICE, 4, 1, 1 }, { item_id::COOKIES, 3, 1, 1 },
    };
    constexpr loot_entry hunter_loot[] = {
        { item_id::HUNTING_RIFLE, 4, 1, 1 }, { item_id::AMMO_RIFLE, 10, 4, 12 }, { item_id::SHOTGUN, 2, 1, 1 },
        { item_id::AMMO_SHELLS, 6, 3, 8 }, { item_id::HUNTING_KNIFE, 6, 1, 1 }, { item_id::ROPE, 6, 1, 1 },
        { item_id::MAP, 4, 1, 1 }, { item_id::COMPASS, 3, 1, 1 }, { item_id::MAGAZINE_TRAPPING, 3, 1, 1 },
        { item_id::BOOK_AIMING, 2, 1, 1 }, { item_id::BOOK_FORAGING, 2, 1, 1 }, { item_id::HIKING_BAG, 2, 1, 1 },
        { item_id::MATCHES, 4, 1, 1 }, { item_id::LIGHTER, 2, 1, 1 },
        // Milestone 14:
        { item_id::FISHING_ROD, 4, 1, 1 }, { item_id::BINOCULARS, 4, 1, 1 },
        { item_id::MACHETE, 2, 1, 1 }, { item_id::PADDED_JACKET, 2, 1, 1 }, { item_id::CARGO_PANTS, 3, 1, 1 },
        { item_id::CAP, 3, 1, 1 },
    };
    constexpr loot_entry bus_loot[] = {
        { item_id::HOODIE, 8, 1, 1 }, { item_id::JEANS, 6, 1, 1 }, { item_id::BEANIE, 6, 1, 1 },
        { item_id::RAINCOAT, 4, 1, 1 }, { item_id::GLOVES, 3, 1, 1 }, { item_id::SCHOOL_BAG, 5, 1, 1 },
        { item_id::HIKING_BAG, 2, 1, 1 }, { item_id::CHOCOLATE, 6, 1, 1 }, { item_id::CHIPS, 6, 1, 1 },
        { item_id::SODA, 6, 1, 1 }, { item_id::WATER_BOTTLE, 6, 1, 1 }, { item_id::CRACKERS, 4, 1, 1 },
        { item_id::PAINKILLERS, 4, 1, 1 }, { item_id::BANDAGE, 3, 1, 1 }, { item_id::MAP, 4, 1, 1 },
        { item_id::NOTE_LETTER, 3, 1, 1 }, { item_id::CIGARETTES, 4, 3, 20 }, { item_id::LIGHTER, 2, 1, 1 },
        { item_id::BOOK_FIRST_AID, 1, 1, 1 }, { item_id::HOUSE_KEY, 2, 1, 1 },
        // Milestone 14:
        { item_id::T_SHIRT, 4, 1, 1 }, { item_id::SWEATER, 3, 1, 1 }, { item_id::SNEAKERS, 3, 1, 1 },
        { item_id::CAP, 3, 1, 1 }, { item_id::DUFFEL_BAG, 3, 1, 1 }, { item_id::SATCHEL, 3, 1, 1 },
        { item_id::COOKIES, 4, 1, 1 }, { item_id::JUICE, 3, 1, 1 }, { item_id::VITAMINS, 2, 1, 1 },
        { item_id::COLD_PILLS, 2, 1, 1 }, { item_id::GOLF_CLUB, 1, 1, 1 }, { item_id::SEWING_KIT, 1, 1, 1 },
    };
    constexpr loot_entry freezer_loot[] = {
        { item_id::RAW_MEAT, 14, 1, 1 }, { item_id::FISH, 8, 1, 1 }, { item_id::BREAD, 4, 1, 1 },
        { item_id::CARROT, 3, 1, 1 }, { item_id::POTATO, 3, 1, 1 }, { item_id::WATER_BOTTLE, 2, 1, 1 },
    };
    constexpr loot_entry bathroom_loot[] = {
        { item_id::BANDAGE, 12, 1, 2 }, { item_id::PAINKILLERS, 10, 1, 1 }, { item_id::DISINFECTANT, 6, 1, 1 },
        { item_id::RAG, 6, 1, 2 }, { item_id::ANTIBIOTICS, 2, 1, 1 }, { item_id::SUTURE_NEEDLE, 1, 1, 1 },
        // Milestone 14:
        { item_id::VITAMINS, 4, 1, 1 }, { item_id::SLEEPING_PILLS, 3, 1, 1 },
        { item_id::COLD_PILLS, 4, 1, 1 }, { item_id::ALCOHOL_WIPES, 4, 1, 4 },
    };
    constexpr loot_entry bedroom_loot[] = {
        { item_id::ALARM_CLOCK, 6, 1, 1 }, { item_id::NOTE_LETTER, 3, 1, 1 }, { item_id::SHOTGUN, 1, 1, 1 }, { item_id::AMMO_SHELLS, 1, 2, 4 }, { item_id::AMMO_9MM, 1, 3, 8 },
        { item_id::HOODIE, 10, 1, 1 }, { item_id::JEANS, 10, 1, 1 }, { item_id::BEANIE, 8, 1, 1 },
        { item_id::GLOVES, 6, 1, 1 }, { item_id::BOOTS, 5, 1, 1 }, { item_id::RAINCOAT, 4, 1, 1 },
        { item_id::SCHOOL_BAG, 6, 1, 1 }, { item_id::LEATHER_JACKET, 2, 1, 1 }, { item_id::HIKING_BAG, 1, 1, 1 },
        { item_id::BASEBALL_BAT, 3, 1, 1 }, { item_id::CHOCOLATE, 3, 1, 1 }, { item_id::HOUSE_KEY, 2, 1, 1 },
        { item_id::BOOK_SNEAKING, 1, 1, 1 }, { item_id::BOOK_LIGHTFOOT, 1, 1, 1 }, { item_id::BOOK_BLUNT, 1, 1, 1 },
        { item_id::RADIO, 2, 1, 1 },
        { item_id::BOOK_BLADE, 1, 1, 1 }, { item_id::CAR_KEY, 3, 1, 1 },
        { item_id::CIGARETTES, 4, 3, 20 }, { item_id::LIGHTER, 2, 1, 1 },
        // Milestone 14:
        { item_id::T_SHIRT, 8, 1, 1 }, { item_id::SHIRT, 6, 1, 1 }, { item_id::SWEATER, 6, 1, 1 },
        { item_id::PADDED_JACKET, 3, 1, 1 }, { item_id::SUIT_TROUSERS, 3, 1, 1 }, { item_id::CARGO_PANTS, 3, 1, 1 },
        { item_id::SNEAKERS, 5, 1, 1 }, { item_id::CAP, 4, 1, 1 }, { item_id::BANDANA, 3, 1, 1 },
        { item_id::SATCHEL, 3, 1, 1 }, { item_id::DUFFEL_BAG, 2, 1, 1 }, { item_id::SLEEPING_PILLS, 1, 1, 1 },
        { item_id::GOLF_CLUB, 1, 1, 1 }, { item_id::HOCKEY_STICK, 2, 1, 1 }, { item_id::BINOCULARS, 1, 1, 1 },
        { item_id::SEWING_KIT, 3, 1, 1 }, { item_id::CLOTH, 3, 1, 3 },
    };
    constexpr loot_entry garage_loot[] = {
        { item_id::AMMO_SHELLS, 1, 2, 4 }, { item_id::AMMO_RIFLE, 1, 2, 4 },
        { item_id::HAMMER, 8, 1, 1 }, { item_id::SCREWDRIVER, 8, 1, 1 }, { item_id::NAILS, 12, 5, 30 },
        { item_id::HINGE, 4, 1, 2 }, { item_id::DOORKNOB, 4, 1, 1 },
        { item_id::PLANKS, 8, 1, 3 }, { item_id::DUCT_TAPE, 6, 1, 1 }, { item_id::GLUE, 5, 1, 1 },
        { item_id::ROPE, 5, 1, 1 }, { item_id::SAW, 3, 1, 1 }, { item_id::PIPE, 4, 1, 1 },
        { item_id::CROWBAR, 2, 1, 1 }, { item_id::AXE, 2, 1, 1 }, { item_id::FLASHLIGHT, 3, 1, 1 },
        { item_id::BATTERY, 4, 1, 2 }, { item_id::BOOK_MECHANICS, 2, 1, 1 }, { item_id::BOOK_CARPENTRY, 2, 1, 1 },
        { item_id::BOOK_AXE, 1, 1, 1 }, { item_id::PROPANE, 2, 1, 1 }, { item_id::GAS_STOVE, 1, 1, 1 },
        { item_id::MAGAZINE_GENERATOR, 1, 1, 1 }, { item_id::SHOVEL, 3, 1, 1 }, { item_id::WATERING_CAN, 2, 1, 1 },
        { item_id::FUEL_CAN, 2, 1, 1 }, { item_id::GENERATOR, 1, 1, 1 },
        { item_id::WRENCH, 6, 1, 1 }, { item_id::SPARK_PLUG, 4, 1, 1 }, { item_id::TYRE, 3, 1, 1 },
        { item_id::HOSE, 3, 1, 1 }, { item_id::CAR_BATTERY, 2, 1, 1 }, { item_id::WHETSTONE, 3, 1, 1 },
        // Milestone 14:
        { item_id::PLIERS, 6, 1, 1 }, { item_id::WIRE, 6, 1, 3 }, { item_id::SCRAP_METAL, 6, 1, 3 },
        { item_id::ZIP_TIES, 4, 1, 4 }, { item_id::TOOLBOX, 2, 1, 1 }, { item_id::CAR_JACK, 3, 1, 1 },
        { item_id::RADIATOR, 1, 1, 1 }, { item_id::BRAKE_PADS, 3, 1, 1 }, { item_id::BUCKET, 3, 1, 1 },
        { item_id::TARP, 3, 1, 1 }, { item_id::HATCHET, 2, 1, 1 }, { item_id::SLEDGEHAMMER, 1, 1, 1 },
        { item_id::SHEET_METAL, 2, 1, 1 }, { item_id::WORK_GLOVES, 3, 1, 1 },
    };
    constexpr loot_entry shop_loot[] = {
        { item_id::CHIPS, 10, 1, 1 }, { item_id::CHOCOLATE, 10, 1, 1 }, { item_id::SODA, 10, 1, 1 },
        { item_id::WATER_BOTTLE, 8, 1, 1 }, { item_id::CANNED_BEANS, 6, 1, 1 }, { item_id::BATTERY, 6, 1, 4 },
        { item_id::MATCHES, 5, 1, 1 }, { item_id::MAP, 2, 1, 1 }, { item_id::BOOK_COOKING, 1, 1, 1 },
        { item_id::RADIO, 2, 1, 1 },
        { item_id::GARBAGE_BAG, 3, 1, 2 }, { item_id::MAGAZINE_CANNING, 1, 1, 1 }, { item_id::SEEDS_CARROT, 2, 1, 4 },
        { item_id::SEEDS_POTATO, 2, 1, 4 },
        { item_id::MAGAZINE_TRAPPING, 1, 1, 1 }, { item_id::MAGAZINE_GENERATOR, 1, 1, 1 },
        { item_id::PROPANE, 1, 1, 1 }, { item_id::CIGARETTES, 6, 20, 20 }, { item_id::LIGHTER, 3, 1, 1 },
        { item_id::BOOK_FARMING, 1, 1, 1 }, { item_id::BOOK_FORAGING, 1, 1, 1 }, { item_id::BOOK_CARPENTRY, 1, 1, 1 },
        // Milestone 14:
        { item_id::CANNED_SOUP, 6, 1, 1 }, { item_id::CANNED_PEACHES, 5, 1, 1 },
        { item_id::PASTA, 4, 1, 1 }, { item_id::RICE, 4, 1, 1 }, { item_id::CEREAL, 4, 1, 1 },
        { item_id::COOKIES, 5, 1, 1 }, { item_id::JUICE, 5, 1, 1 }, { item_id::BEER, 3, 1, 1 },
        { item_id::HONEY, 2, 1, 1 }, { item_id::SEEDS_TOMATO, 2, 1, 3 }, { item_id::SEEDS_ONION, 2, 1, 3 },
        { item_id::ZIP_TIES, 2, 1, 4 },
    };
    constexpr loot_entry police_loot[] = {
        { item_id::PISTOL, 5, 1, 1 }, { item_id::AMMO_9MM, 8, 6, 20 }, { item_id::SHOTGUN, 1, 1, 1 },
        { item_id::NOTE_LETTER, 3, 1, 1 },
        { item_id::AMMO_SHELLS, 2, 2, 6 },
        { item_id::BANDAGE, 8, 1, 2 }, { item_id::FLASHLIGHT, 8, 1, 1 }, { item_id::BATTERY, 6, 1, 2 },
        { item_id::BOOTS, 5, 1, 1 }, { item_id::GLOVES, 5, 1, 1 }, { item_id::LEATHER_JACKET, 3, 1, 1 },
        { item_id::MAP, 3, 1, 1 }, { item_id::COMPASS, 2, 1, 1 }, { item_id::MILITARY_BAG, 1, 1, 1 },
        { item_id::BOOK_AIMING, 2, 1, 1 }, { item_id::BOOK_RELOADING, 2, 1, 1 }, { item_id::BOOK_BLUNT, 1, 1, 1 },
        // Milestone 14:
        { item_id::POLICE_BATON, 6, 1, 1 }, { item_id::POLICE_JACKET, 4, 1, 1 },
        { item_id::REVOLVER, 2, 1, 1 }, { item_id::AMMO_38, 5, 6, 18 }, { item_id::LOCKPICK, 2, 1, 1 },
        { item_id::BINOCULARS, 2, 1, 1 }, { item_id::HELMET, 1, 1, 1 }, { item_id::ZIP_TIES, 4, 1, 4 },
    };
    constexpr loot_entry car_trunk_loot[] = {
        { item_id::WATER_BOTTLE, 6, 1, 1 }, { item_id::ROPE, 4, 1, 1 }, { item_id::DUCT_TAPE, 4, 1, 1 },
        { item_id::FLASHLIGHT, 4, 1, 1 }, { item_id::MAP, 4, 1, 1 }, { item_id::SCHOOL_BAG, 3, 1, 1 },
        { item_id::CROWBAR, 2, 1, 1 }, { item_id::WRENCH, 4, 1, 1 }, { item_id::SPARK_PLUG, 3, 1, 1 },
        { item_id::HOSE, 3, 1, 1 }, { item_id::FUEL_CAN, 2, 1, 1 }, { item_id::TYRE, 2, 1, 1 },
        // Milestone 14:
        { item_id::CAR_JACK, 5, 1, 1 }, { item_id::TARP, 3, 1, 1 }, { item_id::BRAKE_PADS, 2, 1, 1 },
        { item_id::TOOLBOX, 2, 1, 1 }, { item_id::FISHING_ROD, 2, 1, 1 }, { item_id::HOCKEY_STICK, 2, 1, 1 },
        { item_id::GOLF_CLUB, 3, 1, 1 }, { item_id::DUFFEL_BAG, 2, 1, 1 },
    };
    constexpr loot_entry glove_box_loot[] = {
        { item_id::MAP, 6, 1, 1 }, { item_id::FLASHLIGHT, 4, 1, 1 }, { item_id::BATTERY, 4, 1, 2 },
        { item_id::PAINKILLERS, 3, 1, 1 }, { item_id::CHOCOLATE, 3, 1, 1 }, { item_id::MATCHES, 3, 1, 1 },
        { item_id::CAR_KEY, 4, 1, 1 }, { item_id::HOUSE_KEY, 2, 1, 1 }, { item_id::SCREWDRIVER, 2, 1, 1 },
        // Milestone 14:
        { item_id::BINOCULARS, 1, 1, 1 }, { item_id::COLD_PILLS, 2, 1, 1 },
    };
    constexpr loot_entry medicine_loot[] = {
        { item_id::BANDAGE, 12, 1, 3 }, { item_id::PAINKILLERS, 10, 1, 1 }, { item_id::DISINFECTANT, 8, 1, 1 },
        { item_id::ANTIBIOTICS, 5, 1, 1 }, { item_id::SPLINT, 4, 1, 1 }, { item_id::RAG, 4, 1, 2 },
        { item_id::BOOK_FIRST_AID, 2, 1, 1 }, { item_id::SUTURE_NEEDLE, 5, 1, 2 },
        // Milestone 14:
        { item_id::STERILE_BANDAGE, 8, 1, 2 }, { item_id::ALCOHOL_WIPES, 8, 1, 4 },
        { item_id::VITAMINS, 5, 1, 1 }, { item_id::SLEEPING_PILLS, 4, 1, 1 }, { item_id::COLD_PILLS, 6, 1, 1 },
        { item_id::FIRST_AID_KIT, 2, 1, 1 },
    };
    constexpr loot_entry counter_loot[] = {
        { item_id::CHIPS, 8, 1, 1 }, { item_id::CHOCOLATE, 8, 1, 1 }, { item_id::SODA, 6, 1, 1 },
        { item_id::EMPTY_BOTTLE, 3, 1, 2 }, { item_id::NOTE_LETTER, 2, 1, 1 },
        { item_id::BATTERY, 6, 1, 3 }, { item_id::MATCHES, 6, 1, 1 }, { item_id::MAP, 4, 1, 1 },
        { item_id::CIGARETTES, 8, 5, 20 }, { item_id::LIGHTER, 4, 1, 1 },
        { item_id::FLASHLIGHT, 2, 1, 1 }, { item_id::HOUSE_KEY, 2, 1, 1 }, { item_id::GARBAGE_BAG, 2, 1, 1 },
        { item_id::SPARK_PLUG, 3, 1, 1 }, { item_id::HOSE, 2, 1, 1 }, { item_id::CAR_KEY, 1, 1, 1 },
        // Milestone 14:
        { item_id::COOKIES, 3, 1, 1 }, { item_id::BEER, 2, 1, 1 },
    };
    constexpr loot_entry crate_loot[] = {
        { item_id::HUNTING_RIFLE, 1, 1, 1 }, { item_id::AMMO_RIFLE, 2, 2, 6 }, { item_id::AMMO_SHELLS, 2, 2, 6 },
        { item_id::PLANKS, 8, 1, 3 }, { item_id::NAILS, 8, 5, 20 }, { item_id::ROPE, 6, 1, 1 },
        { item_id::APPLE, 8, 1, 1 }, { item_id::WATER_BOTTLE, 4, 1, 1 }, { item_id::DUCT_TAPE, 4, 1, 1 },
        { item_id::HAMMER, 3, 1, 1 }, { item_id::SAW, 2, 1, 1 }, { item_id::AXE, 1, 1, 1 },
        { item_id::BOOK_FARMING, 2, 1, 1 }, { item_id::BOOK_FORAGING, 1, 1, 1 }, { item_id::BOOK_AXE, 1, 1, 1 },
        { item_id::BOOK_SPEAR, 1, 1, 1 }, { item_id::STICKS, 5, 2, 6 }, { item_id::STONES, 4, 2, 5 },
        { item_id::JAR, 4, 1, 4 }, { item_id::MAGAZINE_CANNING, 1, 1, 1 }, { item_id::MAGAZINE_TRAPPING, 1, 1, 1 },
        { item_id::SEEDS_CARROT, 4, 2, 6 }, { item_id::SEEDS_POTATO, 4, 2, 6 }, { item_id::SEEDS_CABBAGE, 2, 1, 4 },
        { item_id::SHOVEL, 2, 1, 1 }, { item_id::WATERING_CAN, 3, 1, 1 }, { item_id::FUEL_CAN, 1, 1, 1 },
        { item_id::GENERATOR, 1, 1, 1 }, { item_id::TYRE, 2, 1, 1 }, { item_id::HOSE, 2, 1, 1 },
        { item_id::WRENCH, 2, 1, 1 }, { item_id::WHETSTONE, 2, 1, 1 },
        // Milestone 14:
        { item_id::PITCHFORK, 5, 1, 1 }, { item_id::OVERALLS, 4, 1, 1 },
        { item_id::WELLIES, 4, 1, 1 }, { item_id::WORK_GLOVES, 4, 1, 1 }, { item_id::SEEDS_TOMATO, 4, 1, 3 },
        { item_id::SEEDS_ONION, 4, 1, 3 }, { item_id::BUCKET, 4, 1, 1 }, { item_id::TARP, 4, 1, 1 },
        { item_id::WIRE, 3, 1, 3 }, { item_id::HATCHET, 3, 1, 1 }, { item_id::MACHETE, 1, 1, 1 },
        { item_id::EGGS, 3, 1, 1 }, { item_id::SCRAP_METAL, 3, 1, 3 },
    };
    constexpr loot_entry desk_loot[] = {
        { item_id::NOTE_LETTER, 4, 1, 1 },
        { item_id::CAR_KEY, 2, 1, 1 },
        { item_id::MAP, 5, 1, 1 }, { item_id::BATTERY, 5, 1, 2 }, { item_id::PAINKILLERS, 4, 1, 1 },
        { item_id::HOUSE_KEY, 4, 1, 1 }, { item_id::CHOCOLATE, 4, 1, 1 }, { item_id::FLASHLIGHT, 3, 1, 1 },
        { item_id::COMPASS, 2, 1, 1 }, { item_id::DUCT_TAPE, 2, 1, 1 }, { item_id::BOOK_FIRST_AID, 2, 1, 1 },
        { item_id::BOOK_AIMING, 1, 1, 1 }, { item_id::BOOK_SNEAKING, 1, 1, 1 }, { item_id::MAGAZINE_CANNING, 1, 1, 1 },
        { item_id::MAGAZINE_GENERATOR, 1, 1, 1 },
        // Milestone 14:
        { item_id::BINOCULARS, 1, 1, 1 }, { item_id::COLD_PILLS, 1, 1, 1 },
    };
    constexpr loot_entry camp_loot[] = {
        { item_id::MATCHES, 8, 1, 1 }, { item_id::WATER_BOTTLE, 8, 1, 1 }, { item_id::CANNED_BEANS, 6, 1, 1 },
        { item_id::CRACKERS, 5, 1, 1 }, { item_id::ROPE, 5, 1, 1 }, { item_id::RAG, 4, 1, 2 },
        { item_id::FLASHLIGHT, 4, 1, 1 }, { item_id::SCHOOL_BAG, 4, 1, 1 }, { item_id::HIKING_BAG, 3, 1, 1 },
        { item_id::HUNTING_KNIFE, 2, 1, 1 }, { item_id::MAP, 2, 1, 1 }, { item_id::BOOK_FORAGING, 1, 1, 1 },
        { item_id::SPEAR, 2, 1, 1 }, { item_id::EMPTY_BOTTLE, 5, 1, 3 },
        { item_id::BOOK_SPEAR, 1, 1, 1 }, { item_id::STICKS, 6, 2, 6 }, { item_id::STONES, 4, 2, 5 },
        { item_id::COOKING_POT, 3, 1, 1 }, { item_id::GAS_STOVE, 2, 1, 1 }, { item_id::PROPANE, 2, 1, 1 },
        { item_id::MAGAZINE_TRAPPING, 1, 1, 1 },
        // Milestone 14:
        { item_id::FISHING_ROD, 3, 1, 1 }, { item_id::TARP, 3, 1, 1 }, { item_id::MACHETE, 1, 1, 1 },
        { item_id::BINOCULARS, 1, 1, 1 }, { item_id::CANNED_SOUP, 3, 1, 1 }, { item_id::BANDANA, 2, 1, 1 },
        { item_id::SATCHEL, 2, 1, 1 }, { item_id::CLOTH, 2, 1, 3 }, { item_id::LARGE_BACKPACK, 1, 1, 1 },
    };
    constexpr loot_entry corpse_loot[] = {
        { item_id::CHOCOLATE, 6, 1, 1 }, { item_id::SODA, 4, 1, 1 }, { item_id::BANDAGE, 4, 1, 1 },
        { item_id::PAINKILLERS, 3, 1, 1 }, { item_id::MATCHES, 4, 1, 1 }, { item_id::BATTERY, 3, 1, 1 },
        { item_id::HOUSE_KEY, 3, 1, 1 }, { item_id::KITCHEN_KNIFE, 1, 1, 1 }, { item_id::MAP, 1, 1, 1 },
        { item_id::CAR_KEY, 2, 1, 1 },
        // Milestone 14:
        { item_id::ALCOHOL_WIPES, 2, 1, 4 }, { item_id::COOKIES, 2, 1, 1 },
    };
    constexpr loot_entry military_loot[] = {
        { item_id::ASSAULT_RIFLE, 2, 1, 1 }, { item_id::AMMO_556, 4, 5, 20 }, { item_id::PISTOL, 2, 1, 1 },
        { item_id::AMMO_9MM, 3, 5, 15 },
        { item_id::CANNED_BEANS, 8, 1, 2 }, { item_id::WATER_BOTTLE, 8, 1, 2 }, { item_id::BANDAGE, 8, 1, 3 },
        { item_id::ANTIBIOTICS, 4, 1, 1 }, { item_id::SPLINT, 3, 1, 1 }, { item_id::HUNTING_KNIFE, 4, 1, 1 },
        { item_id::AXE, 2, 1, 1 }, { item_id::COMPASS, 4, 1, 1 }, { item_id::MAP, 4, 1, 1 },
        { item_id::MILITARY_BAG, 3, 1, 1 }, { item_id::FLASHLIGHT, 4, 1, 1 }, { item_id::BATTERY, 5, 1, 3 },
        { item_id::RADIO, 3, 1, 1 }, { item_id::BOOTS, 3, 1, 1 }, { item_id::FUEL_CAN, 3, 1, 1 },
        { item_id::GENERATOR, 2, 1, 1 },
        // Milestone 14:
        { item_id::ARMY_JACKET, 4, 1, 1 }, { item_id::ARMY_BOOTS, 4, 1, 1 },
        { item_id::HELMET, 3, 1, 1 }, { item_id::LARGE_BACKPACK, 1, 1, 1 }, { item_id::FIRST_AID_KIT, 3, 1, 1 },
        { item_id::MACHETE, 2, 1, 1 }, { item_id::BINOCULARS, 2, 1, 1 }, { item_id::STERILE_BANDAGE, 4, 1, 2 },
        { item_id::CARGO_PANTS, 2, 1, 1 },
    };
    constexpr loot_entry forage_forest_loot[] = {
        { item_id::BERRIES, 10, 1, 1 }, { item_id::MUSHROOMS, 8, 1, 1 }, { item_id::PALE_MUSHROOMS, 4, 1, 1 },
        { item_id::HERBS, 6, 1, 1 }, { item_id::STICKS, 10, 1, 4 }, { item_id::STONES, 4, 1, 3 },
    };
    constexpr loot_entry forage_field_loot[] = {
        { item_id::BERRIES, 8, 1, 1 }, { item_id::HERBS, 8, 1, 1 }, { item_id::STONES, 8, 1, 3 },
        { item_id::STICKS, 4, 1, 3 }, { item_id::APPLE, 3, 1, 1 },
    };
    constexpr loot_entry forage_water_loot[] = {
        { item_id::FISH, 8, 1, 1 }, { item_id::STONES, 6, 1, 3 }, { item_id::HERBS, 3, 1, 1 },
        { item_id::STICKS, 3, 1, 2 },
    };
    constexpr loot_entry police_corpse_loot[] = {
        { item_id::PISTOL, 2, 1, 1 }, { item_id::AMMO_9MM, 4, 3, 10 },
        { item_id::FLASHLIGHT, 8, 1, 1 }, { item_id::BATTERY, 6, 1, 2 }, { item_id::BANDAGE, 5, 1, 1 },
        { item_id::HOUSE_KEY, 3, 1, 1 }, { item_id::GLOVES, 3, 1, 1 }, { item_id::BOOTS, 2, 1, 1 },
        { item_id::MAP, 2, 1, 1 }, { item_id::CHOCOLATE, 3, 1, 1 },
        // Milestone 14:
        { item_id::POLICE_BATON, 3, 1, 1 }, { item_id::AMMO_38, 2, 6, 18 },
    };
    constexpr loot_entry footlocker_loot[] = {
        { item_id::ARMY_JACKET, 6, 1, 1 }, { item_id::ARMY_BOOTS, 6, 1, 1 }, { item_id::CARGO_PANTS, 6, 1, 1 },
        { item_id::T_SHIRT, 6, 1, 1 }, { item_id::HELMET, 2, 1, 1 }, { item_id::CANNED_BEANS, 6, 1, 1 },
        { item_id::WATER_BOTTLE, 6, 1, 1 }, { item_id::BANDAGE, 6, 1, 2 }, { item_id::STERILE_BANDAGE, 3, 1, 1 },
        { item_id::HUNTING_KNIFE, 3, 1, 1 }, { item_id::COMPASS, 2, 1, 1 }, { item_id::MAP, 2, 1, 1 },
        { item_id::CIGARETTES, 5, 3, 20 }, { item_id::LIGHTER, 2, 1, 1 }, { item_id::FLASHLIGHT, 3, 1, 1 },
        { item_id::AMMO_9MM, 4, 6, 15 }, { item_id::AMMO_556, 3, 5, 15 }, { item_id::PISTOL, 1, 1, 1 },
        { item_id::MILITARY_BAG, 1, 1, 1 }, { item_id::NOTE_LETTER, 2, 1, 1 },
    };
    constexpr loot_entry soldier_corpse_loot[] = {
        { item_id::ASSAULT_RIFLE, 1, 1, 1 }, { item_id::AMMO_556, 2, 3, 10 },
        { item_id::CANNED_BEANS, 6, 1, 1 }, { item_id::WATER_BOTTLE, 6, 1, 1 }, { item_id::BANDAGE, 6, 1, 2 },
        { item_id::HUNTING_KNIFE, 3, 1, 1 }, { item_id::COMPASS, 3, 1, 1 }, { item_id::MATCHES, 4, 1, 1 },
        { item_id::BOOTS, 3, 1, 1 }, { item_id::MAP, 2, 1, 1 }, { item_id::MILITARY_BAG, 1, 1, 1 },
        // Milestone 14:
        { item_id::ARMY_BOOTS, 2, 1, 1 }, { item_id::HELMET, 1, 1, 1 },
    };
    constexpr loot_entry doctor_corpse_loot[] = {
        { item_id::BANDAGE, 10, 1, 2 }, { item_id::PAINKILLERS, 8, 1, 1 }, { item_id::DISINFECTANT, 6, 1, 1 },
        { item_id::ANTIBIOTICS, 3, 1, 1 }, { item_id::SPLINT, 2, 1, 1 }, { item_id::GLOVES, 3, 1, 1 },
        { item_id::SUTURE_NEEDLE, 3, 1, 1 },
        // Milestone 14:
        { item_id::STERILE_BANDAGE, 3, 1, 2 }, { item_id::ALCOHOL_WIPES, 3, 1, 4 },
        { item_id::VITAMINS, 2, 1, 1 },
    };
    // Milestone 12.7: what a gun safe holds; a stash in the woods.
    constexpr loot_entry gun_safe_loot[] = {
        { item_id::PISTOL, 8, 1, 1 }, { item_id::AMMO_9MM, 10, 10, 30 }, { item_id::SHOTGUN, 4, 1, 1 },
        { item_id::AMMO_SHELLS, 6, 4, 12 }, { item_id::HUNTING_RIFLE, 3, 1, 1 }, { item_id::AMMO_RIFLE, 4, 3, 10 },
        { item_id::HOUSE_KEY, 2, 1, 1 }, { item_id::NOTE_LETTER, 2, 1, 1 },
    };
    constexpr loot_entry stash_loot[] = {
        { item_id::CANNED_BEANS, 8, 1, 1 }, { item_id::CANNED_TUNA, 8, 1, 1 }, { item_id::WATER_BOTTLE, 8, 1, 1 },
        { item_id::BANDAGE, 6, 1, 3 }, { item_id::ANTIBIOTICS, 3, 1, 1 }, { item_id::AMMO_9MM, 4, 6, 15 },
        { item_id::AMMO_SHELLS, 3, 2, 6 }, { item_id::HUNTING_KNIFE, 3, 1, 1 }, { item_id::MATCHES, 4, 1, 1 },
        { item_id::BATTERY, 4, 1, 3 }, { item_id::MOLOTOV, 2, 1, 1 }, { item_id::HIKING_BAG, 2, 1, 1 },
    };
    constexpr loot_entry farmer_corpse_loot[] = {
        { item_id::APPLE, 8, 1, 2 }, { item_id::MATCHES, 6, 1, 1 }, { item_id::ROPE, 5, 1, 1 },
        { item_id::GLOVES, 5, 1, 1 }, { item_id::HAMMER, 2, 1, 1 }, { item_id::RAG, 4, 1, 2 },
        { item_id::NAILS, 3, 3, 10 }, { item_id::RAW_MEAT, 2, 1, 1 },
        // Milestone 14:
        { item_id::SEEDS_TOMATO, 2, 1, 3 }, { item_id::WORK_GLOVES, 2, 1, 1 },
    };
}

constexpr loot_table_info loot_table_infos[] = {
    { "KITCHEN CABINET", "CABINET", 0, 4, detail::kitchen_loot },   // milestone 14 census: 1-5 before
    { "FRIDGE", "FRIDGE", 0, 3, detail::fridge_loot },
    { "BATHROOM CABINET", "CABINET", 1, 3, detail::bathroom_loot },
    { "WARDROBE", "WARDROBE", 1, 4, detail::bedroom_loot },
    { "TOOL SHELF", "SHELF", 1, 5, detail::garage_loot },
    { "SHOP SHELF", "SHELF", 2, 6, detail::shop_loot },
    { "POLICE LOCKER", "LOCKER", 1, 4, detail::police_loot },
    { "CAR TRUNK", "TRUNK", 0, 4, detail::car_trunk_loot },
    { "ZOMBIE", "CORPSE", 0, 2, detail::corpse_loot },
    { "MEDICINE CABINET", "CABINET", 1, 4, detail::medicine_loot },
    { "COUNTER", "COUNTER", 1, 4, detail::counter_loot },
    { "CRATE", "CRATE", 1, 4, detail::crate_loot },
    { "DESK", "DESK", 0, 3, detail::desk_loot },
    { "TENT", "TENT", 1, 4, detail::camp_loot },
    { "DEAD SURVIVOR", "BODY", 1, 3, detail::corpse_loot },
    { "MILITARY CRATE", "CRATE", 3, 7, detail::military_loot },
    { "FORAGING", "GROUND", 0, 3, detail::forage_forest_loot },
    { "FORAGING", "GROUND", 0, 2, detail::forage_field_loot },
    { "FORAGING", "WATERSIDE", 0, 2, detail::forage_water_loot },
    { "POLICE ZOMBIE", "CORPSE", 0, 2, detail::police_corpse_loot },
    { "SOLDIER ZOMBIE", "CORPSE", 0, 3, detail::soldier_corpse_loot },
    { "DOCTOR ZOMBIE", "CORPSE", 0, 2, detail::doctor_corpse_loot },
    { "FARMER ZOMBIE", "CORPSE", 0, 2, detail::farmer_corpse_loot },
    { "GLOVE BOX", "GLOVE BOX", 0, 3, detail::glove_box_loot },
    { "FLOOR", "FLOOR", 0, 0, bn::span<const loot_entry>() },
    { "GUN SAFE", "SAFE", 2, 4, detail::gun_safe_loot },
    { "STASH", "STASH", 4, 7, detail::stash_loot },
    { "CHEST FREEZER", "FREEZER", 1, 4, detail::freezer_loot },
    { "SCHOOL DESK", "DESK", 0, 2, detail::school_loot },
    { "FILING CABINET", "CABINET", 1, 3, detail::office_loot },
    { "WAREHOUSE RACK", "RACK", 1, 5, detail::warehouse_loot },
    { "GUN RACK", "RACK", 1, 3, detail::armory_loot },
    { "VENDING MACHINE", "MACHINE", 1, 4, detail::vending_loot },
    { "FIRE STATION LOCKER", "LOCKER", 1, 4, detail::fire_station_loot },
    { "BAR", "BAR", 1, 4, detail::pub_loot },
    { "HUNTER'S GUN RACK", "RACK", 1, 3, detail::hunter_loot },
    { "COACH LUGGAGE", "LUGGAGE", 1, 4, detail::bus_loot },
    { "HUNTER STAND", "STAND", 0, 2, detail::hunter_loot },
    { "FOOTLOCKER", "LOCKER", 1, 3, detail::footlocker_loot },
};

static_assert(sizeof(loot_table_infos) / sizeof(loot_table_info) == int(loot_table::COUNT));

[[nodiscard]] constexpr const loot_table_info& info(loot_table table)
{
    return loot_table_infos[int(table)];
}

// ---------------------------------------------------------------------------------------------------------------------
// Inventory and searching tuning.
// ---------------------------------------------------------------------------------------------------------------------

enum class inventory_param : int
{
    BASE_CAPACITY,
    HEAVY_MULT,
    OVERLOAD_MULT,
    SEARCH_START,
    SEARCH_PER_ITEM,
    LOOT_PERCENT,
    INTERACT_REACH,
    PROMPT_SAFE_PX,
    FIGHT_LOCK,
    COUNT
};

constexpr tuning_entry inventory_tuning_defaults[] = {
    tune_int("Base capacity kg", 8, 1, 40),                     // spec: 8 kg + Strength bonus + bag
    tune_milli("Heavy load speed x", 850, 300, 1000, 50),       // over capacity
    tune_milli("Overload speed x", 650, 200, 1000, 50),         // over 1.5x capacity
    tune_int("Search start frames", 30, 0, 240, 5),             // rummaging before the first item
    tune_int("Frames per item", 20, 0, 240, 5),                 // items appear one at a time
    tune_int("Loot amount %", 100, 0, 400, 10),                 // zone tier multiplier stand-in
    tune_int("Interact reach px", 12, 4, 32),
    tune_int("Search safe dist px", 40, 0, 160, 4),             // A attacks instead if a zombie is closer
    tune_int("Search lock frames", 60, 0, 240, 5),              // after a swing or stomp, A keeps attacking
};

static_assert(sizeof(inventory_tuning_defaults) / sizeof(tuning_entry) == int(inventory_param::COUNT));

[[nodiscard]] tuning_set& inventory_tuning();

// Loot by zone (milestone 13; spec: multiplied by the zone tier): the item count of a container in percent, by
// where it is, and weapons and ammunition weighted up on military bases. The test world (benches) keeps 100%.
namespace zone_loot
{
    constexpr int village_percent = 80;         // town tier 1
    constexpr int town_percent = 100;           // tier 2
    constexpr int city_percent = 120;           // tier 3: a town of 2x2 cells
    constexpr int farm_percent = 100;
    constexpr int military_percent = 100;
    constexpr int military_weapon_weight = 3;   // weapons and ammunition weigh this much more
    constexpr int wild_percent = 50;            // wilderness and coast
}

[[nodiscard]] inline bn::fixed tuning(inventory_param param)
{
    return inventory_tuning().get(int(param));
}

}

#endif
