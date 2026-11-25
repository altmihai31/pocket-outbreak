#ifndef KX_CAMERA_H
#define KX_CAMERA_H

#include "bn_camera_ptr.h"
#include "bn_fixed_point.h"
#include "bn_random.h"

namespace kx
{

class player;

// Follows the player with a speed-scaled lookahead, eased motion and a deadzone (spec: Camera).
class camera_controller
{

public:
    explicit camera_controller(const bn::camera_ptr& camera);

    void snap_to(const player& target);

    void update(const player& target);

    void shake(int pixels, int frames);

    // World pixel at the screen's top-left corner.
    [[nodiscard]] int left() const;
    [[nodiscard]] int top() const;

private:
    bn::camera_ptr _camera;
    bn::fixed_point _position;
    bn::fixed_point _lookahead;
    bool _following_x = false;
    bool _following_y = false;
    int _aim_frames = 0;            // 0..aim ease frames: how far the aim shift has eased in
    int _aim_angle = 0;             // the last aim (degrees counterclockwise from east), kept while easing out
    int _shake_pixels = 0;
    int _shake_frames = 0;
    bn::random _random;

    void _clamp_and_apply();
};

}

#endif
