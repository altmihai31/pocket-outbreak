#ifndef KX_GUNS_H
#define KX_GUNS_H

#include "bn_camera_ptr.h"
#include "bn_fixed_point.h"
#include "bn_optional.h"
#include "bn_sprite_ptr.h"

#include "kx_items.h"
#include "kx_sfx.h"
#include "kx_tuning.h"

namespace kx
{

// Guns (milestone 12.6, docs/milestone-12.6-plan.md; spec: Combat > Guns). Hold R to aim: half speed, the nearest
// zombie in front becomes the target, and a cone around the aim narrows while standing still and with the Aiming
// skill. A fires: each shot (or pellet) flies at a random angle inside the cone and hits the first zombie on its
// line. B reloads from the ammunition carried.
enum class gun : uint8_t
{
    PISTOL,
    SHOTGUN,
    HUNTING_RIFLE,
    ASSAULT_RIFLE,
    REVOLVER,           // milestone 14 (its item and rounds come after the others)
    COUNT
};

struct gun_info
{
    uint8_t capacity;           // rounds loaded
    uint8_t range_tiles;
    uint8_t damage;             // per shot or pellet, in zombie health points (tiers 2, 5, 9)
    uint8_t pellets;
    uint8_t spread;             // degrees each side added to the cone (the shotgun's pattern)
    uint8_t cooldown;           // frames between shots
    bool automatic;             // holding A keeps firing
    uint8_t noise_tiles;        // spec: pistol 30, shotgun 40, rifles 45
    int16_t knockback_milli;    // 3000 and up knocks zombies down
    uint8_t hitstop;
    uint8_t shake;
    uint8_t reload_frames;
    sfx::id sound;
};

inline constexpr gun_info gun_infos[] = {
    { 15, 8, 3, 1, 0, 14, false, 30, 1500, 2, 1, 48, sfx::id::SHOT_PISTOL },
    { 6, 5, 2, 5, 15, 40, false, 40, 3500, 6, 3, 60, sfx::id::SHOT_SHOTGUN },
    { 5, 14, 9, 1, 0, 40, false, 45, 2500, 4, 2, 60, sfx::id::SHOT_RIFLE },
    { 30, 10, 3, 1, 0, 6, true, 45, 1200, 1, 1, 54, sfx::id::SHOT_RIFLE },
    { 6, 9, 4, 1, 0, 22, false, 35, 2200, 3, 2, 66, sfx::id::SHOT_PISTOL },
};

static_assert(sizeof(gun_infos) / sizeof(gun_info) == int(gun::COUNT));

[[nodiscard]] constexpr gun gun_of(item_id id)
{
    return id == item_id::REVOLVER ? gun::REVOLVER : gun(int(id) - int(item_id::PISTOL));
}

[[nodiscard]] constexpr const gun_info& info(gun value)
{
    return gun_infos[int(value)];
}

[[nodiscard]] constexpr item_id ammo_of(gun value)
{
    return value == gun::REVOLVER ? item_id::AMMO_38 : item_id(int(item_id::AMMO_9MM) + int(value));
}

// A gun item keeps its loaded rounds in the stack's extra field, and bit 15 for a jam (so a jammed gun stays
// jammed in the bag and in the save).
constexpr uint16_t gun_loaded_mask = 0x7FFF;
constexpr uint16_t gun_jam_bit = 0x8000;

[[nodiscard]] inline bool gun_jammed(const item_stack& stack)
{
    return stack.extra & gun_jam_bit;
}

inline void set_gun_jammed(item_stack& stack, bool jammed)
{
    stack.extra = uint16_t(jammed ? stack.extra | gun_jam_bit : stack.extra & ~gun_jam_bit);
}

[[nodiscard]] inline int gun_loaded(const item_stack& stack)
{
    return stack.extra & gun_loaded_mask;
}

inline void set_gun_loaded(item_stack& stack, int rounds)
{
    stack.extra = uint16_t((stack.extra & ~gun_loaded_mask) | (rounds & gun_loaded_mask));
}

enum class gun_param : int
{
    CONE_MOVING,
    CONE_STILL,
    STILL_FRAMES,
    CONE_PER_LEVEL,
    CONE_PER_PANIC,
    CONE_MIN,
    LOCK_ARC,
    HIT_RADIUS,
    AIM_SPEED,
    EMPTY_CLICK_FRAMES,
    RELOAD_PER_LEVEL,
    RELOAD_LEAST,
    SWEET_FRAMES,
    SWEET_PER_LEVEL,
    SWEET_FROM,
    SWEET_TO,
    CLEAR_FRAMES,
    COUNT
};

inline constexpr tuning_entry gun_tuning_defaults[] = {
    tune_int("Cone moving deg", 20, 1, 60),              // each side of the aim
    tune_int("Cone still deg", 8, 1, 60),                // after standing still
    tune_int("Still frames", 30, 1, 240, 5),
    tune_int("Cone per Aiming lvl", 2, 0, 10),           // narrower per level
    tune_int("Cone per panic lvl", 4, 0, 20),            // wider per Panic level (spec: aim sway)
    tune_int("Cone least deg", 2, 0, 20),
    tune_int("Auto-aim arc deg", 50, 10, 180, 5),        // the nearest zombie inside it is the target
    tune_int("Hit radius px", 7, 1, 16),                 // how far from a zombie's middle a shot still hits
    tune_milli("Aim speed x", 500, 100, 1000, 50),       // spec: movement drops to half speed
    tune_int("Empty click frames", 12, 1, 60),
    // Active reload (spec: a timing bar; the sweet spot reloads faster, a miss jams).
    tune_int("Reload per lvl frames", 4, 0, 20),         // shorter bar per Reloading level
    tune_int("Reload least frames", 20, 4, 120),
    tune_int("Sweet spot frames", 10, 1, 60),
    tune_int("Sweet per lvl frames", 2, 0, 10),          // wider sweet spot per Reloading level
    tune_int("Sweet from %", 35, 0, 100, 5),             // where it can start along the bar
    tune_int("Sweet to %", 70, 0, 100, 5),
    tune_int("Jam clear frames", 30, 1, 240, 5),
};

static_assert(sizeof(gun_tuning_defaults) / sizeof(tuning_entry) == int(gun_param::COUNT));

[[nodiscard]] tuning_set& gun_tuning();

[[nodiscard]] inline int tuning_int(gun_param param)
{
    return gun_tuning().get(int(param)).integer();
}

// What the aiming sprites follow, once a frame.
struct aim_view
{
    bool aiming = false;
    bn::fixed_point muzzle;         // world pixels
    int angle = 0;                  // degrees counterclockwise from east
    int cone = 0;                   // degrees each side
    bool target = false;
    bn::fixed_point target_feet;
    bool single_line = false;       // a throw: one dotted line along the aim, no cone
    int box_half_width = 8;         // the brackets' box: round a zombie, or smaller round a throw's landing spot
    int box_height = 18;
};

// Aiming and firing sprites: the two dotted edges of the cone, corner brackets round the target, and for a few
// frames after a shot its streaks and a muzzle flash. They are not shaded at night.
class gun_effects
{

public:
    static constexpr int max_streaks = 3;

    explicit gun_effects(const bn::camera_ptr& camera);

    // A shot from the muzzle at an angle (degrees counterclockwise from east); up to 3 at once (a shotgun).
    void shot(const bn::fixed_point& muzzle, int angle);

    void update(const aim_view& value);

private:
    bn::camera_ptr _camera;
    bn::optional<bn::sprite_ptr> _edges[2];
    bn::optional<bn::sprite_ptr> _brackets[4];
    bn::optional<bn::sprite_ptr> _streaks[max_streaks];
    bn::optional<bn::sprite_ptr> _flash;
    int _streak_frames = 0;

    void _line(bn::optional<bn::sprite_ptr>& sprite, const bn::fixed_point& start, int angle, int frame);
};

}

#endif
