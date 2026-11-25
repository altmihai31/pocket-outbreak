#ifndef KX_HORDE_EDGES_H
#define KX_HORDE_EDGES_H

#include "bn_optional.h"
#include "bn_sprite_ptr.h"

#include "kx_hordes.h"

namespace kx
{

class game;

// The off-screen part of a horde too big for the sprite pool (spec: Zombies > Hordes and migration): a red arrow at
// the screen edge per direction it waits in, plus distant groans panned towards it.
class horde_edges
{
public:
    // Once per frame while playing.
    void update(game& owner);

    // Hides every arrow (menus, death, teleport).
    void hide();

private:
    bn::optional<bn::sprite_ptr> _arrows[hordes::edge_directions];
    int _frame = 0;
    int _groan_timer = 0;
};

}

#endif
