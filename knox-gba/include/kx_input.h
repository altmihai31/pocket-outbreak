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
};

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

}

#endif
