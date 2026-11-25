#include "kx_sfx.h"

#include "bn_algorithm.h"
#include "bn_log.h"
#include "bn_random.h"
#include "bn_sound_item.h"

#include "bn_sound_items.h"

#include "kx_settings.h"

namespace kx::sfx
{

namespace
{
    constexpr int pitch_variation_percent = 10;     // spec: ±10% random pitch
    constexpr int pan_range_px = 120;
    constexpr int max_sounds_per_frame = 5;     // BN_CFG_AUDIO_MAX_COMMANDS is 13 with 6 channels

    int sounds_this_frame = 0;

    bn::random& rng()
    {
        static bn::random result;
        return result;
    }

    const bn::sound_item& item(id sound)
    {
        switch(sound)
        {

        case id::SWING:
            return bn::sound_items::swing;

        case id::BREATH:
            return bn::sound_items::zombie_breath;

        case id::SNARL:
            return bn::sound_items::zombie_growl;

        case id::CHOKE:
            return bn::sound_items::zombie_choke;

        case id::DEATH_GROAN:
            return bn::sound_items::zombie_death;

        case id::BOAR_GRUNT:
            return bn::sound_items::boar_grunt;

        case id::WOLF_HOWL:
            return bn::sound_items::wolf_howl;

        case id::WOLF_SNARL:
            return bn::sound_items::wolf_snarl;

        case id::HEN_CLUCK:
            return bn::sound_items::hen_cluck;

        case id::GOAT_BLEAT:
            return bn::sound_items::goat_bleat;

        case id::CROW_CAW:
            return bn::sound_items::crow_caw;

        case id::HIT_BLUNT:
            return bn::sound_items::hit_blunt;

        case id::HIT_BLADE:
            return bn::sound_items::hit_blade;

        case id::HIT_HEAVY:
            return bn::sound_items::hit_heavy;

        case id::GROAN:
            return bn::sound_items::zombie_moan;

        case id::SHOVE:
            return bn::sound_items::shove;

        case id::STOMP:
            return bn::sound_items::stomp;

        case id::GRAB:
            return bn::sound_items::grab;

        case id::BITE:
            return bn::sound_items::bite;

        case id::BREAK_FREE:
            return bn::sound_items::break_free;

        case id::WEAPON_BREAK:
            return bn::sound_items::weapon_break;

        case id::CHARGE_READY:
            return bn::sound_items::charge_ready;

        case id::STEP_HARD:
            return bn::sound_items::step_hard;

        case id::STEP_SOFT:
            return bn::sound_items::step_soft;

        case id::STEP_WATER:
            return bn::sound_items::step_water;

        case id::POP:
            return bn::sound_items::pop;

        case id::CHIME:
            return bn::sound_items::chime;

        case id::RUMMAGE:
            return bn::sound_items::rummage;

        case id::PICKUP:
            return bn::sound_items::pickup;

        case id::DROP:
            return bn::sound_items::drop;

        case id::ZIP:
            return bn::sound_items::zip;

        case id::DENIED:
            return bn::sound_items::denied;

        case id::GROWL:
            return bn::sound_items::growl;

        case id::SHIVER:
            return bn::sound_items::shiver;

        case id::HEARTBEAT:
            return bn::sound_items::heartbeat;

        case id::COUGH:
            return bn::sound_items::cough;

        case id::MOODLE:
            return bn::sound_items::moodle;

        case id::EAT:
            return bn::sound_items::eat;

        case id::GULP:
            return bn::sound_items::gulp;

        case id::BANDAGE:
            return bn::sound_items::bandage;

        case id::HAMMER:
            return bn::sound_items::hammer;

        case id::GLASS:
            return bn::sound_items::glass;

        case id::BANG:
            return bn::sound_items::bang;

        case id::THUNDER:
            return bn::sound_items::thunder;

        case id::GUNSHOT:
            return bn::sound_items::gunshot;

        case id::ALARM:
            return bn::sound_items::alarm;

        case id::ROTOR:
            return bn::sound_items::rotor;

        case id::STATIC:
            return bn::sound_items::radio_static;

        case id::ENGINE:
            return bn::sound_items::engine;

        case id::STARTER:
            return bn::sound_items::starter;

        case id::CRASH:
            return bn::sound_items::crash;

        case id::CHAIN:
            return bn::sound_items::chain;

        case id::CLICK:
            return bn::sound_items::click;

        case id::SHOT_PISTOL:
            return bn::sound_items::shot_pistol;

        case id::SHOT_SHOTGUN:
            return bn::sound_items::shot_shotgun;

        case id::SHOT_RIFLE:
            return bn::sound_items::shot_rifle;

        case id::RELOAD:
            return bn::sound_items::reload;

        case id::THROW:
            return bn::sound_items::whoosh;

        case id::FIRE:
            return bn::sound_items::fire;

        case id::RING:
            return bn::sound_items::ring;

        case id::DOOR_OPEN:
            return bn::sound_items::door_open;

        case id::DOOR_CLOSE:
            return bn::sound_items::door_close;

        case id::RATTLE:
            return bn::sound_items::rattle;

        case id::LOCKPICK:
            return bn::sound_items::lockpick;

        case id::CROWBAR:
            return bn::sound_items::crowbar;

        case id::CHOP:
            return bn::sound_items::chop;

        case id::TIMBER:
            return bn::sound_items::timber;

        case id::BUBBLES:
            return bn::sound_items::bubbles;

        case id::FLUTTER:
            return bn::sound_items::flutter;

        case id::SHUFFLE:
            return bn::sound_items::shuffle;

        case id::GLASS_STEP:
            return bn::sound_items::glass_step;

        case id::RAIN_ROOF:
            return bn::sound_items::rain_roof;

        case id::WIND:
            return bn::sound_items::wind;

        default:
            return bn::sound_items::swap;
        }
    }
}

namespace
{
    int hearing = 100;
    bool hearing_stereo = true;
}

void set_hearing(int percent, bool stereo)
{
    hearing = percent;
    hearing_stereo = stereo;
}

int hearing_percent()
{
    return hearing;
}

void new_frame()
{
    sounds_this_frame = 0;
}

void play(id sound, bn::fixed volume, int screen_x)
{
    // Playtest 2: the SOUND EFFECTS setting.
    volume *= settings::sound_scale();

    if(sounds_this_frame >= max_sounds_per_frame || volume <= 0)
    {
        return;
    }

    ++sounds_this_frame;

    #if KX_BENCH
        // Playtest 2: the zombies' voices, so a bench log shows each one firing.
        if(sound == id::GROAN || sound == id::BREATH || sound == id::SNARL || sound == id::CHOKE ||
           sound == id::DEATH_GROAN)
        {
            constexpr const char* names[] = { "MOAN", "BREATH", "GROWL", "CHOKE", "DEATH" };
            int index = sound == id::GROAN ? 0 : sound == id::BREATH ? 1 : sound == id::SNARL ? 2 :
                        sound == id::CHOKE ? 3 : 4;
            BN_LOG("KXVOICE ", names[index], " volume=", volume, " x=", screen_x);
        }

        // Animals plan stage 6: the animals' calls too.
        if(sound >= id::BOAR_GRUNT && sound <= id::CROW_CAW)
        {
            constexpr const char* calls[] = { "GRUNT", "HOWL", "SNARL", "CLUCK", "BLEAT", "CAW" };
            BN_LOG("KXCALL ", calls[int(sound) - int(id::BOAR_GRUNT)], " volume=", volume, " x=", screen_x);
        }
    #endif

    int percent = rng().get_int((pitch_variation_percent * 2) + 1) - pitch_variation_percent;
    bn::fixed speed = bn::fixed(100 + percent) / 100;
    bn::fixed panning = hearing_stereo ? bn::fixed(bn::clamp(screen_x, -pan_range_px, pan_range_px)) / pan_range_px :
                                         bn::fixed(0);
    item(sound).play(bn::clamp(volume, bn::fixed(0), bn::fixed(1)), speed, panning);
}

}
