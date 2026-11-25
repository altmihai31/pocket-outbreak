#include "kx_player.h"
#include "kx_light.h"

#include "bn_sprite_items_ripple.h"
#include "bn_sprite_items_weapons.h"

#include "kx_game.h"
#include "kx_hud_tiles.h"
#include "kx_loot.h"
#include "kx_input.h"
#include "kx_sfx.h"
#include "kx_vector.h"
#include "kx_sprite_frames.h"
#include "kx_weapon_frames.h"
#include "kx_world_map.h"

namespace kx
{

namespace
{
    constexpr int grab_meter_full = 100;
    constexpr int grab_invulnerable_frames = 45;    // after breaking free, so the same zombies can't re-grab at once
    constexpr int stomp_impact_divisor = 3;         // the stomp lands a third of the way through
    constexpr int charge_blink_frames = 8;

    // How a held item sits (sprites in tools/assets/weapons.py): the hand relative to the body sprite's centre,
    // the item's angle (32 steps, 0 right, 8 down) and whether it is drawn behind the body. Per facing: down, side
    // (facing right; mirrored for left), up. Long items rest on the shoulder, held by the raised hand of the carry
    // frames (behind the body from the side, so the hand at the chest shows over the handle); short ones hang low
    // in the hand. Swings start from the raised hand the sprite draws and sweep from the strike hand.
    struct hold_pose
    {
        int8_t hand_x;
        int8_t hand_y;
        int8_t angle;
        bool behind;
    };

    constexpr hold_pose shoulder_poses[] = { { 4, 0, 26, false }, { 1, 0, 21, true }, { -5, 0, 25, false } };
    constexpr hold_pose low_poses[] = { { 4, 3, 9, false }, { 2, 3, 9, false }, { -5, 3, 8, true } };
    constexpr hold_pose windup_hands[] = { { 4, -1, 0, true }, { -4, -2, 0, true }, { -5, -1, 0, false } };
    constexpr hold_pose strike_hands[] = { { 3, 4, 0, false }, { 3, 1, 0, false }, { -4, 4, 0, true } };

    // The off hand's item (a flashlight), held out along the facing in the other hand: facing down it is the left
    // of the picture, from the side the hand in front, facing up behind the body on the right.
    constexpr hold_pose off_poses[] = { { -5, 2, 0, false }, { 4, 1, 0, false }, { 4, 2, 0, true } };

    // Guns (playtest 3, mockup 21), held in the hand: at rest a pistol or revolver hangs in the hand of the standing
    // body, pointing down and forward; a long gun is at low ready (the player_ready frames), across the body, the
    // muzzle down and forward; aimed, it sits at the hands of the player_aim frames (at the aim's angle): facing
    // down at the right of the chest, the stock at the right shoulder; from the side at the end of the arm held
    // out; facing up at the right shoulder, behind the head.
    constexpr hold_pose gun_rest_poses[] = { { 3, 3, 8, false }, { 0, 3, 3, false }, { -4, 3, 8, true } };
    constexpr hold_pose gun_ready_poses[] = { { 2, 2, 10, false }, { 0, 2, 2, false }, { -2, 2, 12, true } };
    constexpr hold_pose gun_aim_hands[] = { { 3, 3, 0, false }, { 5, 1, 0, false }, { 3, -2, 0, true } };

    // Swing angles relative to the facing, in 32 steps (wind-up back over the shoulder, a sweep through the facing).
    constexpr int windup_angle = 11;
    constexpr int sweep_start = 6;
    constexpr int sweep_length = 12;
    constexpr int follow_through = 8;
    constexpr int crouch_hand_drop = 2;         // a crouched body sits 2 px lower

    constexpr int clothing_wear_per_block = 10;
    constexpr wound_type grab_wounds[] = { wound_type::SCRATCH, wound_type::LACERATION, wound_type::BITE };
    constexpr body_part grab_parts[] = { body_part::LEFT_ARM, body_part::RIGHT_ARM, body_part::TORSO };
    constexpr int eat_frames = 60;                  // spec: eating has a short animation
    constexpr int grab_panic = 30;

    // Exertion of a swing (spec: Endurance column: low / medium / high).
    [[nodiscard]] int swing_exertion(weapon value)
    {
        switch(value)
        {

        case weapon::KNIFE:
            return 25;

        case weapon::BAT:
            return 45;

        case weapon::AXE:
            return 70;

        case weapon::SPEAR:
            return 45;

        default:
            return 20;
        }
    }

    [[nodiscard]] int weapon_int(weapon value, weapon_param param)
    {
        return tuning(value, param).integer();
    }

    [[nodiscard]] int combat_int(combat_param param)
    {
        return tuning(param).integer();
    }

    // Long hand items rest on the shoulder; knives and the hammer hang low in the hand.
    [[nodiscard]] bool long_item(item_id id)
    {
        return id != item_id::KITCHEN_KNIFE && id != item_id::HUNTING_KNIFE && id != item_id::HAMMER &&
               id != item_id::PISTOL && id != item_id::REVOLVER && id != item_id::CLEAVER && id != item_id::HATCHET &&
               id != item_id::POLICE_BATON;
    }

    // Each hand item's own picture.
    [[nodiscard]] int held_frame_base(item_id id)
    {
        switch(id)
        {

        case item_id::KITCHEN_KNIFE:
        case item_id::HUNTING_KNIFE:
            return weapon_frames::knife;

        case item_id::HAMMER:
            return weapon_frames::hammer;

        case item_id::PIPE:
            return weapon_frames::pipe;

        case item_id::CROWBAR:
            return weapon_frames::crowbar;

        case item_id::AXE:
            return weapon_frames::axe;

        case item_id::FLASHLIGHT:
            return weapon_frames::flashlight;

        case item_id::PISTOL:
            return weapon_frames::pistol;

        case item_id::SHOTGUN:
        case item_id::HUNTING_RIFLE:
        case item_id::ASSAULT_RIFLE:
            return weapon_frames::long_gun;

        case item_id::SPEAR:
            return weapon_frames::spear;

        case item_id::EMPTY_BOTTLE:
            return weapon_frames::bottle;

        case item_id::MOLOTOV:
            return weapon_frames::molotov;

        case item_id::ALARM_CLOCK:
            return weapon_frames::clock;

        // Milestone 14.
        case item_id::MACHETE:
            return weapon_frames::machete;

        case item_id::HATCHET:
            return weapon_frames::hatchet;

        case item_id::GOLF_CLUB:
            return weapon_frames::golf_club;

        case item_id::SLEDGEHAMMER:
            return weapon_frames::sledgehammer;

        case item_id::PITCHFORK:
            return weapon_frames::pitchfork;

        case item_id::CLEAVER:
            return weapon_frames::cleaver;

        case item_id::HOCKEY_STICK:
            return weapon_frames::hockey_stick;

        case item_id::POLICE_BATON:
            return weapon_frames::baton;

        case item_id::REVOLVER:
            return weapon_frames::revolver;

        default:
            return weapon_frames::bat;
        }
    }

