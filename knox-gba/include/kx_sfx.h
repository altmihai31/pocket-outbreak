#ifndef KX_SFX_H
#define KX_SFX_H

#include "bn_fixed.h"

namespace bn
{
    class sound_item;
}

namespace kx::sfx
{

enum class id : uint8_t
{
    SWING,
    HIT_BLUNT,
    HIT_BLADE,
    HIT_HEAVY,
    GROAN,          // a zombie's moan (playtest 2: the user's zombie_moan)
    SHOVE,
    STOMP,
    GRAB,
    BITE,
    BREAK_FREE,
    WEAPON_BREAK,
    CHARGE_READY,
    STEP_HARD,
    STEP_SOFT,
    STEP_WATER,
    SWAP,
    POP,            // searched item appears
    CHIME,          // rare item
    RUMMAGE,
    PICKUP,
    DROP,
    ZIP,            // clothing and bags
    DENIED,         // blocked action: full bag, nothing to use
    GROWL,          // moodle sounds (spec: stomach growl, shiver)
    SHIVER,
    HEARTBEAT,
    COUGH,
    MOODLE,
    EAT,
    GULP,
    BANDAGE,
    HAMMER,         // base building
    GLASS,          // a window smashed
    BANG,           // a zombie hitting a barricade
    THUNDER,
    GUNSHOT,        // world events (distant)
    ALARM,
    ROTOR,
    STATIC,         // the radio
    ENGINE,         // vehicles: a running engine (repeated), the starter, a crash, a bicycle chain
    STARTER,
    CRASH,
    CHAIN,
    CLICK,          // a flashlight's switch; an empty gun
    SHOT_PISTOL,    // guns (milestone 12.6)
    SHOT_SHOTGUN,
    SHOT_RIFLE,
    RELOAD,
    THROW,          // throwables (milestone 12.6 stage 5)
    FIRE,
    RING,           // an alarm clock
    DOOR_OPEN,      // doors (milestone 12.7)
    DOOR_CLOSE,
    RATTLE,
    LOCKPICK,
    CROWBAR,
    CHOP,           // chopping trees (milestone 12.7 stage 5)
    TIMBER,
    BUBBLES,        // drowning (milestone 13)
    FLUTTER,        // a flock taking off
    SHUFFLE,        // milestone 14 sound pass: a zombie's feet close by
    GLASS_STEP,     // broken glass underfoot
    RAIN_ROOF,      // rain on the roof, indoors
    WIND,           // a gust in the wild
    BREATH,         // playtest 2, the user's zombie sounds: an unaware zombie breathing close by
    SNARL,          // a zombie noticing you (zombie_growl; GROWL is the stomach)
    CHOKE,          // a zombie hit and still standing
    DEATH_GROAN,    // a zombie dying
    BOAR_GRUNT,     // animals plan stage 6: the animals' calls (tools/assets/animal_sounds.py, the user's picks)
    WOLF_HOWL,
    WOLF_SNARL,
    HEN_CLUCK,
    GOAT_BLEAT,
    CROW_CAW
};

// The survivor's hearing (traits): how far sounds carry, in percent (callers scale their audible distances by it),
// and whether they are panned to their side (Hard of Hearing: no stereo cues).
void set_hearing(int percent, bool stereo);
[[nodiscard]] int hearing_percent();

// Plays a sound with the spec's ±10% random pitch, panned by its horizontal offset from the screen centre
// (-120 .. 120 px). Volume 0..1.
void play(id sound, bn::fixed volume = 1, int screen_x = 0);

// Call once per frame: sounds past the per-frame cap are dropped so Butano's audio command queue never fills.
void new_frame();

}

#endif
