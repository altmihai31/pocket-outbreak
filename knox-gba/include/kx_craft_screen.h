#ifndef KX_CRAFT_SCREEN_H
#define KX_CRAFT_SCREEN_H

namespace kx
{

class game;
struct input_frame;

// Start menu, Craft tab (mockup 09; spec: recipes filtered to what you can make now): the heat source nearby, the
// recipes (those that can be cooked now first, the rest dimmed), and the selected one's needs (carried / needed,
// green when there is enough) and what it makes. A cooks it (the menu closes while it cooks).
class craft_screen
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

    void open(game& owner);
    void update(game& owner, const input_frame& input);
    void close(game& owner);

private:
    int _order[32] = {};        // recipe indices, ready ones first
    int _selected = 0;          // into _order
    bool _active = false;
    bool _dirty = false;

    void _sort(game& owner);
    void _draw(game& owner);
};

}

#endif
