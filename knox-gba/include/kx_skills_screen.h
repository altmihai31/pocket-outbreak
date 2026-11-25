#ifndef KX_SKILLS_SCREEN_H
#define KX_SKILLS_SCREEN_H

namespace kx
{

class game;
struct input_frame;

// Start menu, Skills tab (mockup 07; the game is paused): Fitness and Strength, then the 14 skills with 5 level
// pips each (the next one fills with XP); the selected one's level, XP, occupation and trait bonuses, book and what
// it does on the right. Up and Down pick a skill. Skills nothing trains yet are dimmed.
class skills_screen
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
    int _selected = 0;
    bool _active = false;
    bool _dirty = false;

    void _draw(game& owner);
};

}

#endif
