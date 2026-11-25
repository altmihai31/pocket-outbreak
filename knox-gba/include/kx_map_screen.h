#ifndef KX_MAP_SCREEN_H
#define KX_MAP_SCREEN_H

namespace kx
{

class canvas;
class game;
struct input_frame;

// Start menu, Map tab (mockup 05; the game is paused). Needs a Map carried. 18x10 map cells at 12 px across the whole
// width (playtest 4; a 32-tile chunk is 6 px): land, water, roads and settlements, hatched where unexplored, names of
// the settlements visited, safehouses, marks. D-pad moves the cursor a chunk at a time (the view follows), A sets or
// clears a mark. With a compass it opens on the player, shows them as a red marker and has a north arrow; without
// one it opens where it was left.
class map_screen
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
    int _cursor_x = -1;             // chunk; -1 until first opened
    int _cursor_y = -1;
    int _view_x = 0;                // top-left map cell shown
    int _view_y = 0;
    int _repeat = 0;
    bool _active = false;
    bool _dirty = false;

    void _follow_cursor();
    void _draw(game& owner);
};

}

#endif
