#ifndef KX_EVENTS_H
#define KX_EVENTS_H

#include "bn_fixed_point.h"
#include "bn_optional.h"
#include "bn_sprite_ptr.h"

namespace kx
{

class game;

namespace save
{
    class reader;
    class writer;
}

// World events (spec: World timeline and events): the helicopter (day 6-9) that flies over the player's area and
// drags the hordes along its path, distant gunshots, house and car alarms, rare crash sites with military loot and
// a smoke column visible from far away, each a noise event that moves hordes; and the radio broadcasts of the first
// week (survival tips, lore, the emergency channel), going silent one by one.
namespace events_tuning
{
    constexpr int heli_first_day = 6;
    constexpr int heli_last_day = 9;
    constexpr int heli_first_hour = 8;
    constexpr int heli_last_hour = 17;
    constexpr int heli_half_path = 160;         // tiles either side of the player
    constexpr int heli_speed = 3;               // px per frame
    constexpr int heli_noise = 30;              // tiles, every heli_noise_frames
    constexpr int heli_noise_frames = 30;
    constexpr int heli_audible_tiles = 60;

    // Each in-game hour: the chance of each event, and how far from the player it happens (tiles).
    constexpr int gunshot_percent = 12;
    constexpr int alarm_percent = 6;
    constexpr int crash_percent = 3;
    constexpr int crash_first_day = 3;
    constexpr int max_crashes = 3;
    constexpr int near_tiles = 30;
    constexpr int far_tiles = 70;
    constexpr int crash_near_tiles = 60;
    constexpr int crash_far_tiles = 120;

    constexpr int gunshot_noise = 40;           // spec: gunshot 40
    constexpr int alarm_noise = 60;             // spec: car alarm 60
    constexpr int crash_noise = 40;
    constexpr int alarm_frames = 30 * 60;
    constexpr int crash_zombies = 12;           // soldiers added to the crash site's cell
    constexpr int smoke_days = 3;               // the column shows this long
    constexpr int smoke_visible_tiles = 220;

    constexpr int radio_frames = 6 * 60;        // a broadcast stays on screen
    constexpr int radio_cheer = 5;              // unhappiness taken off (spec: radio restores morale)
}

class world_events
{

public:
    // A new world: nothing has happened.
    void reset();

    // Once per frame while playing: the helicopter, alarms and gunshots in progress, smoke columns.
    void update(game& owner);

    // Every in-game hour while awake: maybe something happens.
    void hour(game& owner);

    // Turns the radio on: the broadcast for now (needs a battery carried).
    void listen(game& owner);

    // Turns a television on (milestone 13): the radio's broadcasts and a few of its own while powered, else static.
    void watch_tv(game& owner, bool powered);

    void hide();

    // Debug and benches: starts an event now (0 helicopter, 1 gunshots, 2 alarm, 3 crash site).
    void trigger(game& owner, int which);

    // Debug and benches: what is going on.
    [[nodiscard]] bool helicopter_flying() const
    {
        return _heli.has_value();
    }

    [[nodiscard]] int crashes() const
    {
        return _crash_count;
    }

    [[nodiscard]] bool smoke_shown() const
    {
        return _smoke.has_value();
    }

    void save(save::writer& out) const;
    void load(save::reader& in);

private:
    struct crash_site
    {
        int16_t tile_x = 0;
        int16_t tile_y = 0;
        int16_t day = 0;
    };

    // Saved.
    bool _heli_done = false;
    uint8_t _crash_count = 0;
    crash_site _crashes[events_tuning::max_crashes];

    // In progress.
    bn::optional<bn::sprite_ptr> _heli;
    bn::fixed_point _heli_position;
    bn::fixed_point _heli_step;
    int _heli_frames = 0;
    bn::optional<bn::sprite_ptr> _smoke;
    bn::fixed_point _event_position;
    int _alarm_frames = 0;
    int _shots = 0;
    int _shot_timer = 0;
    int _frame = 0;

    void _start_heli(game& owner);
    void _crash(game& owner, int day);
    [[nodiscard]] bn::fixed_point _somewhere(game& owner, int near, int far) const;
    void _sound(game& owner, int sound, const bn::fixed_point& position, int audible_tiles);
    void _update_smoke(game& owner);
};

}

#endif
