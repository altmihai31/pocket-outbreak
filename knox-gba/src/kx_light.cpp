#include "kx_light.h"

#include "bn_algorithm.h"
#include "bn_math.h"
#include "bn_sprite_double_size_mode.h"
#include "bn_sprite_items_light.h"

namespace kx
{

namespace
{
    constexpr int day_minutes = 24 * 60;

    bool dark_sprites = false;

    // The light sprite's frames (tools/assets/fx.py, build_light).
    constexpr int fire_frame = 0;
    constexpr int beam_frame = 1;
    constexpr int holder_frame = 2;

    // A beam is drawn at double size, so its apex (the frame's left edge) is 64 px from the sprite's centre.
    constexpr int beam_scale = 2;
    constexpr int beam_apex_distance = 32 * beam_scale;

    // Turning sweeps the flashlight round at this many degrees a frame (a half turn in 12 frames).
    constexpr int flashlight_turn = 15;

    // The light frames' shapes (tools/assets/fx.py), for the code's own test of what a light covers: the solid
    // part plus half the soft edge. The fire glow is 30 px in radius (26 solid), the holder's 15 (12 solid); a beam
    // spreads 25 degrees each side (21 solid) and is cut by a circle of 31.5 frame px (28.5 solid) round the frame's
    // centre, 32 frame px along from the apex, drawn at 2x.
    constexpr int fire_lit_radius = 28;
    constexpr int holder_lit_radius = 14;
    constexpr int beam_tan_per_mille = 424;         // tan(23 degrees)
    constexpr int beam_reach_per_mille = 1880;      // the cut circle's radius, of the 32 px frame half (30 of 32, x2)

    struct lit_disc
    {
        int x;
        int y;
        int radius;
    };

    struct lit_beam
    {
        int apex_x;
        int apex_y;
        int dir_x;          // the beam's direction, x256
        int dir_y;
        int centre_x;       // the cut circle
        int centre_y;
        int radius;
    };

    constexpr int max_lit_discs = 4;
    constexpr int max_lit_beams = 3;

    struct lit_set
    {
        bool room = false;
        int room_left = 0;
        int room_top = 0;
        int room_right = 0;
        int room_bottom = 0;
        int discs = 0;
        lit_disc disc[max_lit_discs];
        int beams = 0;
        lit_beam beam[max_lit_beams];
        bool before = false;
        int box_left = 0;           // everything lit lies in this box (a quick no for most sprites)
        int box_top = 0;
        int box_right = 0;
        int box_bottom = 0;
    };

    void grow_box(lit_set& value, int left, int top, int right, int bottom)
    {
        bool first = ! value.room && ! value.discs && ! value.beams;
        value.box_left = first ? left : bn::min(value.box_left, left);
        value.box_top = first ? top : bn::min(value.box_top, top);
        value.box_right = first ? right : bn::max(value.box_right, right);
        value.box_bottom = first ? bottom : bn::max(value.box_bottom, bottom);
    }