    // Milestone 14: a first aid kit or a toolbox opened into the bag (what does not fit is dropped).
    void unpack(game& owner, inventory& items, int slot)
    {
        item_id kit = items.slot(slot).id;
        items.consume_one(slot);

        auto give = [&owner, &items](item_id id, int amount)
        {
            item_stack stack = make_stack(id, amount);

            if(! items.add(stack))
            {
                static_cast<void>(owner.drop_item(stack));
            }
        };

        if(kit == item_id::FIRST_AID_KIT)
        {
            give(item_id::STERILE_BANDAGE, 2);
            give(item_id::ALCOHOL_WIPES, 2);
            give(item_id::PAINKILLERS, 1);
            give(item_id::SUTURE_NEEDLE, 1);
            return;
        }

        constexpr item_id tools[] = { item_id::HAMMER, item_id::SCREWDRIVER, item_id::WRENCH, item_id::PLIERS,
                                      item_id::NAILS, item_id::DUCT_TAPE, item_id::ZIP_TIES, item_id::WIRE,
                                      item_id::SAW };
        constexpr int tool_count = int(sizeof(tools) / sizeof(item_id));
        int first = owner.random().get_int(tool_count);

        for(int index = 0; index < item_tuning::toolbox_items; ++index)
        {
            item_id id = tools[(first + (index * 4)) % tool_count];
            give(id, id == item_id::NAILS ? 12 : id == item_id::WIRE ? 3 : 1);
        }
    }

    void append(char*& output, const char* text)
    {
        while(*text)
        {
            *output++ = *text++;
        }

        *output = 0;
    }

