#ifndef KX_CREATION_H
#define KX_CREATION_H

#include "kx_character.h"

namespace kx
{

// Character creation (mockup 08; spec: Other screens): the occupation (Left/Right) and traits in opposite pairs,
// good ones costing points and bad ones giving them back. Shown before a new world and before each new survivor.
// Start begins when the points are not negative; with points left, or a saved world the new one replaces, it asks
// once more first (playtest 6). B goes back (returns false).
[[nodiscard]] bool character_creation(character_choice& result, bool replaces_save = false);

}

#endif
