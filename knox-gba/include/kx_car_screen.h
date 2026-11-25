#ifndef KX_CAR_SCREEN_H
#define KX_CAR_SCREEN_H

namespace kx
{

class game;
struct input_frame;

// The car screen (mockup 10; spec: a working car needs 5 parts, shown as a tick or cross checklist): the parts on
// the left with the car's actions under them (trunk, glove box, siphon, hotwire, drive), and what the selected line
// needs on the right. A fits a part or does the action, R1 takes a part out, B closes. Like searching, the world
// keeps running while it is open.
class car_screen
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

    void open(game& owner, int vehicle);
    void update(game& owner, const input_frame& input);
    void close(game& owner);

private:
    int _vehicle = -1;
    int _row = 0;               // the parts, then the actions
    bool _active = false;
    bool _dirty = false;
    int _halves = 0;            // halves of a redraw still to draw: 2 the left (header, list), 1 the right

    // A redraw is split over two frames (playtest 2: in one, opening the screen overran by 9-41%).
    void _draw_left(game& owner);
    void _draw_right(game& owner);
};

}

#endif
