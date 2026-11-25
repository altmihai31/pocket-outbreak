#ifndef KX_PLAYER_H
#define KX_PLAYER_H

#include "bn_array.h"
#include "bn_camera_ptr.h"
#include "bn_fixed_point.h"
#include "bn_optional.h"
#include "bn_sprite_palette_ptr.h"
#include "bn_sprite_ptr.h"

#include "kx_bench.h"
#include "kx_character.h"
#include "kx_combat_tuning.h"
#include "kx_inventory.h"
#include "kx_survival.h"
#include "kx_terrain.h"

namespace kx
{

class game;
struct input_frame;

namespace save
{
    class reader;
    class writer;
}

enum class move_mode : uint8_t
{
    IDLE,
    CROUCH,
    WALK,
    JOG,
    SPRINT,
    FALLEN
};

[[nodiscard]] const char* mode_name(move_mode mode);

enum class player_action : uint8_t
{
    NONE,
    WINDUP,
    CHARGE,
    ACTIVE,
    RECOVERY,
    SHOVE,
    STOMP,
    GRABBED,
    EAT
};

// What carries the survivor (spec: Vehicles): a bicycle moves like walking, only faster; a car moves them itself.
enum class ride_kind : uint8_t
{
    NONE,
    BICYCLE,
    CAR
};

class player
{

public:
    player(const bn::camera_ptr& camera, const bn::fixed_point& feet_position);

    void update(const input_frame& input, game& owner);

    // Visual step (every frame, also during hitstop): flash, depth sorting, weapon sprite.
    void update_visuals(int camera_top);

    // World position of the feet (bottom centre of the collision box).
    [[nodiscard]] const bn::fixed_point& position() const
    {
        return _position;
    }

    [[nodiscard]] bn::fixed speed() const
    {
        return _speed;
    }

    [[nodiscard]] move_mode mode() const
    {
        return _mode;
    }

    [[nodiscard]] terrain ground() const
    {
        return _ground;
    }

    // Direction currently pressed (0, 0 when no input, fallen or busy with an action).
    [[nodiscard]] int input_x() const
    {
        return _input_x;
    }

    [[nodiscard]] int input_y() const
    {
        return _input_y;
    }

    // Last direction faced (never 0, 0).
    [[nodiscard]] int facing_x() const
    {
        return _facing_x;
    }

    [[nodiscard]] int facing_y() const
    {
        return _facing_y;
    }

    void teleport(const bn::fixed_point& feet)
    {
        _position = feet;
    }

    // Benches: turn to face a direction (-1, 0, 1 on each axis).
    void face(int dx, int dy)
    {
        _facing_x = dx;
        _facing_y = dy;
    }

    // Tiles; 0 while standing still.
    [[nodiscard]] int noise_radius() const
    {
        return _noise_radius;
    }

    // ----- combat -----

    [[nodiscard]] player_action action() const
    {
        return _action;
    }

    [[nodiscard]] weapon current_weapon() const;

    [[nodiscard]] const inventory& items() const
    {
        return _inventory;
    }

    [[nodiscard]] inventory& items()
    {
        return _inventory;
    }

    // Selected hotbar slot (Select uses it, L + Select moves it).
    [[nodiscard]] int selected_slot() const
    {
        return _selected;
    }

    // Condition of the item in hand in 0..8 eighths (8 for fists).
    [[nodiscard]] int condition_eighths() const;

    // Uses a carried item: equips weapons, clothing and bags, eats food. Returns false (with feedback) if the
    // item has no use yet or there is no room.
    bool use_item(game& owner, int slot);

    // The flashlight held in the off hand: switches it on or off (force_on: on, as when it is taken in hand). An
    // empty one takes a battery from the bag first. Nothing happens without a flashlight in the off hand.
    void switch_flashlight(game& owner, bool force_on = false);

    // Weapon repair (milestone 12.5): what a worn weapon would be mended with, a whetstone for a blade (knives and
    // the axe, back to full), else duct tape (half its condition) or glue (a quarter); NONE when it is not worn or
    // nothing is carried. repair uses it up (a whetstone lasts 5 times).
    [[nodiscard]] item_id repair_material(const item_stack& weapon) const;
    bool repair(game& owner, item_stack& weapon);

