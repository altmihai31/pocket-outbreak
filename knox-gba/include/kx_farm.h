#ifndef KX_FARM_H
#define KX_FARM_H

#include "kx_items.h"
#include "kx_world_gfx_ids.h"

namespace kx
{

class game;

namespace save
{
    class reader;
    class writer;
}

// Farming (spec: Base building, farming and cooking): plough a plot with a shovel, plant seeds, water it. Crops
// grow through 4 stages over 10-30 in-game days; rain waters them; neglected plots rot; winter stops growth.
// 32 plots per world. Timed state is lazy: a plot stores when it was last worked out and when it was last watered,
// and the days since are replayed (with each day's rain from the weather seed) when it is looked at.
enum class crop : uint8_t
{
    NONE,
    CARROT,
    POTATO,
    CABBAGE,
    TOMATO,         // milestone 14
    ONION,
    COUNT
};

static_assert(int(crop::COUNT) <= 8);   // 3 bits in a plot

constexpr int crop_count = int(crop::COUNT);

struct crop_info
{
    const char* name;           // prompt: "[A] PLANT CARROTS"
    item_id seeds;
    item_id harvest;
    uint8_t days;               // growing days to ripe
    uint8_t yield_min;
    uint8_t yield_max;
};

constexpr crop_info crop_infos[crop_count] = {
    { "", item_id::NONE, item_id::NONE, 0, 0, 0 },
    { "CARROTS", item_id::SEEDS_CARROT, item_id::CARROT, 10, 3, 5 },
    { "POTATOES", item_id::SEEDS_POTATO, item_id::POTATO, 20, 4, 6 },
    { "CABBAGES", item_id::SEEDS_CABBAGE, item_id::CABBAGE, 30, 2, 4 },
    { "TOMATOES", item_id::SEEDS_TOMATO, item_id::TOMATO, 14, 3, 5 },
    { "ONIONS", item_id::SEEDS_ONION, item_id::ONION, 18, 3, 5 },
};

[[nodiscard]] constexpr const crop_info& info(crop value)
{
    return crop_infos[int(value)];
}

// The 4 stages, plus an empty plot and a rotten one.
enum class plot_stage : uint8_t
{
    EMPTY,
    SEEDED,
    SPROUT,
    GROWING,
    RIPE,
    ROTTEN
};

namespace farm_tuning
{
    constexpr int max_plots = 32;
    constexpr int wet_days = 2;                 // a watering (or a rainy day) keeps the soil wet: that day and the next
    constexpr int rot_dry_days = 6;             // days without water before a growing crop rots
    constexpr int ripe_keep_days = 10;          // a ripe crop left this long rots
    constexpr int update_tiles = 24;            // plots this close to the player are worked out once a second

    constexpr int plant_xp = 4;                 // Farming XP
    constexpr int water_xp = 2;
    constexpr int harvest_xp = 15;
    constexpr int levels_per_extra = 2;         // one more crop per this many Farming levels
    constexpr int seed_percent = 40;            // a harvest gives a seed packet back
    constexpr int seed_percent_per_level = 10;
}

namespace farm
{
    // A new world: no plots.
    void reset();

    // Plots ploughed (of 32 per world).
    [[nodiscard]] int count();

    // Ploughs a plot (its tile graphic is the caller's).
    bool add(int tile_x, int tile_y, int today);

    // The plot on a tile, or -1.
    [[nodiscard]] int at(int tile_x, int tile_y);

    // A plot's stage today (works out the days since it was last looked at).
    [[nodiscard]] plot_stage stage(int index, int today);

    [[nodiscard]] crop crop_of(int index);

    // Watered (or rained on) today.
    [[nodiscard]] bool wet(int index, int today);

    void plant(int index, crop value, int today);
    void water(int index, int today);

    // Ripe or rotten: the plot is empty again (the crops are the caller's).
    void clear(int index, int today);

    // The tile graphic for a stage.
    [[nodiscard]] world_gfx gfx(plot_stage value);

    // Once a second: plots near the player are worked out and their tiles follow their stage.
    void update(game& owner);

    void save(save::writer& out);
    void load(save::reader& in);
}

}

#endif
