#ifndef KX_SEARCH_SCREEN_H
#define KX_SEARCH_SCREEN_H

#include "kx_ui.h"

namespace kx
{

class game;
struct input_frame;

// Container overlay (mockup 03): container grid on the left, carried items on the right, item info and
// controls below. The first search of a container reveals its items one at a time with a pop (rare ones
// chime and flash); the world keeps running behind it, so a zombie can interrupt.
class search_screen
{

public:
    [[nodiscard]] bool active() const
    {
        return _active;
    }

    // Redraws the whole screen (a popup drawn over it has gone), split over two frames like its first draw: in one
    // frame it overran by 5% (bench 3).
    void refresh()
    {
        _opening = 2;
    }

    void open(game& owner, int container_index, int extra_items = 0);
    void update(game& owner, const input_frame& input);
    void close(game& owner);

private:
    int _container = -1;
    int _revealed = 0;          // container slots below this index are visible
    int _timer = 0;
    int _frames = 0;
    int _shown_seconds = -1;
    int _panel = 0;             // 0 container, 1 carried
    int _cursor[2] = {};
    int _flash = 0;
    int _bag_flash = 0;         // frames left of the carried weight blinking red (a take blocked by weight)
    bool _active = false;
    bool _dirty = false;
    int _opening = 0;           // frames left of the first draw (split in two)
    int _revealed_shown = 0;    // container slots drawn as revealed

    void _advance_reveal(game& owner);
    void _take(game& owner, int slot, bool half);
    void _store(game& owner, int slot);
    void _move_cursor(int dx, int dy);
    void _draw(game& owner);
    void _draw_left(game& owner);
    void _draw_right(game& owner);
    void _draw_weight(game& owner);
    void _draw_time(game& owner);
    void _draw_header(game& owner);
    void _draw_container_slot(game& owner, int slot);
    void _draw_info(game& owner);
    void _draw_reveals(game& owner);
};

}

#endif
