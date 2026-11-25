#include "kx_camera.h"

#include "kx_settings.h"

#include "bn_algorithm.h"
#include "bn_display.h"
#include "bn_math.h"

#include "kx_camera_tuning.h"
#include "kx_movement_tuning.h"
#include "kx_player.h"
#include "kx_vector.h"
#include "kx_world.h"

namespace kx
{

namespace
{
    constexpr int half_screen_width = bn::display::width() / 2;
    constexpr int half_screen_height = bn::display::height() / 2;

    [[nodiscard]] bn::fixed_point focus_point(const player& target)
    {
        return target.position() - bn::fixed_point(0, tuning(camera_param::BODY_OFFSET));
    }

    // Deadzone with hysteresis: small gaps never start the camera, but once it moves it settles fully,
    // so the lookahead is not eaten by the deadzone.
    void follow_axis(bn::fixed& position, bn::fixed target, bool& following)
    {
        bn::fixed gap = target - position;
        bn::fixed abs_gap = bn::abs(gap);

        if(! following && abs_gap > tuning(camera_param::DEADZONE))
        {
            following = true;
        }

        if(following)
        {
            if(abs_gap < 1)
            {
                following = false;
            }
            else
            {
                position += gap / bn::max(tuning(camera_param::EASE_DIV).integer(), 1);
            }
        }
    }
}

camera_controller::camera_controller(const bn::camera_ptr& camera) :
    _camera(camera)
{
}

void camera_controller::snap_to(const player& target)
{
    _position = focus_point(target);
    _lookahead = bn::fixed_point();
    _aim_frames = 0;
    _following_x = false;
    _following_y = false;
    _clamp_and_apply();
}

void camera_controller::update(const player& target)
{
    bn::fixed_point desired_lookahead;
    int input_x = target.input_x();
    int input_y = target.input_y();

    if((input_x || input_y) && target.speed() > 0)
    {
        bn::fixed walk_speed = tuning(move_param::WALK_SPEED);
        bn::fixed sprint_speed = tuning(move_param::SPRINT_SPEED);
        bn::fixed min_distance = tuning(camera_param::LOOKAHEAD_MIN);
        bn::fixed max_distance = tuning(camera_param::LOOKAHEAD_MAX);
        bn::fixed t;

        if(sprint_speed - walk_speed >= min_divisor())
        {
            t = bn::clamp((target.speed() - walk_speed) / (sprint_speed - walk_speed), bn::fixed(0), bn::fixed(1));
        }

        bn::fixed distance = min_distance + ((max_distance - min_distance) * t);

        if(input_x && input_y)
        {
            distance *= tuning(move_param::DIAGONAL);
        }

        desired_lookahead = bn::fixed_point(distance * input_x, distance * input_y);
    }

    int lookahead_ease = bn::max(tuning(camera_param::LOOKAHEAD_EASE).integer(), 1);
    _lookahead += (desired_lookahead - _lookahead) / lookahead_ease;

    // Aiming moves the camera further along the aim, eased in and back out (milestone 13).
    int aim_ease = bn::max(tuning(camera_param::AIM_EASE_FRAMES).integer(), 1);

    if(target.aiming())
    {
        _aim_angle = target.aim_angle();
        _aim_frames = bn::min(_aim_frames + 1, aim_ease);
    }
    else
    {
        _aim_frames = bn::max(bn::min(_aim_frames, aim_ease) - 1, 0);
    }

    bn::fixed_point aim_shift;

    if(_aim_frames)
    {
        int angle = ((_aim_angle % 360) + 360) % 360;
        bn::fixed distance = (tuning(camera_param::AIM_SHIFT) * _aim_frames) / aim_ease;
        aim_shift = bn::fixed_point(distance * bn::degrees_lut_cos(angle), -distance * bn::degrees_lut_sin(angle));
    }

    bn::fixed_point goal = focus_point(target) + _lookahead + aim_shift;
    bn::fixed x = _position.x();
    bn::fixed y = _position.y();
    follow_axis(x, goal.x(), _following_x);
    follow_axis(y, goal.y(), _following_y);
    _position = bn::fixed_point(x, y);
    _clamp_and_apply();
}

int camera_controller::left() const
{
    return _camera.x().integer() - half_screen_width;
}

int camera_controller::top() const
{
    return _camera.y().integer() - half_screen_height;
}

void camera_controller::_clamp_and_apply()
{
    bn::fixed x = bn::clamp(_position.x(), bn::fixed(half_screen_width),
                            bn::fixed(world::width_px() - half_screen_width));
    bn::fixed y = bn::clamp(_position.y(), bn::fixed(half_screen_height),
                            bn::fixed(world::height_px() - half_screen_height));
    _position = bn::fixed_point(x, y);

    // Hit shake: a random 1-3 px offset of the world layers (spec: game feel); the HUD does not move.
    int shake_x = 0;
    int shake_y = 0;

    if(_shake_frames > 0)
    {
        --_shake_frames;
        shake_x = _random.get_int((_shake_pixels * 2) + 1) - _shake_pixels;
        shake_y = _random.get_int((_shake_pixels * 2) + 1) - _shake_pixels;
    }

    // Whole pixels only, so BG and sprites scroll together without shimmer.
    _camera.set_position(x.floor_integer() + shake_x, y.floor_integer() + shake_y);
}

void camera_controller::shake(int pixels, int frames)
{
    // Playtest 2: the SCREEN SHAKE setting.
    if(! settings::shake())
    {
        return;
    }

    if(pixels >= _shake_pixels || _shake_frames <= 0)
    {
        _shake_pixels = pixels;
        _shake_frames = bn::max(_shake_frames, frames);
    }
}

}
