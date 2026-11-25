#ifndef KX_FX_H
#define KX_FX_H

#include "bn_camera_ptr.h"
#include "bn_fixed_point.h"
#include "bn_sprite_ptr.h"
#include "bn_vector.h"

namespace kx
{

enum class fx_kind : uint8_t
{
    DUST,
    SPLASH,
    IMPACT,
    BLOOD
};

// Short-lived 8x8 particles (footstep dust, water splashes). Spec sprite budget: 16 particles.
class fx_pool
{

public:
    static constexpr int max_particles = 8;
    static constexpr int frames_per_kind = 3;
    static constexpr int ticks_per_frame = 5;

    explicit fx_pool(const bn::camera_ptr& camera);

    void spawn(fx_kind kind, const bn::fixed_point& position);

    void update();

private:
    struct particle
    {
        bn::sprite_ptr sprite;
        int first_frame;
        int age;
    };

    bn::camera_ptr _camera;
    bn::vector<particle, max_particles> _particles;
    bool _shaded_dark = false;      // the night shading the sprites have (kx_light)
};

}

#endif
