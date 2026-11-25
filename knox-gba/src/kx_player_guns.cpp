#include "kx_player.h"

#include "bn_algorithm.h"
#include "bn_log.h"
#include "bn_math.h"

#include "kx_bench.h"
#include "kx_game.h"
#include "kx_guns.h"
#include "kx_hud_tiles.h"
#include "kx_input.h"
#include "kx_light.h"
#include "kx_sfx.h"
#include "kx_throwables.h"
#include "kx_ui.h"

namespace kx
{

namespace
{
    // The muzzle from the feet: at chest height, a little ahead along the aim.
    constexpr int muzzle_height = 9;
    constexpr int muzzle_ahead = 7;

    [[nodiscard]] bn::fixed_point unit(int angle)
    {
        // Degrees counterclockwise from east; screen y grows downwards.
        return bn::fixed_point(bn::degrees_lut_cos(angle), -bn::degrees_lut_sin(angle));
    }

    [[nodiscard]] int wrap(int angle)
    {
        return ((angle % 360) + 360) % 360;
    }
}

int player::gun_rounds() const
{
    const item_stack& hand = _inventory.equipped(equip_slot::HAND);
    return is_gun(hand.id) ? gun_loaded(hand) : -1;
}

int player::gun_rounds_carried() const
{
    const item_stack& hand = _inventory.equipped(equip_slot::HAND);
    return is_gun(hand.id) ? _inventory.count(ammo_of(gun_of(hand.id))) : -1;
}

bool player::_wants_aim(const input_frame& input) const
{
    if(! input.r_held || _action != player_action::NONE || _fall_timer > 0)
    {
        return false;
    }

    // A gun in hand aims it; otherwise a throwable in the off hand aims a throw.
    return is_gun(_inventory.equipped(equip_slot::HAND).id) ||
           is_throwable(_inventory.equipped(equip_slot::OFF_HAND).id);
}

void player::_end_aim()
{
    _aiming = false;
    _aim_locked = false;
    _aim_still = 0;
    _reload_length = 0;         // a reload or the clearing of a jam is dropped with the aim
    _jam_clear = 0;
}

void player::_update_aim(const input_frame& input, game& owner)
{
    item_stack& hand = _inventory.equipped(equip_slot::HAND);
    _aiming = true;

    // B belongs to the reload: no shove when it is let go, and no jog or sprint.
    _b_consumed = true;
    _b_frames = 0;
    _jog_latched = false;

    if(! is_gun(hand.id))
    {
        _update_throw(input, owner);
        return;
    }

    const gun_info& held = info(gun_of(hand.id));

    // The cone: wide while moving, narrowing while standing still; Aiming tightens it, panic widens it.
    _aim_still = input.dx || input.dy ? 0 : bn::min(_aim_still + 1, 10000);
    int moving = tuning_int(gun_param::CONE_MOVING);
    int still = tuning_int(gun_param::CONE_STILL);
    int frames = bn::max(tuning_int(gun_param::STILL_FRAMES), 1);
    int cone = moving - (((moving - still) * bn::min(_aim_still, frames)) / frames);
    cone -= _character.level(skill::AIMING) * tuning_int(gun_param::CONE_PER_LEVEL);
    cone += _body.level(moodle::PANIC) * tuning_int(gun_param::CONE_PER_PANIC);
    _aim_cone = bn::max(cone, tuning_int(gun_param::CONE_MIN));

    // The aim follows the facing, and locks on to the nearest zombie in front (spec: soft auto-aim).
    int facing = light::facing_angle(_facing_x, _facing_y);
    bn::fixed_point chest(_position.x(), _position.y() - muzzle_height);
    int range = held.range_tiles * 16;
    int target = owner.zombies().aim_target(chest, unit(facing), range, tuning_int(gun_param::LOCK_ARC));
    bn::fixed_point animal_feet;
    // No zombie in front: an animal (milestone 13: hunting).
    bool animal = target < 0 &&
                  owner.animals().aim_target(chest, unit(facing), range, tuning_int(gun_param::LOCK_ARC), animal_feet);
    _aim_locked = target >= 0 || animal;
    _aim_angle = facing;

    if(_aim_locked)
    {
        _aim_target_feet = animal ? animal_feet : owner.zombies().position(target);
        int dx = _aim_target_feet.x().integer() - chest.x().integer();
        int dy = (_aim_target_feet.y().integer() - 8) - chest.y().integer();

        if(dx || dy)
        {
            _aim_angle = wrap(bn::degrees_atan2(-dy, dx).round_integer());
        }
    }

    _aim_muzzle = chest + (unit(_aim_angle) * muzzle_ahead);

    if(_jam_clear > 0)
    {
        // Clearing a jam: the gun works again at the end.
        if(--_jam_clear == 0)
        {
            set_gun_jammed(hand, false);
            sfx::play(sfx::id::RELOAD, milli(600));
        }
    }
    else if(_reload_length > 0)
    {
        // The reload bar fills; A on it is the active reload, else it finishes at its end.
        if(input.a_pressed)
        {
            reload_press(owner);
        }
        else if(++_reload_frame >= _reload_length)
        {
            _finish_reload(owner, false);
        }
    }
    else if(input.b_pressed)
    {
        if(gun_jammed(hand))
        {
            _jam_clear = tuning_int(gun_param::CLEAR_FRAMES);
            sfx::play(sfx::id::SHOVE, milli(500));
        }
        else
        {
            _begin_reload(owner);
        }
    }
    else if((input.a_pressed || (held.automatic && input.a_held)) && _fire_cooldown <= 0)
    {
        _fire(owner);
    }
}

void player::_update_throw(const input_frame& input, game& owner)
{
    // Aimed like a gun (the nearest zombie in front within throwing range), but without a cone: the marker shows
    // where it lands, short of any wall on the way.
    _aim_cone = 0;
    int facing = light::facing_angle(_facing_x, _facing_y);
    bn::fixed_point chest(_position.x(), _position.y() - muzzle_height);
    int target = owner.zombies().aim_target(chest, unit(facing), throw_tuning::range_px,
                                            tuning_int(gun_param::LOCK_ARC));
    bn::fixed_point towards = target >= 0 ? owner.zombies().position(target) :
                                            _position + (unit(facing) * throw_tuning::range_px);
    _aim_target_feet = throw_manager::landing(_position, towards);
    _aim_locked = true;
    _aim_angle = facing;
    int dx = _aim_target_feet.x().integer() - _position.x().integer();
    int dy = _aim_target_feet.y().integer() - _position.y().integer();

    if(dx || dy)
    {
        _aim_angle = wrap(bn::degrees_atan2(-dy, dx).round_integer());
    }

    _aim_muzzle = chest + (unit(_aim_angle) * muzzle_ahead);

    if(input.a_pressed && _fire_cooldown <= 0)
    {
        _throw(owner);
    }
}

void player::_throw(game& owner)
{
    item_stack& off = _inventory.equipped(equip_slot::OFF_HAND);
    item_id id = off.id;
    owner.throws().launch(owner, id, _position, _aim_target_feet);
    off = item_stack();
    _fire_cooldown = throw_tuning::cooldown;

    // The next one of the same kind comes up from the bag.
    int slot = _inventory.find(id);

    if(slot >= 0)
    {
        static_cast<void>(_inventory.equip_from(slot));
    }
}

bool player::gun_jammed_now() const
{
    const item_stack& hand = _inventory.equipped(equip_slot::HAND);
    return is_gun(hand.id) && gun_jammed(hand);
}

void player::reload_press(game& owner)
{
    if(_reload_length <= 0)
    {
        return;
    }

    if(_reload_frame >= _sweet_start && _reload_frame < _sweet_start + _sweet_length)
    {
        _finish_reload(owner, true);
        return;
    }

    // Missed: the gun jams, the rounds stay as they were.
    #if KX_BENCH == 11
        BN_LOG("KXGUN jammed at ", _reload_frame, " of ", _reload_length, " (sweet ", _sweet_start, "-",
               _sweet_start + _sweet_length, ")");
    #endif

    item_stack& hand = _inventory.equipped(equip_slot::HAND);
    set_gun_jammed(hand, true);
    _reload_length = 0;
    ++_jams;
    sfx::play(sfx::id::WEAPON_BREAK, milli(500));
}

void player::_fire(game& owner)
{
    item_stack& hand = _inventory.equipped(equip_slot::HAND);
    gun kind = gun_of(hand.id);
    const gun_info& held = info(kind);
    int loaded = gun_loaded(hand);

    if(loaded <= 0 || gun_jammed(hand))
    {
        // Spec: every press gets a response (an empty or jammed gun clicks).
        sfx::play(sfx::id::CLICK, milli(600));
        _fire_cooldown = tuning_int(gun_param::EMPTY_CLICK_FRAMES);
        return;
    }

    set_gun_loaded(hand, loaded - 1);
    _fire_cooldown = held.cooldown;
    ++_shots_fired;

    // Each shot or pellet flies at a random angle inside the cone (and the gun's own spread).
    int spread = _aim_cone + held.spread;
    int hits = 0;
    int kills = 0;
    bool animal_hit = false;

    for(int pellet = 0; pellet < held.pellets; ++pellet)
    {
        int angle = wrap(_aim_angle + owner.random().get_int((spread * 2) + 1) - spread);
        shot_info shot;
        shot.origin = _aim_muzzle;
        shot.direction = unit(angle);
        shot.range = held.range_tiles * 16;
        shot.hit_radius = tuning_int(gun_param::HIT_RADIUS);
        shot.damage = held.damage;
        shot.knockback = milli(held.knockback_milli);
        shot.stagger_frames = tuning(zombie_param::HIT_STAGGER).integer();
        attack_result result = owner.zombies().shoot(owner, shot);
        hits += result.hits;
        kills += result.kills;

        // A pellet that missed the zombies may hit an animal (milestone 13); one hit a shot.
        if(result.hits == 0 && ! animal_hit &&
           owner.animals().shoot(owner, shot.origin, shot.direction, shot.range, kind))
        {
            animal_hit = true;
            ++hits;
        }

        // A streak for a single shot; three for a volley (its middle and both edges).
        if(held.pellets == 1)
        {
            owner.guns().shot(_aim_muzzle, angle);
        }
    }

    if(held.pellets > 1)
    {
        owner.guns().shot(_aim_muzzle, _aim_angle);
        owner.guns().shot(_aim_muzzle, wrap(_aim_angle + spread));
        owner.guns().shot(_aim_muzzle, wrap(_aim_angle - spread));
    }

    // Spec: guns win the fight in front of you and start the next one through noise.
    sfx::play(held.sound, 1, owner.screen_x(_position));
    owner.noise(_position, held.noise_tiles);
    owner.shake(held.shake, 4);
    _fight_lock = tuning(inventory_param::FIGHT_LOCK).integer();

    if(hits > 0)
    {
        ++_shots_hit;
        owner.request_hitstop(held.hitstop);
        train(owner, skill::AIMING, (hits * character_tuning::hit_xp) + (kills * character_tuning::kill_xp));
    }

    #if KX_BENCH == 11
        BN_LOG("KXGUN shot ", info(hand.id).name, " loaded=", gun_loaded(hand), " cone=", _aim_cone, " locked=",
               _aim_locked, " angle=", _aim_angle, " hits=", hits, " kills=", kills);
    #endif
}

void player::_begin_reload(game& owner)
{
    const item_stack& hand = _inventory.equipped(equip_slot::HAND);
    gun kind = gun_of(hand.id);
    const gun_info& held = info(kind);

    if(gun_loaded(hand) >= held.capacity)
    {
        sfx::play(sfx::id::DENIED, milli(500));
        return;
    }

    // The bar: shorter with Reloading; the sweet spot wider, somewhere in its middle part.
    int level = _character.level(skill::RELOADING);
    _reload_length = bn::max(int(held.reload_frames) - (level * tuning_int(gun_param::RELOAD_PER_LEVEL)),
                             tuning_int(gun_param::RELOAD_LEAST));
    _sweet_length = bn::min(tuning_int(gun_param::SWEET_FRAMES) + (level * tuning_int(gun_param::SWEET_PER_LEVEL)),
                            _reload_length / 2);
    int earliest = (_reload_length * tuning_int(gun_param::SWEET_FROM)) / 100;
    int latest = bn::max((_reload_length * tuning_int(gun_param::SWEET_TO)) / 100,
                         earliest) - _sweet_length;
    _sweet_start = earliest + owner.random().get_int(bn::max(latest - earliest, 0) + 1);
    _reload_frame = 0;

    if(_inventory.count(ammo_of(kind)) <= 0)
    {
        char text[32];
        int length = ui::append(text, 0, "NO ");
        ui::append(text, length, info(ammo_of(kind)).name);
        owner.toast(text, hud_tiles::bank_danger, 90);
        sfx::play(sfx::id::DENIED, milli(600));
        return;
    }

    sfx::play(sfx::id::RELOAD, milli(800));
}

void player::_finish_reload(game& owner, bool fast)
{
    _reload_length = 0;

    item_stack& hand = _inventory.equipped(equip_slot::HAND);

    if(! is_gun(hand.id))
    {
        return;
    }

    gun kind = gun_of(hand.id);
    int loaded = gun_loaded(hand);
    int taken = bn::min(info(kind).capacity - loaded, _inventory.count(ammo_of(kind)));
    _inventory.remove(ammo_of(kind), taken);
    set_gun_loaded(hand, loaded + taken);
    _fast_reloads += fast;

    if(fast)
    {
        sfx::play(sfx::id::CHARGE_READY, milli(600));
    }

    train(owner, skill::RELOADING, fast ? character_tuning::fast_reload_xp : character_tuning::reload_xp);

    #if KX_BENCH == 11
        BN_LOG("KXGUN reloaded ", info(hand.id).name, fast ? " fast at " : " at the end, ", _reload_frame, " (sweet ",
               _sweet_start, "-", _sweet_start + _sweet_length, ") loaded=", gun_loaded(hand), " left=",
               _inventory.count(ammo_of(kind)));
    #endif
}

}
