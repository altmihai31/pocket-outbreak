#ifndef KX_MUSIC_H
#define KX_MUSIC_H

#include "bn_common.h"

namespace kx::music
{

// Music (milestone 14, docs/milestone-14-plan.md stage 6): five short loops on the GBA's old (DMG) sound channels
// (tools/assets/music.py), so the sound effects keep the direct-sound channels to themselves. A small director picks
// what plays: the title loop on the title and character screens; in the world, silence most of the time, with the
// calm day or the night tune now and then, the chase loop while a horde is after you (fading back to silence after),
// and the death tune under the death screen.
enum class track : uint8_t
{
    NONE,
    TITLE,
    CALM,
    NIGHT,
    CHASE,
    DEATH
};

// What the director looks at, once a game frame.
struct situation
{
    bool night = false;
    bool asleep = false;
    bool dead = false;
    int chasers = 0;            // zombies after the survivor (chasing or attacking)
};

namespace music_tuning
{
    constexpr int chase_start = 3;              // zombies chasing before the chase loop starts (spec: a horde)
    constexpr int chase_hold_frames = 5 * 60;   // it plays on this long after the last one gives up
    constexpr int fade_frames = 3 * 60;         // then fades out over this long
    constexpr int first_gap_frames = 40 * 60;   // silence after a world starts, before the first tune
    constexpr int gap_min_frames = 75 * 60;     // silence between tunes (spec: a minute or more)
    constexpr int gap_extra_frames = 90 * 60;   // plus up to this much more
    constexpr int calm_volume_percent = 75;     // the quiet tunes sit under the world's sounds
    constexpr int night_volume_percent = 70;
    constexpr int death_volume_percent = 85;
    constexpr int update_frames = 4;            // the director looks every 4 frames (counting the chase costs)
}

// The title and character creation: the title loop (it keeps playing between the two screens).
void title();

// A world starts (or the next survivor): whatever plays fades out; silence before the first tune.
void enter_world();

// Every few game frames (on the death screen too): `frames` is how many have passed since the last call.
void update(const situation& now, int frames);

// The MUSIC setting changed (playtest 2): what plays takes the new level.
void apply_volume();

// What is playing (benches log the director's choices).
[[nodiscard]] track playing();
[[nodiscard]] const char* track_name(track value);

// GBT Player is playing something (benches check the director's choice is heard).
[[nodiscard]] bool sounding();

}

#endif
