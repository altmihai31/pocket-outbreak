#include "kx_player.h"

#include "bn_algorithm.h"
#include "bn_math.h"

#include "bn_sprite_items_player.h"
#include "bn_sprite_items_ripple.h"
#include "bn_sprite_items_weapons.h"

#include "kx_bench.h"
#include "kx_flash.h"
#include "kx_hud_tiles.h"
#include "kx_fx.h"
#include "kx_game.h"
#include "kx_input.h"
#include "kx_loot.h"
#include "kx_movement_tuning.h"
#include "kx_outfit.h"
#include "kx_save.h"
#include "kx_ui.h"
#include "kx_sfx.h"
#include "kx_sprite_frames.h"
#include "kx_vector.h"
#include "kx_vehicles.h"
#include "kx_guns.h"
#include "kx_weapon_frames.h"
#include "kx_world.h"

namespace kx
{

namespace
{
    // Frame layout of graphics/player.bmp: see kx_sprite_frames.h (generated).
    constexpr int frame_down = 0;
    constexpr int frame_up = sprite_frames::player_poses;
    constexpr int frame_side = sprite_frames::player_poses * 2;

    constexpr int sprite_bg_priority = 2;   // in front of walls (BG2), behind canopies (BG1)
    constexpr int sprite_feet_offset = 8;   // 16x16 sprite centre sits 8 px above the feet line
    constexpr int fx_feet_offset = 2;
    constexpr int knock_keep_percent = 85;  // a knockback keeps this much of its push each frame
    constexpr int knock_stop_milli = 150;   // and stops below this (px per frame)

    constexpr const char* mode_names[] = { "IDLE", "CROUCH", "WALK", "JOG", "SPRINT", "DOWN" };

    [[nodiscard]] bn::fixed approach(bn::fixed value, bn::fixed target, bn::fixed step)
    {
        if(value < target)
        {
            return bn::min(value + step, target);
        }

        return bn::max(value - step, target);
    }

    [[nodiscard]] bn::fixed mode_speed(move_mode mode)
    {
        switch(mode)
        {

        case move_mode::CROUCH:
            return tuning(move_param::CROUCH_SPEED);

        case move_mode::WALK:
            return tuning(move_param::WALK_SPEED);

        case move_mode::JOG:
            return tuning(move_param::JOG_SPEED);

        case move_mode::SPRINT:
            return tuning(move_param::SPRINT_SPEED);

        default:
            return 0;
        }
    }

    [[nodiscard]] int mode_noise(move_mode mode)
    {
        switch(mode)
        {

        case move_mode::CROUCH:
            return tuning(move_param::CROUCH_NOISE).integer();

        case move_mode::WALK:
            return tuning(move_param::WALK_NOISE).integer();

        case move_mode::JOG:
            return tuning(move_param::JOG_NOISE).integer();

        case move_mode::SPRINT:
            return tuning(move_param::SPRINT_NOISE).integer();

        default:
            return 0;
        }
    }

