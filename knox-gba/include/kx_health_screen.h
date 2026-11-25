#ifndef KX_HEALTH_SCREEN_H
#define KX_HEALTH_SCREEN_H

namespace kx
{

class game;
struct input_frame;

// Start menu, Health tab (mockup 04; the game is paused): body diagram with the selected part and its wound,
// blood / food / water / body temperature / carry bars and the active moodles. Up and Down pick a body part,
// A treats it with what you carry (bandage, new bandage, disinfectant, splint).
class health_screen
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
    int _part = 0;
    int _blink = 0;
    bool _active = false;
    bool _dirty = false;

    void _draw(game& owner);
    void _draw_body(game& owner);
};

}

#endif
