#ifndef KX_CAMERA_TUNING_H
#define KX_CAMERA_TUNING_H

#include "kx_tuning.h"

namespace kx
{

// Camera tuning (spec: Movement, camera and exploration > Camera).
enum class camera_param : int
{
    LOOKAHEAD_MIN,
    LOOKAHEAD_MAX,
    LOOKAHEAD_EASE,
    EASE_DIV,
    DEADZONE,
    BODY_OFFSET,
    AIM_SHIFT,
    AIM_EASE_FRAMES,
    COUNT
};

constexpr tuning_entry camera_tuning_defaults[] = {
    tune_int("Lookahead min px", 24, 0, 80),                // at walk speed and below
    tune_int("Lookahead max px", 32, 0, 80),                // at sprint speed
    tune_int("Lookahead ease 1/n", 4, 1, 32),               // smooths direction changes of the lookahead point
    tune_int("Camera ease 1/n", 8, 1, 32),                  // fraction of the gap closed per frame
    tune_int("Deadzone px", 16, 0, 48),
    tune_int("Body offset px", 8, 0, 16),                   // aim at the chest, not the feet
    tune_int("Aim shift px", 24, 0, 64),                    // milestone 13: aiming looks further along the aim
    tune_int("Aim ease frames", 12, 1, 60),
};

static_assert(sizeof(camera_tuning_defaults) / sizeof(tuning_entry) == int(camera_param::COUNT));

[[nodiscard]] tuning_set& camera_tuning();

[[nodiscard]] inline bn::fixed tuning(camera_param param)
{
    return camera_tuning().get(int(param));
}

}

#endif
