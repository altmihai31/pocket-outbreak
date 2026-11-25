#ifndef KX_CONTAINERS_H
#define KX_CONTAINERS_H

#include "bn_array.h"
#include "bn_vector.h"

#include "kx_loot.h"

namespace kx
{

namespace save
{
    class reader;
    class writer;
}

enum class container_kind : uint8_t
{
    FURNITURE,      // placed by the map; rolled on first open
    CORPSE,         // a killed zombie; rolled on first open
    FLOOR,          // items the player dropped
    SURVIVOR,       // a past survivor's gear, carried by their zombie (off the map until it is put down)
    CRATE,          // built by the player (kx_base): storage, never replaced
    FORAGE,         // a patch of ground being foraged: rolled per day, gone when the search closes
    VEHICLE         // a car's trunk or glove box (kx_vehicles): off the map, rolled on first open
};

// Every container the player has met: 8 bytes, in RAM and in the save.
struct container_record
{
    static constexpr uint8_t no_page = 0xFF;

    int16_t tile_x = 0;
    int16_t tile_y = 0;
    loot_table table = loot_table::FLOOR;
    container_kind kind = container_kind::FLOOR;
    bool opened : 1 = false;                    // spec: opening sets the container's opened bit
    uint8_t body : 2 = 0;                       // playtest 2: a zombie corpse's body (kx_sprite_frames.h); in the
                                                // opened byte's spare bits, so saves from before read as body 0
    uint8_t page = no_page;                     // no_page, or 0 when it holds a page (container_store has which)
};

static_assert(sizeof(container_record) == 8);

// What a container holds (a page), with its record's fields copied in.
struct container
{
    static constexpr int capacity = 12;         // 4x3 grid (mockup 03)

    bn::array<item_stack, capacity> items = {};
    int16_t tile_x = 0;
    int16_t tile_y = 0;
    loot_table table = loot_table::FLOOR;
    container_kind kind = container_kind::FLOOR;
    bool opened = false;

    [[nodiscard]] bool empty() const;

    // Adds into matching stacks, then free slots; returns false if something did not fit (left in `stack`).
    bool add(item_stack& stack);
};

// Every container in the world and what is in it: the world delta (spec: Containers and loot tables; Save
// system). Containers are small records (position, loot table, opened bit); only those holding items own one of
// the item pages, 12 slots each. A page is attached when a container is looked into and given back once empty,
// so the pages go to the containers that still hold something. Unopened containers cost nothing beyond their
// record, and unopened furniture not even that for long: it is registered again with the same loot when next
// faced. Milestone 14: 512 pages (they were 168), and the save's room is a budget of item stacks over all of them
// (the save writes only the filled slots), so many containers holding a few things each fit.
class container_store
{

public:
    static constexpr int max_containers = 1024;
    static constexpr int max_pages = 512;
    static constexpr int max_stacks = 1840;     // item stacks held in all of them (spec: up to 2,000 world items)

    // Worst case in the save: every record, a slot mask for every page, the stack budget (the same size as the
    // 168 full pages before milestone 14).
    static constexpr int max_save_bytes = 4 + (max_containers * int(sizeof(container_record))) + (max_pages * 2) +
                                          (max_stacks * int(sizeof(item_stack)));

    void init();

    // Container on a tile, or -1.
    [[nodiscard]] int at(int tile_x, int tile_y) const;

    // The furniture container on a tile (registered the first time it is faced, since chunks are generated on
    // demand), or -1 if the tile has none.
    int furniture(int tile_x, int tile_y);

    // Adds a container. When the store is full it replaces unopened furniture, then an emptied corpse or floor
    // pile; returns -1 if there is no room at all.
    int add(container_kind kind, loot_table table, int tile_x, int tile_y);

    // A zombie corpse's body (playtest 2: its corpse sprite wears it).
    void set_body(int index, int body)
    {
        _records[index].body = uint8_t(body);
    }

    // Moves a container (a past survivor's gear onto their body).
    void relocate(int index, int tile_x, int tile_y, container_kind kind, loot_table table);

    // Position, kind and opened bit, without touching the pages.
    [[nodiscard]] const container_record& record(int index) const
    {
        return _records[index];
    }

    // The container with its items (attaching a page if it has none). The reference stays valid until a
    // different container is asked for.
    [[nodiscard]] container& get(int index);

    [[nodiscard]] int count() const
    {
        return _records.size();
    }

    // Rolls the contents the first time a container is opened (hash of the world seed and its position), and
    // returns how many items it holds.
    int open(int index, int today, int extra_items = 0);

    // Removes a container (a foraging patch once searched).
    void remove(int index);

    // Floor piles disappear once emptied.
    void remove_if_empty_floor(int index);

    // How full the container records, the item pages or the stack budget are, whichever is fullest, in percent
    // (spec: caps show in the UI when close to full).
    [[nodiscard]] int fill_percent() const;

    // Item stacks that can still be stored anywhere (the save's budget less what the containers hold). Rolling
    // loot, storing and dropping check it.
    [[nodiscard]] int room() const;

    // Containers that hold at least one item (milestone 14: the save must keep 450 or more).
    [[nodiscard]] int holding() const;

    // Stacks held, and a checksum of what and where (save bench).
    [[nodiscard]] int stored_stacks(int& checksum) const;

    void save(save::writer& out) const;
    void load(save::reader& in);

private:
    bn::vector<container_record, max_containers> _records;
    bn::vector<int16_t, max_containers> _page_of;   // each record's page, or -1 (records only flag that they have one)
    bn::array<container, max_pages> _pages;
    bn::array<int16_t, max_pages> _page_owners;     // record index, or -1 for a free page
    container _overflow;                            // when every page holds items players dropped
    int _pinned = -1;                               // the container last asked for keeps its page

    void _free_page(int page);
    void _set_page(int index, int page);
    [[nodiscard]] int _used_stacks() const;
    [[nodiscard]] int _find_page();
};

}

#endif
