#ifndef KX_DEBUG_MENU_H
#define KX_DEBUG_MENU_H

#include "bn_sprite_ptr.h"
#include "bn_sprite_text_generator.h"
#include "bn_vector.h"

namespace kx
{

class game;
class tuning_set;

// Paused tuning editor (Start). Pages are tuning tables; edits apply live when the menu closes.
// Uses sprite text, which the spec allows only while the game is paused.
class debug_menu
{

public:
    debug_menu();

    [[nodiscard]] bool active() const
    {
        return _active;
    }

    void open(game& owner);

    void update(game& owner);

private:
    static constexpr int visible_rows = 11;

    bn::sprite_text_generator _text;
    bn::vector<bn::sprite_ptr, 96> _sprites;
    int _page = 0;
    int _row = 0;
    int _scroll = 0;
    int _repeat_frames = 0;
    int _event = 0;                 // Debug page: the world event A starts
    int _vehicle = 0;               // and the vehicle A spawns (0 car, 1 bicycle)
    int _hour = 23;                 // Debug page: the hour Set hour moves the clock to
    int _item = 1;                  // Debug page: the item A gives (an item_id)
    int _settlement = 0;
    bool _active = false;

    [[nodiscard]] tuning_set& _tuning() const;
    [[nodiscard]] int _rows_count() const;
    [[nodiscard]] bool _repeat(bool pressed, bool held);
    void _close(game& owner);
    void _redraw(game& owner);
};

}

#endif