    BN_DATA_EWRAM_BSS lit_set lights;
}

tuning_set& light_tuning()
{
    static BN_DATA_EWRAM_BSS tuning_set result("NIGHT", light_tuning_defaults);
    return result;
}

namespace light
{

int darkness(int minute_of_day, bool fog, int shift_minutes)
{
    int night = tuning(light_param::NIGHT_STEPS);
    int ramp = tuning(light_param::RAMP_MINUTES);
    int dusk = (tuning(light_param::DUSK_START) + shift_minutes + day_minutes) % day_minutes;
    int dawn = (tuning(light_param::DAWN_END) - shift_minutes + day_minutes) % day_minutes;
    int length = (dawn - dusk + day_minutes) % day_minutes;
    int since = (minute_of_day - dusk + day_minutes) % day_minutes;

    if(night <= 0 || length <= 0 || since >= length)
    {
        return 0;
    }

    // Both ramps reach whole steps evenly: 10 steps over 60 minutes is one step every 6 minutes.
    ramp = bn::min(ramp, length / 2);
    int steps = night;

    if(since < ramp)
    {
        steps = ((since + 1) * night) / ramp;
    }
    else if(since >= length - ramp)
    {
        steps = ((length - since) * night) / ramp;
    }

    if(fog && steps > 0)
    {
        steps += (tuning(light_param::FOG_STEPS) * steps) / night;
    }

    return bn::min(steps, full_steps);
}

bool sprites_dark()
{
    return dark_sprites;
}

void set_sprites_dark(bool dark)
{
    dark_sprites = dark;
}

void clear_lit()
{
    lights.before = lights.room || lights.discs || lights.beams;
    lights.room = false;
    lights.discs = 0;
    lights.beams = 0;
}

void add_lit_rect(int left, int top, int right, int bottom)
{
    grow_box(lights, left, top, right, bottom);
    lights.room = true;
    lights.room_left = left;
    lights.room_top = top;
    lights.room_right = right;
    lights.room_bottom = bottom;
}

void add_lit_disc(const bn::fixed_point& centre, int radius)
{
    if(lights.discs < max_lit_discs)
    {
        int x = centre.x().integer();
        int y = centre.y().integer();
        grow_box(lights, x - radius, y - radius, x + radius + 1, y + radius + 1);
        lights.disc[lights.discs++] = lit_disc{ centre.x().integer(), centre.y().integer(), radius };
    }
}

void add_lit_beam(const bn::fixed_point& apex, int angle, int percent)
{
    if(lights.beams >= max_lit_beams)
    {
        return;
    }

    // Screen y grows downwards, so the direction's y is minus the sine.
    int dir_x = (bn::degrees_lut_cos(angle) * 256).round_integer();
    int dir_y = -(bn::degrees_lut_sin(angle) * 256).round_integer();
    int distance = (beam_apex_distance * percent) / 100;
    int x = apex.x().integer();
    int y = apex.y().integer();
    int centre_x = x + ((dir_x * distance) / 256);
    int centre_y = y + ((dir_y * distance) / 256);
    int radius = (distance * beam_reach_per_mille) / 2000;
    grow_box(lights, centre_x - radius, centre_y - radius, centre_x + radius + 1, centre_y + radius + 1);
    lights.beam[lights.beams++] = lit_beam{ x, y, dir_x, dir_y, centre_x, centre_y, radius };
}

bool lit(const bn::fixed_point& position)
{
    int x = position.x().integer();
    int y = position.y().integer();

    if((! lights.room && ! lights.discs && ! lights.beams) || x < lights.box_left || x >= lights.box_right ||
       y < lights.box_top || y >= lights.box_bottom)
    {
        return false;
    }

    if(lights.room && x >= lights.room_left && x < lights.room_right && y >= lights.room_top &&
       y < lights.room_bottom)
    {
        return true;
    }

    for(int index = 0; index < lights.discs; ++index)
    {
        const lit_disc& value = lights.disc[index];
        int dx = x - value.x;
        int dy = y - value.y;

        if(bn::abs(dx) <= value.radius && bn::abs(dy) <= value.radius &&
           (dx * dx) + (dy * dy) <= value.radius * value.radius)
        {
            return true;
        }
    }

    for(int index = 0; index < lights.beams; ++index)
    {
        const lit_beam& value = lights.beam[index];
        int cx = x - value.centre_x;
        int cy = y - value.centre_y;

        if(bn::abs(cx) > value.radius || bn::abs(cy) > value.radius ||
           (cx * cx) + (cy * cy) > value.radius * value.radius)
        {
            continue;
        }

        // Inside the spread: across the beam no more than tan(23 degrees) of the way along it.
        int vx = x - value.apex_x;
        int vy = y - value.apex_y;
        int along = (vx * value.dir_x) + (vy * value.dir_y);
        int across = bn::abs((vx * value.dir_y) - (vy * value.dir_x));

        if(along > 0 && across * 1000 <= along * beam_tan_per_mille)
        {
            return true;
        }
    }

    return false;
}

bool lights_on()
{
    return lights.before || lights.room || lights.discs || lights.beams;
}

int flashlight_eighths(const item_stack& stack)
{
    int full = bn::max(tuning(light_param::BATTERY_MINUTES), 1);
    int charge = flashlight_charge(stack);
    return charge <= 0 ? 0 : bn::clamp((charge * 8) / full, 1, 8);
}

int facing_angle(int facing_x, int facing_y)
{
    // Screen y grows downwards, so up is 90.
    constexpr int angles[3][3] = {
        { 135, 90, 45 },        // facing up: left, straight, right
        { 180, 0, 0 },
        { 225, 270, 315 }       // facing down
    };

    return angles[bn::clamp(facing_y, -1, 1) + 1][bn::clamp(facing_x, -1, 1) + 1];
}

}

light_sprites::light_sprites(const bn::camera_ptr& camera) :
    _camera(camera)
{
}

void light_sprites::update(const light_view& value)
{
    // The flashlight sweeps round to a new facing the short way.
    if(value.flashlight)
    {
        if(! _flashlight)
        {
            _flashlight_angle = value.flashlight_angle;
        }
        else
        {
            int difference = ((value.flashlight_angle - _flashlight_angle + 540) % 360) - 180;
            _flashlight_angle = (_flashlight_angle + bn::clamp(difference, -flashlight_turn, flashlight_turn) + 360) %
                                360;
        }
    }

    // Up to its full 128 px a beam is one frame drawn smaller or at full size; a longer one adds a second beam
    // further along (its narrower tip stays inside the first one's spread).
    int percent = bn::clamp(value.flashlight_percent, 25, 150);
    _beam(_flashlight, value.flashlight, value.flashlight_apex, _flashlight_angle, bn::min(percent, 100));

    int extra = ((percent - 100) * beam_apex_distance * 2) / 100;
    bn::fixed_point far_apex(value.flashlight_apex.x() + (bn::degrees_lut_cos(_flashlight_angle) * extra),
                             value.flashlight_apex.y() - (bn::degrees_lut_sin(_flashlight_angle) * extra));
    _beam(_flashlight_far, value.flashlight && extra > 0, far_apex, _flashlight_angle);
    _glow(_holder, value.flashlight, value.holder, holder_frame);
    _beam(_headlights, value.headlights, value.headlight_apex, value.headlight_angle);

    for(int index = 0; index < light_view::max_fires; ++index)
    {
        _glow(_fires[index], index < value.fires, value.fire[index], fire_frame);
    }

    // The same lights for the sprites, as drawn (the flashlight at its swept angle).
    if(value.flashlight)
    {
        light::add_lit_beam(value.flashlight_apex, _flashlight_angle, bn::min(percent, 100));

        if(extra > 0)
        {
            light::add_lit_beam(far_apex, _flashlight_angle, 100);
        }

        light::add_lit_disc(value.holder, holder_lit_radius);
    }

    if(value.headlights)
    {
        light::add_lit_beam(value.headlight_apex, value.headlight_angle, 100);
    }

    for(int index = 0; index < value.fires; ++index)
    {
        light::add_lit_disc(value.fire[index], fire_lit_radius);
    }
}

void light_sprites::_beam(bn::optional<bn::sprite_ptr>& sprite, bool on, const bn::fixed_point& apex, int angle,
                          int percent)
{
    if(! on)
    {
        sprite.reset();
        return;
    }

    // The frame's apex is its left edge's middle, so the centre lies along the beam from the apex.
    bn::fixed scale = bn::fixed(beam_scale * percent) / 100;
    int distance = (beam_apex_distance * percent) / 100;
    bn::fixed_point centre(apex.x() + (bn::degrees_lut_cos(angle) * distance),
                           apex.y() - (bn::degrees_lut_sin(angle) * distance));

    if(! sprite)
    {
        // A window sprite is never drawn: its opaque pixels mark where the sprite window's rules apply.
        bn::sprite_ptr created = bn::sprite_items::light.create_sprite(centre, beam_frame);
        created.set_camera(_camera);
        created.set_window_enabled(true);
        created.set_double_size_mode(bn::sprite_double_size_mode::ENABLED);
        sprite = bn::move(created);
    }
    else
    {
        sprite->set_position(centre);
    }

    if(sprite->horizontal_scale() != scale)
    {
        sprite->set_scale(scale);
    }

    sprite->set_rotation_angle(angle);
}

void light_sprites::_glow(bn::optional<bn::sprite_ptr>& sprite, bool on, const bn::fixed_point& centre, int frame)
{
    if(! on)
    {
        sprite.reset();
        return;
    }

    if(! sprite)
    {
        bn::sprite_ptr created = bn::sprite_items::light.create_sprite(centre, frame);
        created.set_camera(_camera);
        created.set_window_enabled(true);
        sprite = bn::move(created);
    }
    else
    {
        sprite->set_position(centre);
    }
}

}