    // Puts an equipped item back into the carried slots.
    bool unequip(game& owner, equip_slot slot);

    [[nodiscard]] ride_kind ride() const
    {
        return _ride;
    }

    void set_ride(ride_kind value);

    // In a car: where it is, how it moves (for the camera) and how loud its engine is.
    void carried(const bn::fixed_point& feet, const bn::fixed_point& velocity, int noise_tiles);

    static constexpr int bed_context = -2;
    static constexpr int drink_context = -3;      // shallow water or a rain collector in front
    static constexpr int forage_context = -4;     // crouched on forest, field or waterside ground
    static constexpr int farm_context = -5;       // a farm plot, generator, trap or fuel pump to tend
    static constexpr int vehicle_context = -6;    // a bicycle to ride, a car or wreck to look over
    static constexpr int door_context = -7;       // a door to open or close (milestone 12.7)
    static constexpr int chop_context = -8;       // a tree to chop, an axe in hand
    static constexpr int butcher_context = -9;    // a deer carcass, a knife or an axe in hand (milestone 13)
    static constexpr int catch_context = -10;     // a hen within reach (animals plan stage 4)
    static constexpr int milk_context = -11;      // a goat not milked today, an empty bottle carried

    // Container in front of the player that A would search, bed_context for a bed, or -1 (set by the game
    // every frame).
    // True for a moment after a swing or stomp: A keeps attacking instead of searching a fresh corpse.
    [[nodiscard]] bool fighting() const
    {
        return _fight_lock > 0;
    }

    // Carrying a long item on the shoulder (the carry body frames; not while swinging or acting).
    [[nodiscard]] bool shouldering() const;

    // The first of the gun body frames in use (player_aim or player_ready, per facing block 4 poses), or -1.
    [[nodiscard]] int gun_frames() const;

    // In deep water (milestone 13): half speed, no sprinting, crouching, attacking, aiming or using items.
    [[nodiscard]] bool swimming() const
    {
        return _swimming;
    }

    void set_context(int container_index)
    {
        _context = container_index;
    }

    [[nodiscard]] int context() const
    {
        return _context;
    }

    [[nodiscard]] bool grabbed() const
    {
        return _action == player_action::GRABBED;
    }

    // Grab struggle: meter progress and time left, both in 0..8 eighths.
    [[nodiscard]] int grab_meter_eighths() const;
    [[nodiscard]] int grab_time_eighths() const;

    #if KX_BENCH
        // Benches: a grab left over from an earlier phase ends (its zombie is gone).
        void bench_release()
        {
            if(_action == player_action::GRABBED)
            {
                _action = player_action::NONE;
                _grabbers = 0;
            }
        }
    #endif

    [[nodiscard]] int grabbers() const
    {
        return _grabbers;
    }

    [[nodiscard]] bool dead() const
    {
        return _dead;
    }

    [[nodiscard]] const survival& body() const
    {
        return _body;
    }

    [[nodiscard]] survival& body()
    {
        return _body;
    }

    // Exertion points since the last call (the game feeds them to the minute tick).
    [[nodiscard]] int take_exertion()
    {
        int result = _exertion;
        _exertion = 0;
        return result;
    }

    // Eating or drinking in progress: 0..8 eighths, -1 when not eating.
    [[nodiscard]] int eat_progress_eighths() const;

    // Occupation, traits and skills (spec: Character).
    [[nodiscard]] const character& skills() const
    {
        return _character;
    }

    [[nodiscard]] character& skills()
    {
        return _character;
    }

    // XP for a skill; a level gained shows a message.
    void train(game& owner, skill value, int amount);

    [[nodiscard]] bool drinking() const
    {
        return _eat_drink;
    }

    // Distance walked, in tiles.
    [[nodiscard]] int distance_tiles() const
    {
        return _distance.integer() / 16;
    }

    // Death without a bite (restart bench).
    void die()
    {
        _dead = true;
        _action = player_action::NONE;
    }

    // ----- guns (milestone 12.6, kx_player_guns.cpp) -----

