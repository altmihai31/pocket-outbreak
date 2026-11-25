#ifndef KX_BUILD_MODE_H
#define KX_BUILD_MODE_H

#include "bn_optional.h"
#include "bn_sprite_ptr.h"

#include "kx_base.h"
#include "kx_hud.h"

namespace kx
{

class game;
struct input_frame;

// Build mode (mockup 06): with the hammer selected on the hotbar, Select opens it. The player stands still; L1/R1
// pick a piece on the bar at the bottom, the D-pad moves a cursor (yellow brackets on a valid spot, red otherwise)
// up to 3 tiles away, holding A builds (the time counts down at the top, hammering is loud), B leaves. The world
// keeps running. A barricade on a damaged one mends it (and reinforces it at Carpentry 3); a campfire on a cold or
// burning one adds fuel; BED picks a bed up, then puts it down elsewhere.
class build_mode
{

public:
    [[nodiscard]] bool active() const
    {
        return _active;
    }

    // With the shovel it starts on the farm plot.
    void open(game& owner, bool farming = false);

    // Benches: builds a piece on a tile as holding A there would (the same checks and materials); the problem if
    // it can't, else nullptr.
    const char* place(game& owner, piece_kind kind, int tile_x, int tile_y);
    void close(game& owner);
    void update(game& owner, const input_frame& input);

    // What the HUD shows (mockup 06's bar and boxes).
    [[nodiscard]] const hud::build_view& view() const
    {
        return _view;
    }

private:
    // What building on the cursor's tile would do.
    struct target
    {
        const char* problem = nullptr;  // why not (red), or nullptr
        covered covers = covered::DOOR_FACE;
        bool repair = false;            // a barricade mended, or a campfire refuelled
        bool pick_bed = false;
    };

    bn::optional<bn::sprite_ptr> _corners[4];
    hud::build_view _view;
    int _piece = 0;
    int _cursor_x = 0;
    int _cursor_y = 0;
    int _hold = 0;
    bool _active = false;
    bool _carrying_bed = false;

    [[nodiscard]] target _check(game& owner) const;
    [[nodiscard]] bool _materials(game& owner, bool use);
    void _finish(game& owner, const target& value);
    void _update_view(game& owner, const target& value);
    void _update_cursor(game& owner, bool valid);
};

}

#endif
