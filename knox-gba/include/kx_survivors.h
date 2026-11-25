#ifndef KX_SURVIVORS_H
#define KX_SURVIVORS_H

#include "bn_fixed_point.h"

#include "kx_items.h"

namespace kx
{

class game;

namespace save
{
    class reader;
    class writer;
}

// Permadeath and past survivors (spec: Save system > Permadeath): on death the character is gone but the world
// stays. The dead survivor rises as a zombie in their own clothes, carrying their gear (in a hidden container),
// and a new survivor starts in a random village. Up to 4 past survivors are remembered; their zombie appears when
// the player comes near where it was last, and putting it down leaves a DEAD SURVIVOR body with the gear.
namespace survivors
{
    constexpr int max_survivors = 4;

    // The gear container of a past survivor lives off the map, on this tile row, so nothing finds it by position.
    constexpr int gear_tile_x = -2;

    // A new world: nobody has died yet.
    void reset();

    // The survivor just died at their feet: their gear goes with their zombie (what does not fit is left in a
    // sack where they fell).
    void bury(game& owner);

    // Once per frame while playing: a past survivor's zombie appears when the player comes near.
    void update(game& owner);

    // Its zombie went back into the world (dissolved far from the player) at this position.
    void left(int id, const bn::fixed_point& position);

    // Where its zombie stands now (before a save; it stays walking).
    void seen_at(int id, const bn::fixed_point& position);

    // Its zombie was put down here: the gear becomes a searchable body.
    void killed(game& owner, int id, const bn::fixed_point& position);

    // Clothes worn when they died (for the zombie's palette), by equip slot.
    [[nodiscard]] const item_id* worn(int id);

    // Where a past survivor's zombie was last.
    void position(int id, int& tile_x, int& tile_y);

    // Deaths in this world so far.
    [[nodiscard]] int deaths();

    // Where the next survivor starts: the crossroads of a random village (another one for each death).
    void start_tile(int& tile_x, int& tile_y);

    void save(save::writer& out);
    void load(save::reader& in);
}

}

#endif