    // Aiming a gun (R held): the HUD and the aiming sprites follow these.
    [[nodiscard]] bool aiming() const
    {
        return _aiming;
    }

    [[nodiscard]] bool reloading() const
    {
        return _reload_length > 0;
    }

    // The active reload's bar: frames filled, its length, and the sweet spot (start and length, in frames).
    [[nodiscard]] int reload_frame() const
    {
        return _reload_frame;
    }

    [[nodiscard]] int reload_length() const
    {
        return _reload_length;
    }

    [[nodiscard]] int sweet_start() const
    {
        return _sweet_start;
    }

    [[nodiscard]] int sweet_length() const
    {
        return _sweet_length;
    }

    // The gun in hand is jammed (firing only clicks); clearing it takes B and half a second.
    [[nodiscard]] bool gun_jammed_now() const;

    [[nodiscard]] bool clearing_jam() const
    {
        return _jam_clear > 0;
    }

    // A during the reload bar: inside the sweet spot the reload finishes at once, outside it the gun jams.
    void reload_press(game& owner);

    // Reloads finished at the sweet spot, normally, and missed into a jam (the guns bench's log).
    [[nodiscard]] int fast_reloads() const
    {
        return _fast_reloads;
    }

    [[nodiscard]] int jams() const
    {
        return _jams;
    }

    // The aim: where the muzzle is, its angle (degrees counterclockwise from east), the cone each side, and the
    // zombie it has locked on to, if any.
    [[nodiscard]] const bn::fixed_point& aim_muzzle() const
    {
        return _aim_muzzle;
    }

    [[nodiscard]] int aim_angle() const
    {
        return _aim_angle;
    }

    [[nodiscard]] int aim_cone() const
    {
        return _aim_cone;
    }

    [[nodiscard]] bool aim_locked() const
    {
        return _aim_locked;
    }

    [[nodiscard]] const bn::fixed_point& aim_target_feet() const
    {
        return _aim_target_feet;
    }

    // Rounds in the gun in hand and of its kind in the bag; -1 without a gun in hand.
    [[nodiscard]] int gun_rounds() const;
    [[nodiscard]] int gun_rounds_carried() const;

    // Shots fired and shots that hit (the guns bench's log).
    [[nodiscard]] int shots_fired() const
    {
        return _shots_fired;
    }

    [[nodiscard]] int shots_hit() const
    {
        return _shots_hit;
    }

    // A zombie tries to grab the player; returns true if it took hold.
    bool try_grab(game& owner);

    // A boar's tusk (animals plan stage 2): a leg laceration (the trousers may stop it, meeting `trouser_percent`
    // of their scratch defence), then thrown back along `push` (px per frame) and down for `down_frames`. Off a
    // bicycle too.
    void gored(game& owner, const bn::fixed_point& push, int down_frames, int trouser_percent);

    // A wolf's bite (stage 3): a laceration on an arm or a leg, never infected (sleeves or trousers may stop it,
    // meeting `cover_percent` of their scratch defence), and a small shove along `push`.
    void bitten_by_wolf(game& owner, const bn::fixed_point& push, int cover_percent);

    // Character block: where the survivor stands, what they carry and their body.
    void save(save::writer& out) const;
    void load(save::reader& in);

