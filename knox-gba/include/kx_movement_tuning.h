#ifndef KX_MOVEMENT_TUNING_H
#define KX_MOVEMENT_TUNING_H

#include "kx_tuning.h"

namespace kx
{

// Player movement tuning (spec: Movement, camera and exploration). Speeds are px/frame at 60 fps.
enum class move_param : int
{
    CROUCH_SPEED,
    WALK_SPEED,
    JOG_SPEED,
    SPRINT_SPEED,
    ACCEL_FRAMES,
    STOP_FRAMES,
    DIAGONAL,
    DOUBLE_TAP_FRAMES,
    SLIDE_REACH,
    SLIDE_SPEED,
    FOREST_MULT,
    MUD_MULT,
    SHALLOW_MULT,
    CROUCH_NOISE,
    WALK_NOISE,
    JOG_NOISE,
    SPRINT_NOISE,
    SHALLOW_NOISE,
    FALL_FRAMES,
    FALL_MIN_SPEED,
    STRIDE_PX,
    HITBOX_W,
    HITBOX_H,
    SWIM_MULT,
    SWIM_NOISE,
    COUNT
};

constexpr tuning_entry movement_tuning_defaults[] = {
    tune_milli("Crouch speed", 600, 100, 3000, 50),
    tune_milli("Walk speed", 1000, 100, 4000, 50),
    tune_milli("Jog speed", 1500, 100, 4000, 50),
    tune_milli("Sprint speed", 2250, 100, 5000, 50),
    tune_int("Accel frames", 5, 1, 30),
    tune_int("Stop frames", 3, 1, 30),
    tune_milli("Diagonal mult", 707, 500, 1000, 1),
    tune_int("Double-tap frames", 12, 0, 30),               // max tap length and gap to start a jog; 0 = no jog
    tune_int("Corner slide px", 4, 0, 8),
    tune_milli("Slide speed", 1000, 250, 4000, 250),
    tune_milli("Forest mult", 800, 100, 1000, 50),
    tune_milli("Mud mult", 800, 100, 1000, 50),
    tune_milli("Shallow mult", 600, 100, 1000, 50),
    tune_int("Crouch noise", 1, 0, 60),
    tune_int("Walk noise", 2, 0, 60),
    tune_int("Jog noise", 4, 0, 60),
    tune_int("Sprint noise", 6, 0, 60),
    tune_int("Shallow noise +", 2, 0, 20),                  // spec says shallow water is "loud"; value is ours
    tune_int("Fall frames (0=off)", 40, 0, 120, 5),
    tune_milli("Fall min speed", 1600, 500, 5000, 100),     // blocked-axis speed; just above jog
    tune_int("Stride px", 6, 2, 24),                        // px travelled per walk-cycle frame
    tune_int("Hitbox width", 10, 4, 15),
    tune_int("Hitbox height", 6, 2, 15),
    tune_milli("Swim mult", 500, 100, 1000, 50),             // milestone 13: of walking speed
    tune_int("Swim noise +", 2, 0, 20),                     // splashing, on top of walking
};

static_assert(sizeof(movement_tuning_defaults) / sizeof(tuning_entry) == int(move_param::COUNT));

[[nodiscard]] tuning_set& movement_tuning();

// Swimming in deep water (milestone 13): no sprinting, crouching, attacking, aiming or using items; heavy load at
// this level or worse keeps you out; Exhausted at the drowning level costs 1 health a drown step; you can't swim
// further than this many tiles from land (along the 8 directions).
namespace swim_tuning
{
    constexpr int heavy_level = 2;
    constexpr int drown_level = 4;
    constexpr int drown_frames = 60;
    constexpr int land_tiles = 8;
    constexpr int ripple_frames = 16;           // the ripple's two frames alternate this often
    constexpr int ripple_offset_y = 3;          // below the sprite's centre: round the waist
}

[[nodiscard]] inline bn::fixed tuning(move_param param)
{
    return movement_tuning().get(int(param));
}

}

#endif