    // Accelerates towards target, or brakes (stop rate) when the target is slower or reversed.
    [[nodiscard]] bn::fixed step_axis(bn::fixed velocity, bn::fixed target, bn::fixed accel_step,
                                      bn::fixed brake_step)
    {
        bool braking = (velocity > 0 && target < velocity) || (velocity < 0 && target > velocity);
        return approach(velocity, target, braking ? brake_step : accel_step);
    }
}

const char* mode_name(move_mode mode)
{
    return mode_names[int(mode)];
}

player::player(const bn::camera_ptr& camera, const bn::fixed_point& feet_position) :
    _sprite(bn::sprite_items::player.create_sprite(0, 0)),
    _normal_palette(bn::sprite_items::player.palette_item().create_new_palette()),
    _flash_palette(flash_palette_item.create_palette()),
    _held_palette(bn::sprite_items::weapons.palette_item().create_new_palette()),
    _position(feet_position)
{
    _sprite.set_camera(camera);
    _sprite.set_bg_priority(sprite_bg_priority);
    _sprite.set_palette(_normal_palette);
    _character.start(occupation::UNEMPLOYED, 0);
    give_starting_kit();
    _ground = world::terrain_at(_position.x().floor_integer() >> world::tile_shift,
                                (_position.y().floor_integer() - 1) >> world::tile_shift);
    _update_animation(nullptr);
}

void player::train(game& owner, skill value, int amount)
{
    int mood_percent = character_tuning::unhappy_xp_percent[_body.level(moodle::UNHAPPY)];

    if(_dead || ! _character.add_xp(value, amount, mood_percent))
    {
        return;
    }

    char text[32];
    int length = ui::append(text, 0, info(value).name);
    length = ui::append(text, length, " LEVEL ");
    ui::append_int(text, length, _character.level(value));
    owner.toast(text, hud_tiles::bank_accent, 120);
    sfx::play(sfx::id::MOODLE, milli(800));
}

void player::update(const input_frame& input, game& owner)
{
    _inventory.set_capacity_bonus(_character.capacity_bonus());

    if(_dead)
    {
        _velocity = bn::fixed_point();
        _speed = 0;
        _noise_radius = 0;
        _input_x = 0;
        _input_y = 0;
        _mode = move_mode::FALLEN;
        _update_animation(&owner);
        return;
    }

    if(_invulnerable > 0)
    {
        --_invulnerable;
    }

    if(_shove_cooldown > 0)
    {
        --_shove_cooldown;
    }

    if(_ride == ride_kind::CAR)
    {
        // The car carries the survivor (carried()): no walking, swinging or crouching in the seat.
        _crouched = false;
        _mode = move_mode::IDLE;
        _update_animation(&owner);
        return;
    }

    if(_ride == ride_kind::BICYCLE)
    {
        // A or L1 gets off; there is no swinging, shoving or crouching from the saddle.
        if(input.a_pressed || input.l_pressed)
        {
            _ride = ride_kind::NONE;
            _l_combo = true;
            sfx::play(sfx::id::CHAIN, milli(500));
            _update_movement(0, 0, false, owner);
        }
        else
        {
            _crouched = false;
            _update_movement(input.dx, input.dy, false, owner);
        }

        return;
    }

    if(_fire_cooldown > 0)
    {
        --_fire_cooldown;
    }

    // Aiming a gun takes the buttons: A fires, B reloads (no shove, no context action, no crouch toggle).
    // Swimming (milestone 13) takes them all: no attacks, no aiming, no items, no context action.
    if(_swimming)
    {
        _end_aim();
        _crouched = false;
        _sea_message = bn::max(_sea_message - 1, 0);
        _body.soak();

        if(_action != player_action::NONE && _action != player_action::GRABBED)
        {
            _action = player_action::NONE;
        }
    }
    else if(_wants_aim(input))
    {
        _update_aim(input, owner);
    }
    else
    {
        _end_aim();
        _update_buttons(input, owner);
        _update_action(input, owner);
    }

    // Committed actions: no walking while swinging, shoving, stomping or grabbed, but the swing can still be
    // aimed during the wind-up and charge.
    bool busy = _action != player_action::NONE;

    if((_action == player_action::WINDUP || _action == player_action::CHARGE) && (input.dx || input.dy))
    {
        _facing_x = input.dx;
        _facing_y = input.dy;
    }

    bool sprint_held = ! _aiming && ! _swimming && input.b_held &&
                       _b_frames > tuning(combat_param::TAP_FRAMES).integer();
    _update_movement(busy ? 0 : input.dx, busy ? 0 : input.dy, sprint_held, owner);
}

void player::set_ride(ride_kind value)
{
    _ride = value;
    _crouched = false;
    _jog_latched = false;
    _braking = false;

    if(value == ride_kind::CAR)
    {
        _velocity = bn::fixed_point();
        _speed = 0;
    }
}

void player::carried(const bn::fixed_point& feet, const bn::fixed_point& velocity, int noise_tiles)
{
    _position = feet;
    _velocity = bn::fixed_point();
    _speed = approx_length(velocity);
    _input_x = velocity.x() > milli(500) ? 1 : velocity.x() < -milli(500) ? -1 : 0;
    _input_y = velocity.y() > milli(500) ? 1 : velocity.y() < -milli(500) ? -1 : 0;
    _noise_radius = noise_tiles;
    _ground = world::terrain_at(_position.x().floor_integer() >> world::tile_shift,
                                (_position.y().floor_integer() - 1) >> world::tile_shift);
}

void player::_update_movement(int dx, int dy, bool sprint_held, game& owner)
{
    bool riding = _ride == ride_kind::BICYCLE;

    if(_fall_timer > 0)
    {
        --_fall_timer;
        dx = 0;
        dy = 0;
    }

    bool has_input = dx || dy;
    // Exhaustion level 3 and up takes the sprint away (spec: Exhausted); so does swimming.
    bool sprinting = has_input && sprint_held && _body.can_sprint() && ! riding && ! _swimming;

    if(_swimming)
    {
        _crouched = false;
        _jog_latched = false;
    }
    _update_double_tap(dx, dy);
    _input_x = dx;
    _input_y = dy;

    if(has_input)
    {
        _facing_x = dx;
        _facing_y = dy;
    }

    if(sprinting)
    {
        _crouched = false;
    }

    // Mode.
    if(_fall_timer > 0)
    {
        _mode = move_mode::FALLEN;
    }
    else if(sprinting)
    {
        _mode = move_mode::SPRINT;
    }
    else if(_crouched)
    {
        _mode = move_mode::CROUCH;
    }
    else if(! has_input)
    {
        _mode = move_mode::IDLE;
    }
    else
    {
        _mode = _jog_latched || riding ? move_mode::JOG : move_mode::WALK;
    }

    // Target velocity from mode, terrain and the diagonal correction.
    _ground = world::terrain_at(_position.x().floor_integer() >> world::tile_shift,
                                (_position.y().floor_integer() - 1) >> world::tile_shift);

    const terrain_info& ground_info = info(_ground);
    bn::fixed base_speed = mode_speed(_mode);
    bool smooth = _ground == terrain::ROAD || _ground == terrain::PAVEMENT || _ground == terrain::FLOOR;

    if(riding && has_input)
    {
        // Spec: fast on roads and paths, poor off-road.
        base_speed = milli(smooth ? vehicle_tuning::bike_speed_milli :
                           _ground == terrain::SHALLOW_WATER ? vehicle_tuning::bike_water_milli :
                                                               vehicle_tuning::bike_rough_milli);
    }

    // Swimming (milestone 13): a fraction of walking speed whatever the gait.
    if(_swimming)
    {
        base_speed = tuning(move_param::WALK_SPEED) * tuning(move_param::SWIM_MULT);
    }

    // Heavy load (spec: above capacity you get Heavy load: slower movement).
    base_speed *= _body.speed_multiplier();

    // Spec: aiming drops movement to half speed.
    if(_aiming)
    {
        base_speed *= gun_tuning().get(int(gun_param::AIM_SPEED));
    }

    if(_inventory.overloaded())
    {
        base_speed *= tuning(inventory_param::OVERLOAD_MULT);
    }
    else if(_inventory.heavy())
    {
        base_speed *= tuning(inventory_param::HEAVY_MULT);
    }

    bn::fixed target_speed = base_speed;

    if(ground_info.speed_param != terrain_info::NONE && ! riding)
    {
        target_speed *= movement_tuning().get(ground_info.speed_param);
    }

    bn::fixed axis_speed = dx && dy ? target_speed * tuning(move_param::DIAGONAL) : target_speed;
    int accel_frames = bn::max(tuning(move_param::ACCEL_FRAMES).integer(), 1);
    int stop_frames = bn::max(tuning(move_param::STOP_FRAMES).integer(), 1);

    if(riding)
    {
        // A bicycle picks up speed and rolls to a stop.
        accel_frames = vehicle_tuning::bike_accel_frames;
        stop_frames = vehicle_tuning::bike_stop_frames;
    }

    if(has_input)
    {
        _braking = false;

        bn::fixed accel_step = axis_speed / accel_frames;
        bn::fixed brake_step = base_speed / stop_frames;
        _velocity.set_x(step_axis(_velocity.x(), axis_speed * dx, accel_step, brake_step));
        _velocity.set_y(step_axis(_velocity.y(), axis_speed * dy, accel_step, brake_step));
    }
    else
    {
        // Constant braking rate fixed when the stop starts, so every stop takes STOP_FRAMES frames.
        if(! _braking)
        {
            _braking = true;
            _brake_step = bn::max(bn::max(bn::abs(_velocity.x()), bn::abs(_velocity.y())) / stop_frames, milli(50));
        }

        _velocity.set_x(approach(_velocity.x(), 0, _brake_step));
        _velocity.set_y(approach(_velocity.y(), 0, _brake_step));
    }

    // Collision, one axis at a time. Corner sliding only when pushing straight along one axis.
    bn::fixed_point before = _position;
    bn::fixed velocity_x = _velocity.x();
    bn::fixed velocity_y = _velocity.y();
    bn::fixed impact_speed;

    if(velocity_x != 0)
    {
        bn::fixed speed_x = bn::abs(velocity_x);

        if(_move_axis(true, velocity_x, dy == 0))
        {
            impact_speed = speed_x;
        }
    }

    if(velocity_y != 0)
    {
        bn::fixed speed_y = bn::abs(velocity_y);

        if(_move_axis(false, velocity_y, dx == 0))
        {
            impact_speed = bn::max(impact_speed, speed_y);
        }
    }

    _velocity.set_x(velocity_x);
    _velocity.set_y(velocity_y);

    // Thrown back (a boar's gore): a push of its own, fading each frame, stopped by walls like any step.
    if(_knock != bn::fixed_point())
    {
        bn::fixed knock_x = _knock.x();
        bn::fixed knock_y = _knock.y();

        if(knock_x != 0 && _move_axis(true, knock_x, false))
        {
            _knock.set_x(0);
        }

        if(knock_y != 0 && _move_axis(false, knock_y, false))
        {
            _knock.set_y(0);
        }

        _knock = (_knock * knock_keep_percent) / 100;

        if(bn::abs(_knock.x()) + bn::abs(_knock.y()) < milli(knock_stop_milli))
        {
            _knock = bn::fixed_point();
        }
    }

    // Deep water (milestone 13): in it you swim; too far from land the water pushes you back where you were.
    {
        int tile_x = _position.x().floor_integer() >> world::tile_shift;
        int tile_y = (_position.y().floor_integer() - 1) >> world::tile_shift;
        bool moved_tile = tile_x != (before.x().floor_integer() >> world::tile_shift) ||
                          tile_y != ((before.y().floor_integer() - 1) >> world::tile_shift);

        // The ground below only changes with the tile (or a teleport, which _ground catches next frame).
        bool deep = ! riding && (moved_tile || _ground == terrain::DEEP_WATER || _swimming ?
                                 world::terrain_at(tile_x, tile_y) == terrain::DEEP_WATER : false);

        if(deep && moved_tile && ! _near_land(tile_x, tile_y))
        {
            _position = before;
            _velocity = bn::fixed_point();

            if(! _sea_message)
            {
                owner.toast("TOO FAR FROM LAND", hud_tiles::bank_danger, 90);
                _sea_message = 180;
            }
        }
        else if(deep != _swimming)
        {
            _swimming = deep;
            _crouched = false;
            _anim_step = 0;
            _stride = 0;
            owner.fx().spawn(fx_kind::SPLASH, bn::fixed_point(_position.x(), _position.y() - fx_feet_offset));
            sfx::play(sfx::id::STEP_WATER, milli(800));
        }
    }

    // Sprinting head-on into an obstacle knocks the player down; glancing off one diagonally does not,
    // because only the speed along the blocked axis counts.
    int fall_frames = tuning(move_param::FALL_FRAMES).integer();

    // Sprinting over a downed or crawling zombie trips the player too (spec: Fall-over).
    bool tripped = _mode == move_mode::SPRINT && owner.zombies().crawler_underfoot(_position);

    if(_mode == move_mode::SPRINT && fall_frames > 0 &&
            (tripped || (impact_speed > 0 && impact_speed >= tuning(move_param::FALL_MIN_SPEED))))
    {
        _fall_timer = fall_frames;

        // Falls can break a leg (spec: Fracture: falls).
        if(owner.random().get_int(100) < tuning(survival_param::FRACTURE_CHANCE))
        {
            _body.add_wound(owner.random().get_int(2) ? body_part::LEFT_LEG : body_part::RIGHT_LEG,
                            wound_type::FRACTURE, false, owner.random());
            owner.toast("FRACTURE: LEG", hud_tiles::bank_danger, 120);
        }
        _mode = move_mode::FALLEN;
        _velocity = bn::fixed_point();
        _crouched = false;
        owner.fx().spawn(fx_kind::DUST, bn::fixed_point(_position.x(), _position.y() - fx_feet_offset));
        sfx::play(sfx::id::STEP_SOFT, 1);
    }

    _speed = approx_length(_velocity.x(), _velocity.y());
    _distance += _speed;

    // Spec: you fall off if you hit a zombie (or ride over one on the ground).
    if(riding && _speed >= milli(vehicle_tuning::bike_crash_milli) && _fall_timer <= 0 &&
       (owner.zombies().standing_within(_position, vehicle_tuning::bike_crash_px) ||
        owner.zombies().crawler_underfoot(_position)))
    {
        _ride = ride_kind::NONE;
        _fall_timer = bn::max(fall_frames, 30);
        _mode = move_mode::FALLEN;
        _velocity = bn::fixed_point();
        _speed = 0;
        owner.toast("YOU FALL OFF THE BICYCLE", hud_tiles::bank_danger, 90);
        owner.fx().spawn(fx_kind::DUST, bn::fixed_point(_position.x(), _position.y() - fx_feet_offset));
        owner.shake(2, 8);
        sfx::play(sfx::id::HIT_BLUNT, milli(800));
        riding = false;
    }

    if(_swimming)
    {
        // Treading water tires you too (milestone 13).
        _exertion += tuning(survival_param::SWIM_EXERTION);
    }
    else if(riding && _speed > 0)
    {
        _exertion += vehicle_tuning::bike_exertion;
    }
    else if(_mode == move_mode::SPRINT && _speed > 0)
    {
        _exertion += tuning(survival_param::SPRINT_EXERTION);
    }
    else if(_mode == move_mode::JOG && _speed > 0)
    {
        _exertion += tuning(survival_param::JOG_EXERTION);
    }

    if(_speed > 0 && _mode != move_mode::FALLEN)
    {
        // While braking the mode is IDLE, so keep reporting the noise of the gait being slowed from.
        move_mode noise_mode = _mode == move_mode::IDLE ? move_mode::WALK : _mode;
        _noise_radius = mode_noise(noise_mode);

        // Footsteps: Lightfoot and the Graceful / Clumsy traits (crouching stays as quiet as it is).
        if(_noise_radius > 0 && noise_mode != move_mode::CROUCH)
        {
            _noise_radius = bn::max(_noise_radius + _character.footstep_noise_change(), 1);
        }

        if(ground_info.noise_param != terrain_info::NONE)
        {
            _noise_radius += movement_tuning().get(ground_info.noise_param).integer();
        }

        if(_swimming)
        {
            _noise_radius += tuning(move_param::SWIM_NOISE).integer();
        }

        if(riding)
        {
            _noise_radius = vehicle_tuning::bike_noise;     // spec: quiet
        }
    }
    else
    {
        _noise_radius = 0;
    }

    _update_animation(&owner);
}

void player::_update_double_tap(int dx, int dy)
{
    // Double-tap: a short press, a release, then a press in a related direction within the window
    // starts a jog that lasts until every direction is released. Called before _input_x/y are updated,
    // so they still hold last frame's direction.
    int window = _ride == ride_kind::NONE ? tuning(move_param::DOUBLE_TAP_FRAMES).integer() : 0;
    bool has_input = dx || dy;
    bool had_input = _input_x || _input_y;

    if(has_input)
    {
        if(! had_input)
        {
            bool same_direction = (dx && dx == _tap_x) || (dy && dy == _tap_y);
            _jog_latched = window > 0 && _tap_valid && _release_frames <= window && same_direction;
            _tap_valid = false;
            _press_frames = 0;
        }

        _press_frames = bn::min(_press_frames + 1, 1000);
    }
    else
    {
        if(had_input)
        {
            _tap_valid = ! _jog_latched && _press_frames <= window;
            _tap_x = _input_x;
            _tap_y = _input_y;
            _release_frames = 0;
        }

        _release_frames = bn::min(_release_frames + 1, 1000);
        _jog_latched = false;
    }
}

bool player::_blocked_at(const bn::fixed_point& feet) const
{
    bn::fixed half_width = tuning(move_param::HITBOX_W) / 2;
    bn::fixed height = tuning(move_param::HITBOX_H);
    int left = (feet.x() - half_width).floor_integer();
    int right = (feet.x() + half_width).ceil_integer() - 1;
    int top = (feet.y() - height).floor_integer();
    int bottom = feet.y().ceil_integer() - 1;

    #if KX_BENCH == 7 || KX_BENCH == 8
        // Travel and save benches: walk through everything, so the route crosses chunks whatever the seed puts there.
        static_cast<void>(left + right + top + bottom);
        return false;
    #else
        return world::box_blocked(left, top, right, bottom, _swim_allowed());
    #endif
}

bool player::_swim_allowed() const
{
    // On foot; a heavy load keeps you out of deep water, but never traps you in it.
    return _ride == ride_kind::NONE && (_swimming || _body.level(moodle::HEAVY) < swim_tuning::heavy_level);
}

bool player::_near_land(int tile_x, int tile_y)
{
    constexpr int directions[8][2] = { { 1, 0 }, { 1, 1 }, { 0, 1 }, { -1, 1 }, { -1, 0 }, { -1, -1 }, { 0, -1 },
                                       { 1, -1 } };

    for(const auto& direction : directions)
    {
        for(int step = 1; step <= swim_tuning::land_tiles; ++step)
        {
            int x = tile_x + (direction[0] * step);
            int y = tile_y + (direction[1] * step);

            if(! world::loaded(x, y))
            {
                break;
            }

            if(world::terrain_at(x, y) != terrain::DEEP_WATER)
            {
                return true;
            }
        }
    }

    return false;
}

bool player::_move_axis(bool horizontal, bn::fixed& velocity, bool slide_allowed)
{
    bn::fixed_point step = horizontal ? bn::fixed_point(velocity, 0) : bn::fixed_point(0, velocity);
    bn::fixed_point target = _position + step;

    if(! _blocked_at(target))
    {
        _position = target;
        return false;
    }

    // Corner sliding: if a small perpendicular shift opens the way, nudge towards it.
    if(slide_allowed)
    {
        int reach = tuning(move_param::SLIDE_REACH).integer();
        bn::fixed slide_speed = tuning(move_param::SLIDE_SPEED);

        for(int offset = 1; offset <= reach; ++offset)
        {
            for(int sign = -1; sign <= 1; sign += 2)
            {
                bn::fixed shift = offset * sign;
                bn::fixed_point probe = target + (horizontal ? bn::fixed_point(0, shift) : bn::fixed_point(shift, 0));

                if(! _blocked_at(probe))
                {
                    bn::fixed nudge = bn::min(slide_speed, bn::fixed(offset)) * sign;
                    bn::fixed_point nudged = _position +
                            (horizontal ? bn::fixed_point(0, nudge) : bn::fixed_point(nudge, 0));

                    if(! _blocked_at(nudged))
                    {
                        _position = nudged;
                        return false;
                    }
                }
            }
        }
    }

    // Blocked: move flush against the tile edge and stop on this axis.
    bn::fixed half_extent = horizontal ? tuning(move_param::HITBOX_W) / 2 : bn::fixed(0);
    bn::fixed position = horizontal ? _position.x() : _position.y();
    bn::fixed target_position = horizontal ? target.x() : target.y();
    bn::fixed snapped = position;

    if(velocity > 0)
    {
        int leading_px = (target_position + half_extent).ceil_integer() - 1;
        bn::fixed edge = (leading_px >> world::tile_shift) << world::tile_shift;
        snapped = bn::max(position, edge - half_extent);
    }
    else
    {
        bn::fixed trailing = horizontal ? half_extent : tuning(move_param::HITBOX_H);
        int leading_px = (target_position - trailing).floor_integer();
        bn::fixed edge = ((leading_px >> world::tile_shift) + 1) << world::tile_shift;
        snapped = bn::min(position, edge + trailing);
    }

    bn::fixed_point snapped_point = horizontal ? bn::fixed_point(snapped, _position.y()) :
                                                 bn::fixed_point(_position.x(), snapped);

    if(! _blocked_at(snapped_point))
    {
        _position = snapped_point;
    }

    velocity = 0;
    return true;
}

void player::_update_animation(game* owner)
{
    // Out of sight in a car.
    bool in_car = _ride == ride_kind::CAR;

    if(_sprite.visible() == in_car)
    {
        _sprite.set_visible(! in_car);
    }

    if(in_car)
    {
        _sprite.set_position(_position.x().floor_integer(), _position.y().floor_integer() - sprite_feet_offset);
        return;
    }

    int frame;
    int bob = 0;
    int base = _facing_x ? frame_side : (_facing_y < 0 ? frame_up : frame_down);

    if(_mode == move_mode::FALLEN)
    {
        frame = sprite_frames::player_fallen;
        _anim_step = 0;
        _stride = 0;
    }
    else if(_action != player_action::NONE)
    {
        // Attack poses (spec: a visible pose shows full charge; see update_visuals for the charge flash).
        int pose = sprite_frames::player_stand;

        switch(_action)
        {

        case player_action::WINDUP:
        case player_action::CHARGE:
            pose = sprite_frames::player_windup;
            break;

        case player_action::ACTIVE:
        case player_action::SHOVE:
            pose = sprite_frames::player_strike;
            break;

        case player_action::RECOVERY:
            pose = _action_timer * 2 > _action_length ? sprite_frames::player_strike : sprite_frames::player_stand;
            break;

        case player_action::STOMP:
            pose = sprite_frames::player_step_a;
            bob = _action_timer * 3 > _action_length * 2 ? -2 : 0;
            break;

        default:
            break;
        }

        frame = base + pose;
        _anim_step = 0;
        _stride = 0;
    }
    else if(_swimming)
    {
        // Swimming (milestone 13): the body cut at the waterline, two strokes as you go; a splash on each.
        int block = _facing_x ? 2 : (_facing_y < 0 ? 1 : 0);
        int stride = bn::max(tuning(move_param::STRIDE_PX).integer(), 2) * 2;
        _stride += _speed;

        if(_stride >= stride)
        {
            _stride = 0;
            _anim_step = (_anim_step + 1) & 1;

            if(owner && _anim_step)
            {
                sfx::play(sfx::id::STEP_WATER, milli(450));
                owner->fx().spawn(fx_kind::SPLASH, bn::fixed_point(_position.x(), _position.y() - fx_feet_offset));
            }
        }

        frame = sprite_frames::player_swim + (block * sprite_frames::player_swim_strokes) + (_anim_step & 1);
    }
    else if(_ride == ride_kind::BICYCLE)
    {
        // In the saddle: sitting low, lifted over the frame; the chain ticks as it rolls.
        frame = base + sprite_frames::player_crouch;
        bob = -3;
        _anim_step = 0;
        _stride += _speed;

        if(_stride >= 48)
        {
            _stride = 0;
            sfx::play(sfx::id::CHAIN, milli(250));
        }
    }
    else
    {
        if(_speed > 0)
        {
            int stride = bn::max(tuning(move_param::STRIDE_PX).integer(), 2);
            _stride += _speed;

            while(_stride >= stride)
            {
                _stride -= stride;
                _anim_step = (_anim_step + 1) & 3;

                // Footfall on the two step poses: per-terrain footstep sound, dust and splashes.
                if(owner && (_anim_step & 1))
                {
                    bn::fixed_point feet(_position.x(), _position.y() - fx_feet_offset);
                    bn::fixed volume = _mode == move_mode::CROUCH ? milli(200) :
                                       (_mode == move_mode::SPRINT ? milli(700) :
                                       (_mode == move_mode::JOG ? milli(500) : milli(350)));

                    if(_ground == terrain::SHALLOW_WATER)
                    {
                        sfx::play(sfx::id::STEP_WATER, volume);

                        if(_mode != move_mode::CROUCH)
                        {
                            owner->fx().spawn(fx_kind::SPLASH, feet);
                        }
                    }
                    else
                    {
                        bool hard = _ground == terrain::ROAD || _ground == terrain::PAVEMENT ||
                                    _ground == terrain::FLOOR;
                        sfx::play(hard ? sfx::id::STEP_HARD : sfx::id::STEP_SOFT, volume);

                        if(_mode == move_mode::SPRINT)
                        {
                            owner->fx().spawn(fx_kind::DUST, feet);
                        }
                    }
                }
            }

            // 1 px bob: the body rises on the passing (stand) poses.
            bob = (_anim_step & 1) ? 0 : -1;
        }
        else
        {
            _anim_step = 0;
            _stride = 0;
        }

        int pose = _anim_step == 1 ? sprite_frames::player_step_a :
                   (_anim_step == 3 ? sprite_frames::player_step_b : sprite_frames::player_stand);

        if(_crouched)
        {
            pose = sprite_frames::player_crouch;
        }

        frame = base + pose;

        // A long item rests on the shoulder: the same poses with the weapon arm raised to hold it. A gun is held
        // in the hands, aimed or at low ready (playtest 3).
        int block = _facing_x ? 2 : (_facing_y < 0 ? 1 : 0);

        if(shouldering())
        {
            frame = sprite_frames::player_carry + (block * sprite_frames::player_carry_poses) + pose;
        }
        else if(int guns = gun_frames(); guns >= 0)
        {
            frame = guns + (block * sprite_frames::player_carry_poses) + pose;
        }
    }

    if(frame != _sprite_frame)
    {
        _sprite_frame = frame;
        _sprite.set_tiles(bn::sprite_items::player.tiles_item(), frame);
    }

    _update_outfit();

    // Struggling in a grab: the body shakes.
    int jitter = _action == player_action::GRABBED ? ((_grab_timer >> 1) & 1) : 0;
    int y = _position.y().floor_integer();
    _sprite.set_horizontal_flip(_facing_x < 0);
    _sprite.set_position(_position.x().floor_integer() + jitter, y - sprite_feet_offset + bob);
}

void player::_update_outfit()
{
    // Clothes show through palette swaps (spec cut list): each worn item colours its areas of the sprite; with
    // nothing worn there, the area shows skin (underwear on the hips). Without a bag the back is the torso.
    int key = 0;

    for(int slot = int(equip_slot::BACK); slot < int(equip_slot::COUNT); ++slot)
    {
        key = (key * 67) + int(_inventory.equipped(equip_slot(slot)).id) + 1;
        key &= 0x3FFFFFFF;
    }

    if(key == _outfit_key)
    {
        return;
    }

    _outfit_key = key;

    bn::color colors[16];
    bn::span<const bn::color> defaults = bn::sprite_items::player.palette_item().colors_ref();

    for(int index = 0; index < 16; ++index)
    {
        colors[index] = defaults[index];
    }

    for(const outfit::area_colour& value : outfit::bare)
    {
        colors[value.index] = value.colour;
    }

    auto wear = [&colors](item_id id)
    {
        for(const outfit::look& value : outfit::looks)
        {
            if(value.item == id)
            {
                for(int index = 0; index < value.count; ++index)
                {
                    colors[value.colours[index].index] = value.colours[index].colour;
                }
            }
        }
    };

    for(int slot = int(equip_slot::HEAD); slot < int(equip_slot::COUNT); ++slot)
    {
        wear(_inventory.equipped(equip_slot(slot)).id);
    }

    colors[outfit::bag] = colors[outfit::torso];
    wear(_inventory.equipped(equip_slot::BACK).id);
    _normal_palette.set_colors(colors);

    // The hand gripping a held item matches the hands (skin or gloves).
    _held_palette.set_color(weapon_frames::hand_colour, colors[outfit::hands]);
}

void player::save(save::writer& out) const
{
    out.put(_position.x().data());
    out.put(_position.y().data());
    out.put(_facing_x);
    out.put(_facing_y);
    out.put(_distance.data());
    out.put(_selected);
    out.put(_inventory);
    out.put(_body);
    out.put(_character);
}

void player::load(save::reader& in)
{
    int x = in.get<int>();
    int y = in.get<int>();
    _position = bn::fixed_point(bn::fixed::from_data(x), bn::fixed::from_data(y));
    _velocity = bn::fixed_point();
    in.get(_facing_x);
    in.get(_facing_y);
    _distance = bn::fixed::from_data(in.get<int>());
    in.get(_selected);
    in.get(_inventory);
    in.get(_body);
    in.get(_character);
    _ride = ride_kind::NONE;
    _inventory.set_capacity_bonus(_character.capacity_bonus());
    _outfit_key = -1;
}

}
