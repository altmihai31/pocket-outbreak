#ifndef KX_WORLD_MAP_H
#define KX_WORLD_MAP_H

#include "bn_optional.h"
#include "bn_sprite_ptr.h"

#include "kx_save.h"

namespace kx
{

class game;

// What the Map tab shows (milestone 12.5 stage 4, mockup 05; spec: Map and compass are items). It belongs to the
// world, so a new survivor keeps it, but needs a Map to see it.
namespace world_map
{
    // Fog of war: one bit per 32x32-tile chunk, 128x128 chunks (spec: 128x128 bitset, 2 KB).
    constexpr int chunks = 128;
    constexpr int chunk_shift = 5;
    constexpr int max_marks = 8;
    constexpr int max_auto_marks = 16;          // milestone 12.7: places found from keys and notes
    constexpr int save_bytes = (chunks * chunks / 8) + 4 + (max_marks * 2) + 4 + (max_auto_marks * 5);

    // What an automatic mark points at.
    enum class auto_mark : uint8_t
    {
        KEY,        // a key's house
        NOTE        // a note's place (stage 4)
    };

    // A new world: nothing explored, no marks.
    void reset();

    // The player is on this tile: its chunk and the 8 around it are explored (cheap while the chunk is the same).
    void explore(int tile_x, int tile_y);

    // Binoculars (milestone 14): the chunks within `reach` chunks of a tile's are explored.
    void scout(int tile_x, int tile_y, int reach);

    [[nodiscard]] bool explored(int chunk_x, int chunk_y);

    // Debug: the whole map explored.
    void reveal_all();

    // Marks (chunk coordinates), set on the Map tab with A.
    [[nodiscard]] int mark_count();
    void mark(int index, int& chunk_x, int& chunk_y);

    // Sets a mark on a chunk, or clears the one there. False when all 8 are used.
    bool toggle_mark(int chunk_x, int chunk_y);

    // Automatic marks (milestone 12.7): a key's house or a note's place, kept apart from the player's marks. The id
    // is what the mark is for (a house id), so it can go once that is done. A full list drops its oldest; one
    // already there for the id is kept.
    void add_auto_mark(auto_mark kind, uint16_t id, int tile_x, int tile_y);
    void remove_auto_marks(auto_mark kind, uint16_t id);
    [[nodiscard]] bool has_auto_mark(auto_mark kind, uint16_t id);
    [[nodiscard]] int auto_mark_count();
    void auto_mark_at(int index, auto_mark& kind, uint16_t& id, int& chunk_x, int& chunk_y);

    // The mark nearest a tile, in tiles (its chunk's middle); false without marks.
    [[nodiscard]] bool nearest_mark(int tile_x, int tile_y, int& mark_tile_x, int& mark_tile_y);

    // Each settlement's name, picked from a list of 24 by the seed (the same world always names them the same).
    [[nodiscard]] const char* settlement_name(int index);

    void save(save::writer& out);
    void load(save::reader& in);
}

// With a compass, an arrow at the screen edge points at the nearest mark while it is off screen.
class mark_arrow
{

public:
    void update(game& owner);
    void hide();

    // The arrow's screen position and frame, for the night bench's log; false while hidden.
    [[nodiscard]] bool shown(int& x, int& y, int& frame) const
    {
        if(! _arrow)
        {
            return false;
        }

        x = _arrow->x().integer();
        y = _arrow->y().integer();
        frame = _frame;
        return true;
    }

private:
    bn::optional<bn::sprite_ptr> _arrow;
    int _frame = -1;
};

}

#endif