    // Clothing slots covering each body part a grab can wound (spec: Inventory > clothing protection).
    constexpr equip_slot arm_cover[] = { equip_slot::JACKET, equip_slot::TORSO, equip_slot::HANDS };
    constexpr equip_slot torso_cover[] = { equip_slot::JACKET, equip_slot::TORSO, equip_slot::HEAD };
}

namespace
{
    // The skill a weapon class trains (fists: none).
    [[nodiscard]] bool weapon_skill(weapon value, skill& result)
    {
        switch(value)
        {

        case weapon::KNIFE:
            result = skill::BLADE;
            return true;

        case weapon::BAT:
            result = skill::BLUNT;
            return true;

        case weapon::AXE:
            result = skill::AXE;
            return true;

        case weapon::SPEAR:
            result = skill::SPEAR;
            return true;

        default:
            return false;
        }
    }
}

weapon player::current_weapon() const
{
    const item_stack& hand = _inventory.equipped(equip_slot::HAND);
    return hand.empty() ? weapon::FISTS : info(hand.id).melee;
}

int player::condition_eighths() const
{
    const item_stack& hand = _inventory.equipped(equip_slot::HAND);
    int durability = hand.empty() ? 0 : info(hand.id).durability;

    if(durability <= 0)
    {
        return 8;
    }

    return bn::clamp(((hand.amount * 8) + durability - 1) / durability, 0, 8);
}

int player::grab_meter_eighths() const
{
    int needed = grab_meter_full + (((_grabbers - 1) * combat_int(combat_param::GRAB_EXTRA) * grab_meter_full) / 100);
    return bn::clamp((_grab_meter * 8) / bn::max(needed, 1), 0, 8);
}

int player::grab_time_eighths() const
{
    return bn::clamp(((_grab_timer * 8) + combat_int(combat_param::GRAB_FRAMES) - 1) /
                     bn::max(combat_int(combat_param::GRAB_FRAMES), 1), 0, 8);
}

void player::give_starting_kit()
{
    _inventory.clear();
    _inventory.equipped(equip_slot::HAND) = make_stack(item_id::BASEBALL_BAT);
    _inventory.equipped(equip_slot::BACK) = make_stack(item_id::SCHOOL_BAG);
    _inventory.equipped(equip_slot::TORSO) = make_stack(item_id::HOODIE);
    _inventory.equipped(equip_slot::LEGS) = make_stack(item_id::JEANS);
    _inventory.equipped(equip_slot::FEET) = make_stack(item_id::BOOTS);

    constexpr item_id carried[] = { item_id::KITCHEN_KNIFE, item_id::WATER_BOTTLE, item_id::CHIPS };

    for(item_id id : carried)
    {
        item_stack stack = make_stack(id);
        _inventory.add(stack);
    }

    item_stack bandages = make_stack(item_id::BANDAGE, 2);
    _inventory.add(bandages);

    _inventory.set_hotbar(0, item_id::BASEBALL_BAT);
    _inventory.set_hotbar(1, item_id::KITCHEN_KNIFE);
    _inventory.set_hotbar(2, item_id::WATER_BOTTLE);
    _inventory.set_hotbar(3, item_id::BANDAGE);
    _selected = 0;
}

void player::give_test_kit(game& owner)
{
    constexpr item_id kit[] = { item_id::AXE, item_id::CROWBAR, item_id::HUNTING_KNIFE, item_id::LEATHER_JACKET,
                                item_id::GLOVES, item_id::HIKING_BAG, item_id::CANNED_BEANS };
    int added = 0;

    for(item_id id : kit)
    {
        item_stack stack = make_stack(id);
        added += _inventory.add(stack);
    }

    owner.toast(added == int(sizeof(kit) / sizeof(kit[0])) ? "TEST KIT ADDED" : "BAG FULL", hud_tiles::bank_accent);
}

bool player::try_grab(game& owner)
{
    // Nothing reaches the driver of a car; a rider is pulled off the bicycle.
    if(_dead || _invulnerable > 0 || _fall_timer > 0 || _ride == ride_kind::CAR)
    {
        return false;
    }

    _ride = ride_kind::NONE;

    if(_action != player_action::GRABBED)
    {
        _eat_item = item_id::NONE;
        _action = player_action::GRABBED;
        _grab_meter = 0;
        _grab_timer = combat_int(combat_param::GRAB_FRAMES);
        _grabbers = 1;
        _velocity = bn::fixed_point();
        _crouched = false;
    }
    else
    {
        // A second zombie arriving during a grab is the main way players die (spec: Grab).
        ++_grabbers;
    }

    _body.add_panic(grab_panic);
    sfx::play(sfx::id::GRAB, 1, owner.screen_x(_position));
    owner.shake(1, 8);
    _flash_white();
    _roll_wound(owner);
    return true;
}

void player::_update_buttons(const input_frame& input, game& owner)
{
    // B: a tap shoves, holding while moving sprints (spec: Controls).
    if(input.b_pressed)
    {
        _b_frames = 0;
        _b_consumed = false;

        // Standing still there is nothing to sprint, so shove at once.
        if(! input.dx && ! input.dy && _action == player_action::NONE && _shove_cooldown <= 0 && _fall_timer <= 0)
        {
            _b_consumed = true;
            _action = player_action::SHOVE;
            _action_timer = _action_length = combat_int(combat_param::SHOVE_FRAMES);
            _apply_shove(owner);
        }
    }

    if(input.b_held)
    {
        ++_b_frames;
    }
    else if(input.b_released)
    {
        if(! _b_consumed && _b_frames <= combat_int(combat_param::TAP_FRAMES) && _action == player_action::NONE &&
                _shove_cooldown <= 0 && _fall_timer <= 0)
        {
            _action = player_action::SHOVE;
            _action_timer = _action_length = combat_int(combat_param::SHOVE_FRAMES);
            _apply_shove(owner);
        }

        // A release only counts after a press seen here (not one a closing screen used).
        _b_frames = 0;
        _b_consumed = true;
    }

    // L toggles crouch on release, unless it was used as the modifier for L + Select.
    if(input.l_pressed)
    {
        _l_combo = false;
    }

    if(input.select_pressed && _action != player_action::GRABBED)
    {
        if(input.l_held)
        {
            _l_combo = true;
            _selected = (_selected + 1) % inventory::hotbar_slots;
            sfx::play(sfx::id::SWAP, milli(400));
        }
        else if(_action == player_action::NONE)
        {
            _use_hotbar(owner);
        }
    }

    if(input.l_released && ! _l_combo && _fall_timer <= 0 && _action == player_action::NONE)
    {
        _crouched = ! _crouched;
    }
}

void player::_update_action(const input_frame& input, game& owner)
{
    if(_fight_lock > 0)
    {
        --_fight_lock;
    }

    switch(_action)
    {

    case player_action::NONE:
        if(input.a_pressed && _fall_timer <= 0)
        {
            int target = owner.zombies().stomp_target(_position, combat_int(combat_param::STOMP_REACH));
            bool safe = ! fighting() && ! owner.zombie_near(tuning(inventory_param::PROMPT_SAFE_PX).integer());

            if(target >= 0)
            {
                _action = player_action::STOMP;
                _action_timer = _action_length = combat_int(combat_param::STOMP_FRAMES);
                _fight_lock = tuning(inventory_param::FIGHT_LOCK).integer();
            }
            else if(_context == door_context)
            {
                // Doors open and close even with zombies about.
                owner.use_door();
            }
            else if(_context >= 0 && safe)
            {
                owner.begin_search(_context);
            }
            else if(_context == bed_context && safe)
            {
                owner.begin_sleep();
            }
            else if(_context == drink_context && safe)
            {
                owner.drink();
            }
            else if(_context == forage_context && safe)
            {
                owner.forage();
            }
            else if(_context == farm_context && safe)
            {
                owner.tend();
            }
            else if(_context == chop_context && safe)
            {
                owner.chop();
            }
            else if(_context == butcher_context && safe)
            {
                owner.butcher();
            }
            else if(_context == catch_context && safe)
            {
                owner.catch_hen();
            }
            else if(_context == milk_context && safe)
            {
                owner.milk_goat();
            }
            else if(_context == vehicle_context && safe)
            {
                owner.use_vehicle();
            }
            else
            {
                _fight_lock = tuning(inventory_param::FIGHT_LOCK).integer();
                _action = player_action::WINDUP;
                _action_timer = _action_length = weapon_int(current_weapon(), weapon_param::WINDUP) +
                                                 _body.attack_slowdown();
                _charge = 0;
                _charged = false;
                ++_swing_id;
                _swing_hits = 0;
            }
        }
        break;

    case player_action::WINDUP:
        if(--_action_timer <= 0)
        {
            if(input.a_held)
            {
                _action = player_action::CHARGE;
            }
            else
            {
                _begin_active();
            }
        }
        break;

    case player_action::CHARGE:
        if(++_charge == combat_int(combat_param::CHARGE_FRAMES))
        {
            sfx::play(sfx::id::CHARGE_READY, milli(500));
        }

        if(! input.a_held)
        {
            _charged = _charge >= combat_int(combat_param::CHARGE_FRAMES);
            _begin_active();
        }
        break;

    case player_action::ACTIVE:
        _apply_swing(owner);

        if(--_action_timer <= 0)
        {
            _action = player_action::RECOVERY;
            _action_timer = _action_length = weapon_int(current_weapon(), weapon_param::RECOVERY);

            // Skilled swings recover sooner.
            if(skill used; weapon_skill(current_weapon(), used))
            {
                _action_timer = _action_length = bn::max(_action_length - (_character.level(used) *
                                                         character_tuning::recovery_frames_per_level), 4);
            }
        }
        break;

    case player_action::RECOVERY:
    case player_action::SHOVE:
        if(--_action_timer <= 0)
        {
            _action = player_action::NONE;
        }
        break;

    case player_action::STOMP:
        if(_action_timer == _action_length - (_action_length / stomp_impact_divisor))
        {
            int target = owner.zombies().stomp_target(_position, combat_int(combat_param::STOMP_REACH));

            if(target >= 0)
            {
                owner.zombies().stomp(owner, target);
                owner.request_hitstop(combat_int(combat_param::STOMP_HITSTOP));
                owner.shake(2, combat_int(combat_param::STOMP_HITSTOP) + 4);
                owner.noise(_position, combat_int(combat_param::STOMP_NOISE));
            }

            sfx::play(sfx::id::STOMP);
        }

        if(--_action_timer <= 0)
        {
            _action = player_action::NONE;
        }
        break;

    case player_action::EAT:
        // Eating is a short committed action; a grab cancels it and the food is kept.
        if(--_action_timer <= 0)
        {
            _finish_eating(owner);
        }
        break;

    case player_action::GRABBED:
    default:
        _update_grab(input, owner);
        break;
    }
}

void player::_begin_active()
{
    _action = player_action::ACTIVE;
    _action_timer = _action_length = weapon_int(current_weapon(), weapon_param::ACTIVE);
    _exertion += swing_exertion(current_weapon()) * (_charged ? 2 : 1);
    sfx::play(sfx::id::SWING, _charged ? 1 : milli(700));
}

void player::_apply_swing(game& owner)
{
    weapon value = current_weapon();
    int targets = weapon_int(value, weapon_param::TARGETS) - _swing_hits;

    if(targets <= 0)
    {
        return;
    }

    attack_info info;
    info.origin = _position;
    info.direction = direction_unit(_facing_x, _facing_y);
    info.reach = weapon_int(value, weapon_param::REACH);
    info.arc_degrees = weapon_int(value, weapon_param::ARC);
    info.max_targets = targets;
    info.damage = bn::max(weapon_int(value, weapon_param::DAMAGE) - _body.attack_damage_penalty(), 1);
    info.knockback = tuning(value, weapon_param::KNOCKBACK) * _character.knockback_percent() / 100;
    skill used;
    bool skilled = weapon_skill(value, used);

    // Weapon skill: a chance per level of a point more damage.
    if(skilled && owner.random().get_int(100) < _character.level(used) * character_tuning::extra_damage_percent)
    {
        ++info.damage;
    }
    info.stagger_frames = tuning(zombie_param::HIT_STAGGER).integer();
    info.swing_id = _swing_id;
    info.weapon_used = value;
    info.stealth_capable = _crouched && is_blade(value);

    if(_charged)
    {
        // Charged attack; a charged axe hit kills outright (spec: Melee frame data).
        info.damage = value == weapon::AXE ? 100 : (tuning(combat_param::CHARGE_DAMAGE) * info.damage).integer();
        info.knockback *= tuning(combat_param::CHARGE_KNOCKBACK);
    }

    attack_result result = owner.zombies().attack(owner, info);

    if(result.hits <= 0)
    {
        // No zombie: maybe an animal (milestone 13), once per swing.
        if(_swing_hits == 0 &&
           owner.animals().strike(owner, info.origin, info.direction, info.reach, info.arc_degrees, value))
        {
            _swing_hits = weapon_int(value, weapon_param::TARGETS);
            owner.request_hitstop(weapon_int(value, weapon_param::HITSTOP));
            sfx::play(is_blade(value) ? sfx::id::HIT_BLADE : sfx::id::HIT_BLUNT, 1);

            if(skilled)
            {
                train(owner, used, character_tuning::hit_xp);
            }

            _wear_weapon(owner, 1);
        }

        return;
    }

    bool first_contact = _swing_hits == 0;
    _swing_hits += result.hits;

    if(skilled)
    {
        train(owner, used, (result.hits * character_tuning::hit_xp) + (result.kills * character_tuning::kill_xp));
    }

    train(owner, skill::STRENGTH, result.hits * character_tuning::strength_hit_xp);

    if(first_contact)
    {
        // Game feel on every hit: hitstop, shake scaled by weapon weight, layered impact + groan.
        int hitstop = weapon_int(value, weapon_param::HITSTOP) + (_charged ? 2 : 0);
        owner.request_hitstop(hitstop);
        owner.shake(weapon_int(value, weapon_param::SHAKE) + (_charged ? 1 : 0), hitstop + 4);

        sfx::id impact = value == weapon::AXE || _charged ? sfx::id::HIT_HEAVY :
                         (is_blade(value) ? sfx::id::HIT_BLADE : sfx::id::HIT_BLUNT);
        sfx::play(impact, result.stealth_kill ? milli(400) : bn::fixed(1));

        // The zombie's own choke or death groan (playtest 2) answers the hit (kx_zombies).
        if(! result.stealth_kill)
        {
            owner.noise(_position, weapon_int(value, weapon_param::NOISE));
        }
    }

    _wear_weapon(owner, result.hits);
}

void player::_apply_shove(game& owner)
{
    _shove_cooldown = combat_int(combat_param::SHOVE_COOLDOWN);
    _exertion += 30;
    sfx::play(sfx::id::SHOVE, milli(800));

    attack_info info;
    info.origin = _position;
    info.direction = direction_unit(_facing_x, _facing_y);
    info.reach = combat_int(combat_param::SHOVE_REACH);
    info.arc_degrees = combat_int(combat_param::SHOVE_ARC);
    info.max_targets = zombie_manager::max_zombies;     // the main tool against groups
    info.knockback = tuning(combat_param::SHOVE_KNOCKBACK);
    info.stagger_frames = combat_int(combat_param::SHOVE_STAGGER);
    info.swing_id = ++_swing_id;
    info.shove = true;

    attack_result result = owner.zombies().attack(owner, info);
    owner.noise(_position, combat_int(combat_param::SHOVE_NOISE));

    if(result.hits > 0)
    {
        owner.request_hitstop(combat_int(combat_param::SHOVE_HITSTOP));
        owner.shake(1, 4);
        sfx::play(sfx::id::HIT_BLUNT, milli(500));
    }
}

void player::_wear_weapon(game& owner, int hits)
{
    item_stack& hand = _inventory.equipped(equip_slot::HAND);

    if(hand.empty() || info(hand.id).durability == 0)
    {
        return;
    }

    if(hand.amount > hits)
    {
        hand.amount = uint8_t(hand.amount - hits);
        return;
    }

    char text[32];
    char* output = text;
    append(output, info(hand.id).name);
    append(output, " BROKE");
    owner.toast(text, hud_tiles::bank_danger);
    sfx::play(sfx::id::WEAPON_BREAK);
    hand = item_stack();
}

void player::_use_hotbar(game& owner)
{
    item_id id = _inventory.hotbar(_selected);

    if(! _inventory.available(id))
    {
        sfx::play(sfx::id::DENIED, milli(600));
        return;
    }

    // The hammer opens build mode (mockup 06) instead of being swapped in; the shovel opens it on the farm plot.
    if(id == item_id::HAMMER || id == item_id::SHOVEL)
    {
        owner.begin_build(id == item_id::SHOVEL);
        return;
    }

    // The item in hand goes back to the bag (fists); anything else is used from the carried slots.
    if(_inventory.equipped(equip_slot::HAND).id == id)
    {
        unequip(owner, equip_slot::HAND);
        return;
    }

    // In the off hand: a flashlight is switched on or off (the Gear tab puts it away); anything else goes back
    // to the bag.
    if(_inventory.equipped(equip_slot::OFF_HAND).id == id)
    {
        if(id == item_id::FLASHLIGHT)
        {
            switch_flashlight(owner);
        }
        else
        {
            unequip(owner, equip_slot::OFF_HAND);
        }

        return;
    }

    int slot = _inventory.find(id);

    if(slot >= 0)
    {
        use_item(owner, slot);
    }
}

bool player::use_item(game& owner, int slot)
{
    const item_stack& stack = _inventory.slot(slot);

    if(stack.empty())
    {
        return false;
    }

    // Swimming (milestone 13): hands are busy.
    if(_swimming)
    {
        owner.toast("NOT WHILE SWIMMING", hud_tiles::bank_danger, 60);
        sfx::play(sfx::id::DENIED, milli(600));
        return false;
    }

    const item_info& item = info(stack.id);
    char text[32];
    char* output = text;

    if(item.slot != equip_slot::NONE)
    {
        // What the other hand held, to say so if taking this up puts it away (two-handed weapons).
        bool to_off_hand = item.slot == equip_slot::OFF_HAND;
        item_id other = to_off_hand ? _inventory.equipped(equip_slot::HAND).id :
                        item.slot == equip_slot::HAND ? _inventory.equipped(equip_slot::OFF_HAND).id : item_id::NONE;
        bool torch = stack.id == item_id::FLASHLIGHT;

        if(_inventory.equip_from(slot))
        {
            sfx::play(item.category == item_category::WEAPON || to_off_hand ? sfx::id::SWAP : sfx::id::ZIP,
                      milli(700));

            if(other != item_id::NONE &&
                    _inventory.equipped(to_off_hand ? equip_slot::HAND : equip_slot::OFF_HAND).id != other)
            {
                append(output, info(other).name);
                append(output, " PUT AWAY");
                owner.toast(text, hud_tiles::bank_quiet, 90);
            }

            // A flashlight taken in hand is switched on.
            if(torch)
            {
                switch_flashlight(owner, true);
            }

            return true;
        }

        owner.toast("NO ROOM", hud_tiles::bank_danger, 60);
        sfx::play(sfx::id::DENIED, milli(600));
        return false;
    }

    // A cigarette (milestone 13): 3 s, with a lighter or matches.
    if(stack.id == item_id::CIGARETTES)
    {
        if(! _inventory.fire_starter())
        {
            owner.toast("NEEDS A LIGHTER OR MATCHES", hud_tiles::bank_danger, 90);
            sfx::play(sfx::id::DENIED, milli(600));
            return false;
        }

        _eat_item = stack.id;
        _eat_drink = false;
        _action = player_action::EAT;
        _action_timer = _action_length = smoker_tuning::smoke_frames;
        sfx::play(sfx::id::CLICK, milli(600));
        return true;
    }

    if(item.flags & item_flag::edible)
    {
        // A short eating action in the world; the menu closes so it can be seen (spec: visible meter refill).
        _eat_item = stack.id;
        _eat_drink = item.flags & item_flag::drink;
        _action = player_action::EAT;
        _action_timer = _action_length = eat_frames;
        _velocity = bn::fixed_point();
        sfx::play((item.flags & item_flag::drink) ? sfx::id::GULP : sfx::id::EAT, milli(500));
        owner.close_menus();
        return true;
    }

    if(stack.id == item_id::RADIO)
    {
        owner.listen_radio();
        return true;
    }

    // A note: read at once (milestone 12.7); it stays in the bag.
    if(is_note(stack.id))
    {
        owner.read_note(stack);
        return true;
    }

    if(stack.id >= item_id::MAGAZINE_CANNING && stack.id <= item_id::MAGAZINE_GENERATOR)
    {
        return owner.begin_reading_magazine(magazine(int(magazine::CANNING) + int(stack.id) -
                                                     int(item_id::MAGAZINE_CANNING)));
    }

    if(skill taught; book_skill(stack.id, taught))
    {
        return owner.begin_reading(taught, bn::max(int(stack.extra), 1));
    }

    // Milestone 14: pills, kits, binoculars and the sewing kit.
    switch(stack.id)
    {

    case item_id::VITAMINS:
    case item_id::COLD_PILLS:
    case item_id::SLEEPING_PILLS:
        if(stack.id == item_id::SLEEPING_PILLS)
        {
            _body.take_sleeping_pills();
        }
        else
        {
            _body.ease_sickness(stack.id == item_id::VITAMINS ? item_tuning::vitamins_sickness :
                                                                item_tuning::cold_pills_sickness);
        }

        append(output, "TOOK ");
        append(output, item.name);
        owner.toast(text, hud_tiles::bank_normal, 60);
        sfx::play(sfx::id::GULP, milli(600));
        _inventory.consume_one(slot);
        return true;

    case item_id::FIRST_AID_KIT:
    case item_id::TOOLBOX:
        append(output, "OPENED THE ");
        append(output, item.name);
        unpack(owner, _inventory, slot);
        owner.toast(text, hud_tiles::bank_normal, 60);
        sfx::play(sfx::id::RUMMAGE, milli(700));
        return true;

    case item_id::BINOCULARS:
        world_map::scout(position().x().integer() >> world::tile_shift, position().y().integer() >> world::tile_shift,
                         item_tuning::scout_chunks);
        owner.toast("SCOUTED: SEE THE MAP", hud_tiles::bank_normal, 90);
        sfx::play(sfx::id::ZIP, milli(500));
        return true;

    case item_id::SEWING_KIT:
    {
        // Mends the worn piece of clothing most worn, with a piece of cloth or a rag.
        int cloth = _inventory.find(item_id::CLOTH) >= 0 ? _inventory.find(item_id::CLOTH) :
                                                            _inventory.find(item_id::RAG);
        item_stack* worst = nullptr;

        for(int worn = int(equip_slot::HEAD); worn <= int(equip_slot::HANDS); ++worn)
        {
            item_stack& piece = _inventory.equipped(equip_slot(worn));

            if(! piece.empty() && info(piece.id).durability && piece.amount < info(piece.id).durability &&
               (! worst || piece.amount < worst->amount))
            {
                worst = &piece;
            }
        }

        if(! worst || cloth < 0)
        {
            owner.toast(! worst ? "NOTHING WORN TO MEND" : "NEEDS CLOTH OR A RAG", hud_tiles::bank_danger, 60);
            sfx::play(sfx::id::DENIED, milli(600));
            return false;
        }

        worst->amount = uint8_t(bn::min(int(worst->amount) + item_tuning::sew_condition,
                                        int(info(worst->id).durability)));
        append(output, "MENDED ");
        append(output, info(worst->id).name);
        _inventory.consume_one(cloth);
        item_stack& kit = _inventory.slot(_inventory.find(item_id::SEWING_KIT));

        if(kit.amount > 1)
        {
            --kit.amount;
        }
        else
        {
            kit = item_stack();
        }

        owner.toast(text, hud_tiles::bank_normal, 60);
        sfx::play(sfx::id::RUMMAGE, milli(600));
        return true;
    }

    default:
        break;
    }

    if(stack.id == item_id::PAINKILLERS || stack.id == item_id::ANTIBIOTICS)
    {
        if(stack.id == item_id::PAINKILLERS)
        {
            _body.take_painkillers();
        }
        else
        {
            _body.take_antibiotics();
        }

        append(output, "TOOK ");
        append(output, item.name);
        owner.toast(text, hud_tiles::bank_normal, 60);
        sfx::play(sfx::id::GULP, milli(600));
        _inventory.consume_one(slot);
        return true;
    }

    append(output, item.category == item_category::MEDICAL ? "TREAT WOUNDS IN HEALTH (L/R)" : "CAN'T USE THAT YET");
    owner.toast(text, hud_tiles::bank_quiet, 60);
    sfx::play(sfx::id::DENIED, milli(600));
    return false;
}

item_id player::repair_material(const item_stack& weapon) const
{
    if(weapon.empty())
    {
        return item_id::NONE;
    }

    const item_info& item = info(weapon.id);

    if(item.category != item_category::WEAPON || item.durability == 0 || weapon.amount >= item.durability)
    {
        return item_id::NONE;
    }

    if((item.melee == weapon::KNIFE || item.melee == weapon::AXE) && _inventory.find(item_id::WHETSTONE) >= 0)
    {
        return item_id::WHETSTONE;
    }

    if(_inventory.find(item_id::DUCT_TAPE) >= 0)
    {
        return item_id::DUCT_TAPE;
    }

    if(_inventory.find(item_id::ZIP_TIES) >= 0)
    {
        return item_id::ZIP_TIES;
    }

    return _inventory.find(item_id::GLUE) >= 0 ? item_id::GLUE : item_id::NONE;
}

bool player::repair(game& owner, item_stack& weapon)
{
    item_id material = repair_material(weapon);

    if(material == item_id::NONE)
    {
        sfx::play(sfx::id::DENIED, milli(600));
        return false;
    }

    int full = info(weapon.id).durability;
    int added = material == item_id::WHETSTONE ? full : material == item_id::DUCT_TAPE ? full / 2 :
                material == item_id::ZIP_TIES ? (full * item_tuning::zip_ties_percent) / 100 : full / 4;
    weapon.amount = uint8_t(bn::min(int(weapon.amount) + bn::max(added, 1), full));

    // Tape and glue are used up; a whetstone wears down one use at a time.
    int slot = _inventory.find(material);
    item_stack& used = _inventory.slot(slot);

    if(material == item_id::WHETSTONE && used.amount > 1)
    {
        --used.amount;
    }
    else
    {
        _inventory.consume_one(slot);
    }

    char text[32];
    char* output = text;
    append(output, material == item_id::WHETSTONE ? "SHARPENED " : "REPAIRED ");
    append(output, info(weapon.id).name);
    owner.toast(text, hud_tiles::bank_normal, 90);
    sfx::play(material == item_id::WHETSTONE ? sfx::id::HIT_BLADE : sfx::id::RUMMAGE, milli(600));
    return true;
}

void player::switch_flashlight(game& owner, bool force_on)
{
    item_stack& torch = _inventory.equipped(equip_slot::OFF_HAND);

    if(torch.id != item_id::FLASHLIGHT)
    {
        return;
    }

    int charge = light::flashlight_charge(torch);

    if(light::flashlight_on(torch))
    {
        if(! force_on)
        {
            light::set_flashlight(torch, false, charge);
            sfx::play(sfx::id::CLICK, milli(700));
        }

        return;
    }

    if(charge <= 0)
    {
        int battery = _inventory.find(item_id::BATTERY);

        if(battery < 0)
        {
            owner.toast("NO BATTERY FOR THE FLASHLIGHT", hud_tiles::bank_danger, 90);
            sfx::play(sfx::id::CLICK, milli(400));
            return;
        }

        _inventory.consume_one(battery);
        charge = tuning(light_param::BATTERY_MINUTES);
        owner.toast("NEW BATTERY IN THE FLASHLIGHT", hud_tiles::bank_normal, 90);
    }

    light::set_flashlight(torch, true, charge);
    sfx::play(sfx::id::CLICK, milli(700));
}

bool player::unequip(game& owner, equip_slot slot)
{
    if(_inventory.unequip(slot))
    {
        sfx::play(slot == equip_slot::HAND || slot == equip_slot::OFF_HAND ? sfx::id::SWAP : sfx::id::ZIP,
                  milli(600));
        return true;
    }

    owner.toast("NO ROOM", hud_tiles::bank_danger, 60);
    sfx::play(sfx::id::DENIED, milli(600));
    return false;
}

void player::_update_grab(const input_frame& input, game& owner)
{
    // Mash A to break free; every extra grabber raises the bar.
    if(input.a_pressed)
    {
        _grab_meter += combat_int(combat_param::GRAB_PRESS);
    }

    _grab_meter = bn::max(_grab_meter - combat_int(combat_param::GRAB_DECAY), 0);

    int needed = grab_meter_full + (((_grabbers - 1) * combat_int(combat_param::GRAB_EXTRA) * grab_meter_full) / 100);

    if(_grab_meter >= needed)
    {
        _break_free(owner);
        return;
    }

    if(--_grab_timer <= 0)
    {
        if(owner.god_mode())
        {
            owner.toast("BITTEN (GOD MODE)", hud_tiles::bank_danger);
            _break_free(owner);
        }
        else
        {
            sfx::play(sfx::id::BITE);
            owner.shake(3, 20);
            _flash_white();
            _body.bitten_to_death();
            _dead = true;
            _action = player_action::NONE;
            owner.player_bitten();
        }
    }
}

void player::_break_free(game& owner)
{
    owner.zombies().release_grabs(owner);
    _action = player_action::NONE;
    _grabbers = 0;
    _grab_meter = 0;
    _invulnerable = grab_invulnerable_frames;
    sfx::play(sfx::id::BREAK_FREE);
    owner.shake(1, 6);
}

void player::_roll_wound(game& owner)
{
    // Each grab rolls a wound on an arm or the torso (spec: Grab), which may carry the zombie infection.
    int weights[] = { combat_int(combat_param::WOUND_SCRATCH), combat_int(combat_param::WOUND_LACERATION),
                      combat_int(combat_param::WOUND_BITE) };
    int total = weights[0] + weights[1] + weights[2];

    if(total <= 0)
    {
        return;
    }

    int roll = owner.random().get_int(total);
    int wound = roll < weights[0] ? 0 : (roll < weights[0] + weights[1] ? 1 : 2);
    int part = owner.random().get_int(3);
    bn::span<const equip_slot> cover = part == 2 ? bn::span<const equip_slot>(torso_cover) :
                                                   bn::span<const equip_slot>(arm_cover);

    // Clothing: a bite may be turned into a scratch, and a scratch or laceration may be stopped outright.
    // Each piece that helps wears down.
    for(equip_slot slot : cover)
    {
        item_stack& worn = _inventory.equipped(slot);

        if(worn.empty())
        {
            continue;
        }

        const item_info& item = info(worn.id);
        bool helped = false;

        if(wound == 2 && owner.random().get_int(100) < item.bite_defense)
        {
            wound = 0;
            helped = true;
        }
        else if(wound < 2 && owner.random().get_int(100) < item.scratch_defense)
        {
            wound = -1;
            helped = true;
        }

        if(helped)
        {
            worn.amount = uint8_t(bn::max(int(worn.amount) - clothing_wear_per_block, 0));

            if(worn.amount == 0)
            {
                worn = item_stack();
            }

            if(wound < 0)
            {
                char text[32];
                char* output = text;
                append(output, "BLOCKED: ");
                append(output, item.name);
                owner.toast(text, hud_tiles::bank_accent, 90);
                return;
            }
        }
    }

    _body.add_wound(grab_parts[part], grab_wounds[wound], true, owner.random());

    char text[32];
    char* output = text;
    append(output, wound_name(grab_wounds[wound]));
    append(output, ": ");
    append(output, body_part_name(grab_parts[part]));
    owner.toast(text, wound == 0 ? hud_tiles::bank_accent : hud_tiles::bank_danger, 120);
}

int player::eat_progress_eighths() const
{
    if(_action != player_action::EAT)
    {
        return -1;
    }

    return ((_action_length - _action_timer) * 8) / bn::max(_action_length, 1);
}

void player::_finish_eating(game& owner)
{
    _action = player_action::NONE;
    int slot = _inventory.find(_eat_item);

    if(slot < 0)
    {
        return;
    }

    item_stack stack = _inventory.slot(slot);
    const item_info& item = info(stack.id);

    if(stack.id == item_id::CIGARETTES)
    {
        _inventory.consume_one(slot);
        _inventory.use_fire_starter();
        _body.smoke();
        owner.toast("SMOKED A CIGARETTE", hud_tiles::bank_normal, 60);
        _eat_item = item_id::NONE;
        return;
    }

    _body.consume(stack.id, food_age_days(stack, owner.day()), owner.random());
    _inventory.consume_one(slot);

    // A water bottle or a soda leaves its bottle (milestone 12.6: a throwable); dropped if the bag is full.
    if(stack.id == item_id::WATER_BOTTLE || stack.id == item_id::SODA || stack.id == item_id::MILK)
    {
        item_stack bottle = make_stack(item_id::EMPTY_BOTTLE);

        if(! _inventory.add(bottle))
        {
            static_cast<void>(owner.drop_item(bottle));
        }
    }

    char text[32];
    char* output = text;
    append(output, (item.flags & item_flag::drink) ? "DRANK " : "ATE ");
    append(output, item.name);
    owner.toast(text, hud_tiles::bank_normal, 60);
    sfx::play((item.flags & item_flag::drink) ? sfx::id::GULP : sfx::id::EAT, milli(800));
    _eat_item = item_id::NONE;
}

void player::gored(game& owner, const bn::fixed_point& push, int down_frames, int trouser_percent)
{
    if(_dead)
    {
        return;
    }

    if(_aiming)
    {
        _end_aim();
    }

    _ride = ride_kind::NONE;
    _crouched = false;
    _fall_timer = bn::max(_fall_timer, down_frames);
    _mode = move_mode::FALLEN;
    _velocity = bn::fixed_point();
    _knock = push;
    _flash_white();
    owner.shake(3, 10);
    owner.fx().spawn(fx_kind::BLOOD, bn::fixed_point(_position.x(), _position.y() - 6));
    sfx::play(sfx::id::HIT_BLUNT, milli(900));

    body_part part = owner.random().get_int(2) ? body_part::LEFT_LEG : body_part::RIGHT_LEG;
    static_cast<void>(_animal_wound(owner, part, trouser_percent, "GORED: "));
}

void player::bitten_by_wolf(game& owner, const bn::fixed_point& push, int cover_percent)
{
    if(_dead)
    {
        return;
    }

    _knock = push;
    _flash_white();
    owner.shake(2, 6);
    owner.fx().spawn(fx_kind::BLOOD, bn::fixed_point(_position.x(), _position.y() - 6));
    sfx::play(sfx::id::HIT_BLADE, milli(800));

    constexpr body_part parts[] = { body_part::LEFT_ARM, body_part::RIGHT_ARM, body_part::LEFT_LEG,
                                    body_part::RIGHT_LEG };
    static_cast<void>(_animal_wound(owner, parts[owner.random().get_int(4)], cover_percent, "WOLF BITE: "));
}

bool player::_animal_wound(game& owner, body_part part, int cover_percent, const char* label)
{
    // An animal's tooth or tusk: a laceration, never the zombie infection. Sleeves (arms) or trousers (legs) may
    // stop it, wearing as they do against a grab.
    constexpr equip_slot leg_cover[] = { equip_slot::LEGS };
    bool leg = part == body_part::LEFT_LEG || part == body_part::RIGHT_LEG;
    bn::span<const equip_slot> cover = leg ? bn::span<const equip_slot>(leg_cover) :
                                             bn::span<const equip_slot>(arm_cover);

    for(equip_slot slot : cover)
    {
        item_stack& worn = _inventory.equipped(slot);

        if(worn.empty() || owner.random().get_int(100) >= (info(worn.id).scratch_defense * cover_percent) / 100)
        {
            continue;
        }

        char text[32];
        char* output = text;
        append(output, "BLOCKED: ");
        append(output, info(worn.id).name);
        owner.toast(text, hud_tiles::bank_accent, 90);
        worn.amount = uint8_t(bn::max(int(worn.amount) - clothing_wear_per_block, 0));

        if(worn.amount == 0)
        {
            worn = item_stack();
        }

        return false;
    }

    _body.add_wound(part, wound_type::LACERATION, false, owner.random());
    char text[32];
    char* output = text;
    append(output, label);
    append(output, body_part_name(part));
    owner.toast(text, hud_tiles::bank_danger, 120);
    return true;
}

void player::_flash_white()
{
    _flash = combat_int(combat_param::FLASH_FRAMES);

    if(_flash > 0)
    {
        _sprite.set_palette(_flash_palette);
    }
}

bool player::shouldering() const
{
    // Long guns aren't shouldered: they are held at low ready (gun_frames).
    item_id held = _inventory.equipped(equip_slot::HAND).id;
    return long_item(held) && ! is_gun(held) && ! _inventory.equipped(equip_slot::HAND).empty() && ! _aiming &&
           _action == player_action::NONE && _mode != move_mode::FALLEN;
}

int player::gun_frames() const
{
    // A gun in hand with nothing else going on (a swing with it is a club's): aimed, or a long one at low ready.
    item_id held = _inventory.equipped(equip_slot::HAND).id;

    if(! is_gun(held) || _inventory.equipped(equip_slot::HAND).empty() || _action != player_action::NONE ||
       _mode == move_mode::FALLEN)
    {
        return -1;
    }

    if(_aiming)
    {
        return sprite_frames::player_aim;
    }

    return long_item(held) ? sprite_frames::player_ready : -1;
}

int player::_facing_angle() const
{
    // 32 held-item angles: 0 right, 8 down, 16 left, 24 up.
    constexpr int angles[3][3] = {
        { 20, 24, 28 },     // dy = -1: up-left, up, up-right
        { 16, 8, 0 },       // dy = 0: left, (none), right
        { 12, 8, 4 },       // dy = 1: down-left, down, down-right
    };

    return angles[_facing_y + 1][_facing_x + 1];
}

bool player::_weapon_side_flipped() const
{
    return _facing_x < 0 || (_facing_x == 0 && _facing_y < 0);
}

void player::_update_off_sprite(int z_order)
{
    item_id held = _inventory.equipped(equip_slot::OFF_HAND).id;

    if(held == item_id::NONE || _dead || _mode == move_mode::FALLEN || _ride != ride_kind::NONE || _swimming)
    {
        _off_sprite.reset();
        _off_frame = -1;
        return;
    }

    int block = _facing_x != 0 ? 1 : _facing_y < 0 ? 2 : 0;
    hold_pose pose = off_poses[block];
    int hand_x = _facing_x < 0 ? -pose.hand_x : pose.hand_x;
    int hand_y = pose.hand_y + (_crouched ? crouch_hand_drop : 0);

    if(! _off_sprite)
    {
        _off_sprite = bn::sprite_items::weapons.create_sprite(0, 0);
        _off_sprite->set_camera(*_sprite.camera());
        _off_sprite->set_bg_priority(_sprite.bg_priority());
        _off_sprite->set_palette(_held_palette);
        _off_frame = -1;
    }

    // It points along the facing.
    int frame = held_frame_base(held) + _facing_angle();

    if(frame != _off_frame)
    {
        _off_frame = frame;
        _off_sprite->set_tiles(bn::sprite_items::weapons.tiles_item(), frame);
    }

    _off_sprite->set_position(_sprite.position() + bn::fixed_point(hand_x, hand_y));
    light::shade(*_off_sprite);

    int off_z = z_order + (pose.behind ? 1 : -1);

    if(_off_sprite->z_order() != off_z)
    {
        _off_sprite->set_z_order(off_z);
    }
}

void player::update_visuals(int camera_top)
{
    ++_visual_frame;
    light::shade(_sprite);

    int z_order = depth_z(_position.y().floor_integer(), camera_top);

    if(_sprite.z_order() != z_order)
    {
        _sprite.set_z_order(z_order);
    }


    // White flash, and a blink while a charge is full (the visible full-charge pose).
    if(_flash > 0)
    {
        if(--_flash == 0)
        {
            _sprite.set_palette(_normal_palette);
        }
    }
    else if(_action == player_action::CHARGE && _charge >= combat_int(combat_param::CHARGE_FRAMES))
    {
        bool lit = (_visual_frame / charge_blink_frames) & 1;
        _sprite.set_palette(lit ? _flash_palette : _normal_palette);
    }
    else if(_sprite.palette() != _normal_palette)
    {
        _sprite.set_palette(_normal_palette);
    }

    // A ripple round the swimmer's waist (milestone 13), in front of the body.
    if(_swimming && ! _dead)
    {
        if(! _ripple)
        {
            _ripple = bn::sprite_items::ripple.create_sprite(0, 0);
            _ripple->set_camera(*_sprite.camera());
            _ripple->set_bg_priority(_sprite.bg_priority());
            _ripple_frame = -1;
        }

        int frame = (_visual_frame / swim_tuning::ripple_frames) & 1;

        if(frame != _ripple_frame)
        {
            _ripple_frame = frame;
            _ripple->set_tiles(bn::sprite_items::ripple.tiles_item(), frame);
        }

        _ripple->set_position(_sprite.position() + bn::fixed_point(0, swim_tuning::ripple_offset_y));
        _ripple->set_z_order(z_order - 1);
        light::shade(*_ripple);
    }
    else if(_ripple)
    {
        _ripple.reset();
    }

    _update_off_sprite(z_order);

    if(_inventory.equipped(equip_slot::HAND).empty() || _dead || _mode == move_mode::FALLEN ||
            _ride != ride_kind::NONE || _swimming)
    {
        _weapon_sprite.reset();
        _weapon_frame = -1;
        return;
    }

    // Rest pose, or the swing: wind-up back over the shoulder, a sweep through the facing, a follow-through.
    int frame_base = held_frame_base(_inventory.equipped(equip_slot::HAND).id);
    int facing = _facing_angle();
    int side = _weapon_side_flipped() ? -1 : 1;
    int block = _facing_x != 0 ? 1 : _facing_y < 0 ? 2 : 0;
    bool mirrored = _facing_x < 0;
    hold_pose pose = (shouldering() ? shoulder_poses : low_poses)[block];
    int angle = mirrored ? (weapon_frames::angles / 2) - pose.angle : pose.angle;

    // A gun at rest: in the hand, or a long one at low ready (aimed, below).
    item_id hand_item = _inventory.equipped(equip_slot::HAND).id;

    if(is_gun(hand_item) && _action == player_action::NONE)
    {
        pose = (long_item(hand_item) ? gun_ready_poses : gun_rest_poses)[block];
        angle = mirrored ? (weapon_frames::angles / 2) - pose.angle : pose.angle;
    }

    if(_crouched && _action == player_action::NONE)
    {
        pose.hand_y = int8_t(pose.hand_y + crouch_hand_drop);
    }
    bool recovering = _action == player_action::RECOVERY && _action_timer * 2 > _action_length;

    if(_action == player_action::WINDUP || _action == player_action::CHARGE)
    {
        const hold_pose& hand = windup_hands[block];
        angle = facing - (windup_angle * side);
        pose = hold_pose{ hand.hand_x, hand.hand_y, 0, hand.behind };
    }
    else if(_action == player_action::ACTIVE || recovering)
    {
        const hold_pose& hand = strike_hands[block];
        int elapsed = _action_length - _action_timer;
        angle = recovering ? facing + (follow_through * side) :
                             facing - (sweep_start * side) +
                             (((elapsed * sweep_length) / bn::max(_action_length - 1, 1)) * side);
        pose = hold_pose{ hand.hand_x, hand.hand_y, 0, hand.behind };
    }

    if(_aiming && is_gun(hand_item))
    {
        // A gun points along the aim from the aiming frames' hands (32 picture angles, clockwise from east).
        const hold_pose& hand = gun_aim_hands[block];
        angle = (((360 - _aim_angle) * weapon_frames::angles) + 180) / 360;
        int drop = _crouched && _action == player_action::NONE ? crouch_hand_drop : 0;
        pose = hold_pose{ hand.hand_x, int8_t(hand.hand_y + drop), 0, hand.behind };
    }

    angle &= weapon_frames::angles - 1;
    int hand_x = mirrored ? -pose.hand_x : pose.hand_x;
    int hand_y = pose.hand_y;

    if(! _weapon_sprite)
    {
        _weapon_sprite = bn::sprite_items::weapons.create_sprite(0, 0);
        _weapon_sprite->set_camera(*_sprite.camera());
        _weapon_sprite->set_bg_priority(_sprite.bg_priority());
        _weapon_sprite->set_palette(_held_palette);
        _weapon_frame = -1;
    }

    int frame = frame_base + angle;

    if(frame != _weapon_frame)
    {
        _weapon_frame = frame;
        _weapon_sprite->set_tiles(bn::sprite_items::weapons.tiles_item(), frame);
    }

    _weapon_sprite->set_position(_sprite.position() + bn::fixed_point(hand_x, hand_y));
    light::shade(*_weapon_sprite);

    int weapon_z = z_order + (pose.behind ? 1 : -1);

    if(_weapon_sprite->z_order() != weapon_z)
    {
        _weapon_sprite->set_z_order(weapon_z);
    }
}

}
