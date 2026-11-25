#ifndef KX_GEAR_SCREEN_H
#define KX_GEAR_SCREEN_H

#include "kx_bench.h"
#include "kx_ui.h"

namespace kx
{

class game;
struct input_frame;

// Start menu, Gear tab (spec: Menu tabs; the game is paused): worn and held items on the left, carried items
// on the right with the weight, the hotbar, and the selected item's info. A opens the item's actions (use,
// equip, wear, eat or remove; move to another carried slot; hotbar; drop at your feet). L/R switch tabs.
class gear_screen
{

public:
    [[nodiscard]] bool active() const
    {
        return _active;
    }

    // Redraws the whole screen on the next update (a popup drawn over it has gone).
    void refresh()
    {
        _dirty = true;
    }

    #if KX_DUMP
        // For the dumps: a bag slot chosen.
        void show_bag_slot(int slot)
        {
            _panel = 1;
            _cursor[1] = slot;
            _dirty = true;
        }
    #endif

    void open(game& owner);
    void update(game& owner, const input_frame& input);
    void close(game& owner);

    // True while the action list is open or an item is being moved (L/R then belong to the screen, not the tabs).
    [[nodiscard]] bool choosing() const
    {
        return _choosing || _moving >= 0;
    }

private:
    int _panel = 1;             // 0 worn, 1 carried
    int _cursor[2] = {};
    int _choice = 0;
    int _moving = -1;           // carried slot being moved, or -1
    bool _active = false;
    bool _dirty = false;
    bool _choosing = false;

    void _move_cursor(int dx, int dy, bool change_panel);
    void _draw(game& owner);
};

}

#endif
