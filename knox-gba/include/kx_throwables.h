#ifndef KX_THROWABLES_H
#define KX_THROWABLES_H

#include "bn_camera_ptr.h"
#include "bn_fixed_point.h"
#include "bn_sprite_ptr.h"
#include "bn_vector.h"

#include "kx_items.h"

namespace kx
{

class game;

// Throwables and fire (milestone 12.6 stage 5, docs/milestone-12.6-plan.md; spec: Combat > Throwables). A
// throwable in the off hand is thrown while aiming (R1) with A when the main hand holds no gun. It flies in an arc
// up to 6 tiles, stopping short of walls, and on landing: a bottle smashes (noise 12), an alarm clock rings for
// 20 s (noise 10 every 2 s) and is left there, a Molotov smashes and sets a 3x3 patch of tiles burning for 15 s.
// Zombies in the fire lose 1 health every half second and keep burning 3 s after walking out; stepping in gives the
// survivor a Burn wound on a leg. At night a fire lights its patch like a campfire.
namespace throw_tuning
{
    constexpr int range_px = 96;                // 6 tiles
    constexpr int frames_per_tile = 3;          // flight time
    constexpr int least_frames = 10;
    constexpr int arc_px = 20;                  // the arc's height over a full-range throw
    constexpr int hand_height = 10;             // let go at chest height
    constexpr int cooldown = 30;                // frames between throws
    constexpr int glass_noise = 12;             // tiles (spec: breaking glass)
    constexpr int alarm_frames = 1200;          // rings 20 s
    constexpr int alarm_interval = 120;         // noise every 2 s
    constexpr int alarm_noise = 10;
    constexpr int fire_frames = 900;            // the patch burns 15 s
    constexpr int fire_radius = 1;              // tiles round the centre: 3x3
    constexpr int burn_interval = 30;           // zombies in it lose health every half second
    constexpr int burn_damage = 1;
    constexpr int burn_ticks_after = 6;         // and keep burning 3 s after walking out
    constexpr int player_burn_frames = 60;      // a Burn wound each second the survivor stands in it
    constexpr int crackle_interval = 45;
}

class throw_manager
{

public:
    static constexpr int max_flying = 2;
    static constexpr int max_alarms = 2;
    static constexpr int max_fires = 2;
    static constexpr int flames_per_fire = 9;

    explicit throw_manager(const bn::camera_ptr& camera);

    // Where a throw from the feet towards a point lands: the point (at most range_px away), or short of the first
    // wall on the way.
    [[nodiscard]] static bn::fixed_point landing(const bn::fixed_point& from, const bn::fixed_point& towards);

    // Throws a throwable from the feet to a landing point (from landing()).
    void launch(game& owner, item_id id, const bn::fixed_point& from, const bn::fixed_point& to);

    // Flights, ringing clocks, fires and burns (skipped during hitstop, like the zombies).
    void update(game& owner);

    // Time skipped ahead (sleeping, cooking, reading): everything in the air lands, clocks stop ringing (left on
    // the ground) and fires burn out.
    void settle(game& owner);

    // Burning patches: their centres (world pixels) for the night's lights; how many were written.
    int fire_centres(bn::fixed_point* centres, int capacity) const;

    // True if a fire burns within `tiles` tiles of a tile (on either axis).
    [[nodiscard]] bool fire_near(int tile_x, int tile_y, int tiles) const;

    // Statistics for bench 11.
    [[nodiscard]] int thrown() const
    {
        return _thrown;
    }

    [[nodiscard]] int smashed() const
    {
        return _smashed;
    }

    [[nodiscard]] int rings() const
    {
        return _rings;
    }

    [[nodiscard]] int fires_started() const
    {
        return _fires_started;
    }

    [[nodiscard]] int burn_kills() const
    {
        return _burn_kills;
    }

    [[nodiscard]] int player_burns() const
    {
        return _player_burns;
    }

    [[nodiscard]] int fires_burning() const
    {
        return _fires.size();
    }

    [[nodiscard]] int alarms_ringing() const
    {
        return _alarms.size();
    }

private:
    struct flight
    {
        bn::sprite_ptr sprite;
        bn::sprite_ptr shadow;
        bn::fixed_point from;
        bn::fixed_point to;
        item_id id;
        int frame;
        int frames;
        int arc;
    };

    struct alarm
    {
        bn::sprite_ptr sprite;
        bn::fixed_point position;
        int frames_left;
    };

    struct fire
    {
        bn::vector<bn::sprite_ptr, flames_per_fire> flames;
        int tile_x;
        int tile_y;
        int frames_left;
    };

    bn::camera_ptr _camera;
    bn::vector<flight, max_flying> _flying;
    bn::vector<alarm, max_alarms> _alarms;
    bn::vector<fire, max_fires> _fires;
    int _frame = 0;
    int _player_burn_timer = 0;
    bool _zombies_burning = false;
    int _thrown = 0;
    int _smashed = 0;
    int _rings = 0;
    int _fires_started = 0;
    int _burn_kills = 0;
    int _player_burns = 0;

    void _land(game& owner, item_id id, const bn::fixed_point& at);
    void _start_fire(game& owner, int tile_x, int tile_y);
    void _stop_alarm(game& owner, int index);
    void _update_fires(game& owner);
};

}

#endif