    // Starting kit, and the debug test kit.
    void give_starting_kit();
    void give_test_kit(game& owner);

private:
    bn::sprite_ptr _sprite;
    bn::optional<bn::sprite_ptr> _weapon_sprite;
    bn::optional<bn::sprite_ptr> _off_sprite;   // the off hand's item
    bn::optional<bn::sprite_ptr> _ripple;       // round a swimmer's waist (milestone 13)
    int _ripple_frame = -1;
    int _sea_message = 0;                       // frames until "too far from land" may show again
    bn::sprite_palette_ptr _normal_palette;
    int _outfit_key = -1;
    bn::sprite_palette_ptr _flash_palette;
    bn::sprite_palette_ptr _held_palette;       // the held item's, with the gripping hand in the hands' colour
    bn::fixed_point _position;
    bn::fixed_point _velocity;
    bn::fixed_point _knock;             // a push from outside (a boar's gore), fading
    bn::fixed _speed;
    bn::fixed _brake_step;
    bn::fixed _stride;
    inventory _inventory;
    int _selected = 0;
    int _context = -1;
    int _fight_lock = 0;
    int _press_frames = 0;
    int _release_frames = 1000;
    int _tap_x = 0;
    int _tap_y = 0;
    int _fall_timer = 0;
    int _facing_x = 0;
    int _facing_y = 1;
    int _input_x = 0;
    int _input_y = 0;
    int _anim_step = 0;
    int _sprite_frame = -1;
    int _weapon_frame = -1;
    int _off_frame = -1;
    bn::fixed_point _aim_muzzle;
    bn::fixed_point _aim_target_feet;
    int _aim_angle = 0;
    int _aim_cone = 0;
    int _aim_still = 0;             // frames standing still while aiming
    int _fire_cooldown = 0;
    int _reload_frame = 0;
    int _reload_length = 0;         // 0: not reloading
    int _sweet_start = 0;
    int _sweet_length = 0;
    int _jam_clear = 0;             // frames left clearing a jam
    int _fast_reloads = 0;
    int _jams = 0;
    int _shots_fired = 0;
    int _shots_hit = 0;
    bool _aiming = false;
    bool _aim_locked = false;
    int _noise_radius = 0;
    int _action_timer = 0;
    int _action_length = 0;
    int _charge = 0;
    int _swing_id = 0;
    int _swing_hits = 0;
    int _shove_cooldown = 0;
    int _b_frames = 0;
    int _grab_meter = 0;
    int _grab_timer = 0;
    int _grabbers = 0;
    int _invulnerable = 0;
    int _flash = 0;
    int _visual_frame = 0;
    move_mode _mode = move_mode::IDLE;
    player_action _action = player_action::NONE;
    terrain _ground = terrain::GRASS;
    bool _crouched = false;
    bool _swimming = false;

    [[nodiscard]] bool _swim_allowed() const;
    [[nodiscard]] static bool _near_land(int tile_x, int tile_y);

public:
    [[nodiscard]] bool crouched() const
    {
        return _crouched;
    }

private:
    bool _braking = false;
    bool _tap_valid = false;
    bool _jog_latched = false;
    bool _b_consumed = true;
    bool _l_combo = false;
    bool _charged = false;
    bool _dead = false;
    survival _body;
    character _character;
    bn::fixed _distance;
    item_id _eat_item = item_id::NONE;
    int _exertion = 0;
    bool _eat_drink = false;
    ride_kind _ride = ride_kind::NONE;

    void _update_double_tap(int dx, int dy);
    void _update_movement(int dx, int dy, bool sprint_held, game& owner);

    [[nodiscard]] bool _blocked_at(const bn::fixed_point& feet) const;

    // Moves along one axis with collision; returns true if stopped by a wall (no corner slide).
    bool _move_axis(bool horizontal, bn::fixed& velocity, bool slide_allowed);

    void _update_animation(game* owner);

    // Combat (kx_player_combat.cpp).
    void _update_buttons(const input_frame& input, game& owner);
    void _update_action(const input_frame& input, game& owner);
    void _begin_active();
    void _apply_swing(game& owner);
    void _apply_shove(game& owner);
    void _wear_weapon(game& owner, int hits);
    void _use_hotbar(game& owner);

    // Guns: aiming while R is held, firing with A, reloading with B.
    [[nodiscard]] bool _wants_aim(const input_frame& input) const;
    void _update_aim(const input_frame& input, game& owner);
    void _update_throw(const input_frame& input, game& owner);
    void _throw(game& owner);
    void _end_aim();
    void _fire(game& owner);
    void _begin_reload(game& owner);
    void _finish_reload(game& owner, bool fast);
    void _update_off_sprite(int z_order);

    void _update_grab(const input_frame& input, game& owner);
    void _break_free(game& owner);
    void _roll_wound(game& owner);
    bool _animal_wound(game& owner, body_part part, int cover_percent, const char* label);
    void _finish_eating(game& owner);
    void _flash_white();
    void _update_outfit();
    [[nodiscard]] int _facing_angle() const;
    [[nodiscard]] bool _weapon_side_flipped() const;
};

}

#endif
