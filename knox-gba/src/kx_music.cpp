#include "kx_music.h"

#include "bn_dmg_music.h"
#include "bn_dmg_music_item.h"
#include "bn_fixed.h"
#include "bn_random.h"

#include "kx_settings.h"

#include "bn_dmg_music_items_calm.h"
#include "bn_dmg_music_items_chase.h"
#include "bn_dmg_music_items_death.h"
#include "bn_dmg_music_items_night.h"
#include "bn_dmg_music_items_title.h"

namespace kx::music
{

using namespace music_tuning;

namespace
{
    track current = track::NONE;
    int fade = 0;                   // frames left of a fade-out; 0: none
    int fade_from = 100;            // the volume (percent) the fade started at
    int gap = first_gap_frames;     // frames of silence left before the next quiet tune
    int calm_frames = 0;            // frames since the last zombie gave up the chase
    bool entering = false;          // the fade is the title's, leaving for a world: the first gap follows
    bool master_set = false;

    bn::random& rng()
    {
        static bn::random result;
        return result;
    }

    void set_volume_percent(int percent)
    {
        // Playtest 2: the MUSIC setting scales every level.
        bn::dmg_music::set_volume((bn::fixed(bn::max(percent, 0)) / 100) * settings::music_scale());
    }

    void start(track value, bool loop, int volume_percent)
    {
        // The music sits under the sound effects: half the DMG mixing level.
        if(! master_set)
        {
            bn::dmg_music::set_master_volume(bn::dmg_music_master_volume::HALF);
            master_set = true;
        }

        switch(value)
        {

        // Each song's own speed in frames per row (tools/assets/music.py; its first row sets it again).
        case track::TITLE:
            bn::dmg_music_items::title.play(10, loop);
            break;

        case track::CALM:
            bn::dmg_music_items::calm.play(14, loop);
            break;

        case track::NIGHT:
            bn::dmg_music_items::night.play(18, loop);
            break;

        case track::CHASE:
            bn::dmg_music_items::chase.play(6, loop);
            break;

        default:
            bn::dmg_music_items::death.play(16, loop);
            break;
        }

        set_volume_percent(volume_percent);
        current = value;
        fade = 0;
        fade_from = volume_percent;
    }

    void stop_now()
    {
        if(bn::dmg_music::playing())
        {
            bn::dmg_music::stop();
        }

        current = track::NONE;
        fade = 0;
    }

    void begin_fade()
    {
        if(current != track::NONE && fade == 0)
        {
            fade = fade_frames;
        }
    }

    [[nodiscard]] int next_gap()
    {
        return gap_min_frames + rng().get_int(gap_extra_frames);
    }
}

void apply_volume()
{
    // A fade sets its own level each update (scaled the same way).
    if(current != track::NONE && fade == 0 && bn::dmg_music::playing())
    {
        set_volume_percent(fade_from);
    }
}

void title()
{
    if(current != track::TITLE || ! bn::dmg_music::playing())
    {
        start(track::TITLE, true, 100);
    }
}

void enter_world()
{
    begin_fade();
    entering = current != track::NONE;
    gap = first_gap_frames;
    calm_frames = 0;
}

void update(const situation& now, int frames)
{
    // A fade in progress: down to silence, then the next gap.
    if(fade > 0)
    {
        fade = bn::max(fade - frames, 0);
        set_volume_percent((fade_from * fade) / fade_frames);

        if(fade == 0)
        {
            stop_now();
            gap = entering ? first_gap_frames : next_gap();
            entering = false;
        }
    }

    // The death screen: its tune once, whatever was playing.
    if(now.dead)
    {
        if(current != track::DEATH)
        {
            start(track::DEATH, false, death_volume_percent);
        }

        return;
    }

    // A tune that ends by itself (the quiet ones, a title cut short) leaves silence.
    if(current != track::NONE && ! bn::dmg_music::playing())
    {
        current = track::NONE;
        fade = 0;
        gap = next_gap();
    }

    // Asleep: nothing new starts; what plays fades.
    if(now.asleep)
    {
        if(current != track::NONE)
        {
            begin_fade();
        }

        return;
    }

    // A horde after you: the chase loop at once, held a while after the last one gives up, then faded.
    if(now.chasers >= chase_start)
    {
        calm_frames = 0;

        if(current != track::CHASE || fade > 0)
        {
            start(track::CHASE, true, 100);
        }

        return;
    }

    if(current == track::CHASE)
    {
        if(now.chasers > 0)
        {
            calm_frames = 0;
        }
        else if((calm_frames += frames) >= chase_hold_frames)
        {
            begin_fade();
        }

        return;
    }

    // Leaving the title for a world fades it (enter_world); otherwise, in silence, a quiet tune now and then.
    if(current == track::NONE && (gap -= frames) <= 0)
    {
        if(now.night)
        {
            start(track::NIGHT, false, night_volume_percent);
        }
        else
        {
            start(track::CALM, false, calm_volume_percent);
        }
    }
}

track playing()
{
    return current;
}

bool sounding()
{
    return bn::dmg_music::playing();
}

const char* track_name(track value)
{
    constexpr const char* names[] = { "NONE", "TITLE", "CALM", "NIGHT", "CHASE", "DEATH" };
    return names[int(value)];
}

}
