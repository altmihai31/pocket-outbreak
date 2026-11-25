#ifndef KX_INPUT_H
#define KX_INPUT_H

namespace kx
{

// Input for one frame (spec: Controls and UI): gameplay and the search and gear screens. The debug menu
// reads bn::keypad directly.
struct input_frame
{
    int dx = 0;                     // -1, 0, 1
    int dy = 0;
    bool up_pressed = false;
    bool down_pressed = false;
    bool left_pressed = false;
    bool right_pressed = false;
    bool a_held = false;
    bool a_pressed = false;
    bool b_held = false;
    bool b_pressed = false;
    bool b_released = false;
    bool l_held = false;
    bool l_pressed = false;
    bool l_released = false;
    bool select_pressed = false;
    bool select_held = false;
    bool start_pressed = false;
    bool r_pressed = false;
    bool r_held = false;
    bool repeated = false;          // a direction press made by menu_repeat (a held direction), not a new press
};

// Up/Down in a list of `count` rows: a press wraps round at the ends, a held direction stops there (playtest 5).
[[nodiscard]] inline int step_list(int value, int count, const input_frame& input)
{
    int next = value + (input.down_pressed ? 1 : -1);

    if(next < 0 || next >= count)
    {
        return input.repeated ? value : (next + count) % count;
    }

    return next;
}

// Reads the keypad, or in bench builds (make BENCH=1) plays a scripted route for repeatable profiling.
class input_source
{

public:
    [[nodiscard]] input_frame read();

    // Bench builds: true once the scripted route is over.
    [[nodiscard]] bool finished() const
    {
        return _finished;
    }

    // Bench builds: frames of the route played so far (bench frames stop in menus and asleep; this doesn't).
    [[nodiscard]] int route_frame() const
    {
        return _route_frame;
    }

private:
    int _route_frame = 0;
    int _segment = 0;
    int _segment_frame = 0;
    unsigned _previous_buttons = 0;
    int _previous_dx = 0;
    int _previous_dy = 0;
    bool _finished = false;
    bool _primed = false;       // the first read takes what is already held as the previous state
};

// Held directions in menu lists (playtest 5): a press steps once; held, it steps again after a delay and then at a
// steady rate, as extra presses. Menus and searches only: in the world a press is a step of the double-tap jog.
namespace menu_repeat_tuning
{
    constexpr int delay_frames = 18;
    constexpr int interval_frames = 5;
}

class menu_repeat
{

public:
    void apply(input_frame& input);

    void reset()
    {
        _held[0] = _held[1] = _held[2] = _held[3] = 0;
    }

private:
    int _held[4] = {};      // frames each direction has been held: up, down, left, right
};

}

#endif
