#include "kx_fx.h"
#include "kx_light.h"

#include "bn_sprite_items_fx.h"

namespace kx
{

namespace
{
    constexpr int fx_bg_priority = 2;
    constexpr int fx_z_order = 4;      // behind actors in the lowest screen band (see depth_z)
}

fx_pool::fx_pool(const bn::camera_ptr& camera) :
    _camera(camera)
{
}

void fx_pool::spawn(fx_kind kind, const bn::fixed_point& position)
{
    if(_particles.full())
    {
        _particles.erase(_particles.begin());
    }

    int first_frame = int(kind) * frames_per_kind;
    bn::sprite_ptr sprite = bn::sprite_items::fx.create_sprite(position, first_frame);
    sprite.set_camera(_camera);
    sprite.set_bg_priority(fx_bg_priority);
    sprite.set_z_order(fx_z_order);
    light::shade(sprite);
    _particles.push_back(particle{ bn::move(sprite), first_frame, 0 });
}

void fx_pool::update()
{
    bool reshade = light::sprites_dark() != _shaded_dark || (light::sprites_dark() && light::lights_on());
    _shaded_dark = light::sprites_dark();

    for(auto it = _particles.begin(); it != _particles.end();)
    {
        particle& current = *it;
        ++current.age;

        int frame = current.age / ticks_per_frame;

        if(frame >= frames_per_kind)
        {
            it = _particles.erase(it);
        }
        else
        {
            if(reshade)
            {
                light::shade(current.sprite);
            }

            if(current.age % ticks_per_frame == 0)
            {
                current.sprite.set_tiles(bn::sprite_items::fx.tiles_item(), current.first_frame + frame);
            }

            ++it;
        }
    }
}

}
