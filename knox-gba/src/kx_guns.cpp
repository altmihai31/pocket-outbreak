#include "kx_guns.h"

#include "bn_math.h"
#include "bn_sprite_double_size_mode.h"
#include "bn_sprite_items_aim.h"
#include "bn_sprite_items_fx.h"

namespace kx
{

namespace
{
    // The aim sprite's frames (tools/assets/fx.py, build_aim): 32 px lines, drawn at twice their length.
    constexpr int streak_frame = 0;
    constexpr int dotted_frame = 1;
    constexpr int line_scale = 2;
    constexpr int line_half_length = 16 * line_scale;
    constexpr int line_gap = 3;                 // from the muzzle to where a line starts
    constexpr int shot_frames = 3;              // how long a shot's streaks and flash show

    // fx frames (tools/assets/fx.py): the impact spark as the muzzle flash, the build cursor's yellow corner.
    constexpr int flash_frame = 6;
    constexpr int corner_frame = 17;
    constexpr int ui_bg_priority = 1;


    static_assert(bn::sprite_items::fx.tiles_item().graphics_count() > corner_frame);
}

tuning_set& gun_tuning()
{
    static BN_DATA_EWRAM_BSS tuning_set result("GUNS", gun_tuning_defaults);
    return result;
}

gun_effects::gun_effects(const bn::camera_ptr& camera) :
    _camera(camera)
{
}

void gun_effects::_line(bn::optional<bn::sprite_ptr>& sprite, const bn::fixed_point& start, int angle, int frame)
{
    angle = ((angle % 360) + 360) % 360;
    int distance = line_gap + line_half_length;
    bn::fixed_point centre(start.x() + (bn::degrees_lut_cos(angle) * distance),
                           start.y() - (bn::degrees_lut_sin(angle) * distance));

    if(! sprite)
    {
        bn::sprite_ptr created = bn::sprite_items::aim.create_sprite(centre, frame);
        created.set_camera(_camera);
        created.set_bg_priority(ui_bg_priority);
        created.set_double_size_mode(bn::sprite_double_size_mode::ENABLED);
        created.set_horizontal_scale(line_scale);
        sprite = bn::move(created);
    }
    else
    {
        sprite->set_position(centre);
    }

    sprite->set_rotation_angle(angle);
}

void gun_effects::shot(const bn::fixed_point& muzzle, int angle)
{
    if(_streak_frames < shot_frames)
    {
        // A new volley: the last one's streaks go.
        for(bn::optional<bn::sprite_ptr>& streak : _streaks)
        {
            streak.reset();
        }
    }

    for(bn::optional<bn::sprite_ptr>& streak : _streaks)
    {
        if(! streak)
        {
            _line(streak, muzzle, angle, streak_frame);
            break;
        }
    }

    if(! _flash)
    {
        _flash = bn::sprite_items::fx.create_sprite(muzzle, flash_frame);
        _flash->set_camera(_camera);
        _flash->set_bg_priority(ui_bg_priority);
    }
    else
    {
        _flash->set_position(muzzle);
    }

    _streak_frames = shot_frames;
}

void gun_effects::update(const aim_view& value)
{
    if(_streak_frames > 0 && --_streak_frames == 0)
    {
        for(bn::optional<bn::sprite_ptr>& streak : _streaks)
        {
            streak.reset();
        }

        _flash.reset();
    }

    if(! value.aiming)
    {
        for(bn::optional<bn::sprite_ptr>& edge : _edges)
        {
            edge.reset();
        }

        for(bn::optional<bn::sprite_ptr>& bracket : _brackets)
        {
            bracket.reset();
        }

        return;
    }

    _line(_edges[0], value.muzzle, value.angle + value.cone, dotted_frame);

    if(value.single_line)
    {
        _edges[1].reset();
    }
    else
    {
        _line(_edges[1], value.muzzle, value.angle - value.cone, dotted_frame);
    }

    // Corner brackets round the target: the corner graphic is a sprite's top-left, flipped for the others.
    for(int index = 0; index < 4; ++index)
    {
        bn::optional<bn::sprite_ptr>& bracket = _brackets[index];

        if(! value.target)
        {
            bracket.reset();
            continue;
        }

        bool right = index & 1;
        bool bottom = index & 2;
        bn::fixed_point position(value.target_feet.x() + (right ? value.box_half_width - 4 : 4 - value.box_half_width),
                                 value.target_feet.y() - (bottom ? 4 : value.box_height - 4));

        if(! bracket)
        {
            bracket = bn::sprite_items::fx.create_sprite(position, corner_frame);
            bracket->set_camera(_camera);
            bracket->set_bg_priority(ui_bg_priority);
            bracket->set_horizontal_flip(right);
            bracket->set_vertical_flip(bottom);
        }
        else
        {
            bracket->set_position(position);
        }
    }
}

}
