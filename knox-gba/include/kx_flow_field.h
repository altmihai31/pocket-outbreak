#ifndef KX_FLOW_FIELD_H
#define KX_FLOW_FIELD_H

#include "bn_common.h"

// The shared flow field chasing zombies follow (spec: Zombies > AI budget): a breadth-first fill over the 64x64 tiles
// around the player, built a slice per frame into a second buffer and swapped in when complete, so no zombie needs
// its own path search.
namespace kx::flow
{
    constexpr int size = 64;
    constexpr int unknown = 255;        // not reached (yet, or no way through)
    constexpr int blocked = 254;

    // Forgets the field (new game, teleport).
    void reset();

    // Once per frame: grows the field being built by up to `pops` tiles. A new fill starts from the player's tile
    // only while someone chases (reach_tiles: the farthest chaser, in tiles; 0 for none) and the last field is
    // stale (the player moved to another tile, or a chaser is beyond what it covered); it stops once past the
    // farthest chaser.
    BN_CODE_IWRAM void update(int player_tile_x, int player_tile_y, int pops, int reach_tiles);

    // Steps from a tile to the player in the last complete field, or unknown / blocked.
    [[nodiscard]] BN_CODE_IWRAM int distance(int tile_x, int tile_y);

    // The neighbouring tile one step closer to the player (diagonals only past open corners); false if the tile is
    // outside the field or has no way through.
    [[nodiscard]] BN_CODE_IWRAM bool next_step(int tile_x, int tile_y, int& next_x, int& next_y);
}

#endif
