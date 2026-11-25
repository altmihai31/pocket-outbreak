#ifndef KX_SAFE_SCREEN_H
#define KX_SAFE_SCREEN_H

#include <cstdint>

namespace kx
{

class game;
struct input_frame;

// A gun safe's dial (milestone 12.7 stage 4, mockup 12 panel 5): three digits; Left/Right pick one, Up/Down turn
// it, A tries the code (a click when wrong; when right, the safe opens and is searched), B leaves. A code read on a
// note shows under the digits. The world keeps running meanwhile, as while searching.
class safe_screen
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

    void open(game& owner, int container_index);
    void update(game& owner, const input_frame& input);
    void close(game& owner);

    // Benches: the digits as a 3-digit number.
    void set_digits(int code);

private:
    int _container = -1;
    uint16_t _house = 0;
    int8_t _digits[3] = {};
    int8_t _selected = 0;
    bool _active = false;
    bool _dirty = false;

    void _draw(game& owner);
};

}

#endif
