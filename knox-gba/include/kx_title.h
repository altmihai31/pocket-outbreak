#ifndef KX_TITLE_H
#define KX_TITLE_H

#include "bn_common.h"

#include "kx_sandbox.h"

namespace kx
{

struct title_choice
{
    uint32_t seed = 0;
    bool load = false;      // continue the saved world
    bool new_survivor = false;  // ... whose last survivor died (a new one is made first)
    bool replaces = false;      // a new world with a saved one it replaces
    sandbox::settings sandbox;  // a new world's sandbox settings (a saved one's come from its save)
};

// Title screen. With a save: CONTINUE (the day and time it was saved; a new survivor if the last one died) or NEW
// WORLD, which asks before replacing the save. New world (spec: Other screens > world creation): shows the seed,
// Left/Right roll a new one, A or Start begins. Without a roll the seed comes from the moment A is pressed. Below
// the seed, the sandbox settings (milestone 13): up and down pick one, left and right change it.
[[nodiscard]] title_choice title_screen();

}

#endif
