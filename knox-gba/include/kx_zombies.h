#ifndef KX_ZOMBIES_H
#define KX_ZOMBIES_H

#include "bn_camera_ptr.h"
#include "bn_optional.h"
#include "bn_fixed_point.h"
#include "bn_random.h"
#include "bn_sprite_palette_ptr.h"
#include "bn_sprite_ptr.h"
#include "bn_vector.h"

#include "kx_combat_tuning.h"
#include "kx_sfx.h"
#include "kx_zombie_outfits.h"

namespace kx
{

class game;

// Milestone 14 sound pass: the nearest moving zombie's feet are heard this often, from this close.
namespace zombie_sound_tuning
{
    constexpr int shuffle_interval = 26;        // frames
    constexpr int shuffle_px = 56;

    // Playtest 2, the user's zombie sounds (tools/assets/zombie_sounds.py). The files peak at -3 dB; each plays at
    // its specified final volume (moan -12 dB, breath -16, growl -6, choke -8, death -9), that is at 10^((final +
    // 3) / 20). Times in frames.
    constexpr int moan_min_frames = 8 * 60;     // one moan every 8-15 s, from a zombie in earshot
    constexpr int moan_extra_frames = 7 * 60;
    constexpr int moan_reach_px = 160;          // from the screen's middle, scaled by the survivor's hearing
    constexpr int breath_min_frames = 4 * 60;   // the nearest unaware zombie breathes every 4-7 s
    constexpr int breath_extra_frames = 3 * 60;
    constexpr int breath_reach_px = 5 * 16;
    constexpr int snarl_gap_frames = 40;        // growls on noticing you are at least this far apart
    constexpr int choke_gap_frames = 8;
    constexpr int heard_reach_px = 200;         // growls, chokes and death groans from at most this far off centre
    inline constexpr bn::fixed moan_volume = bn::fixed(0.355);      // -9 dB
    inline constexpr bn::fixed breath_volume = bn::fixed(0.224);    // -13 dB
    inline constexpr bn::fixed snarl_volume = bn::fixed(0.708);     // -3 dB
    inline constexpr bn::fixed choke_volume = bn::fixed(0.562);     // -5 dB
    inline constexpr bn::fixed death_volume = bn::fixed(0.501);     // -6 dB
}

enum class zombie_state : uint8_t
{
    IDLE,
    WANDER,
    CHASE,
    SEARCH,
    WINDUP,         // telegraphed lunge wind-up (spec: 18 frames)
    LUNGE,
    RECOVER,
    GRAB,
    STAGGER,
    DOWN,           // knocked down; becomes a crawler unless finished with a stomp
    CRAWL,
    DYING
};

// A melee attack resolved against the zombies (one swing, shove or stomp).
struct attack_info
{
    bn::fixed_point origin;         // attacker's feet
    bn::fixed_point direction;      // unit facing
    int reach = 0;
    int arc_degrees = 0;
    int max_targets = 1;
    int damage = 0;                 // health points; >= 100 kills outright
    bn::fixed knockback;
    int stagger_frames = 0;
    int swing_id = 0;               // each zombie is hit at most once per swing id
    weapon weapon_used = weapon::FISTS;
    bool stealth_capable = false;   // crouched with a blade
    bool shove = false;
};

struct attack_result
{
    int hits = 0;
    int kills = 0;
    bool stealth_kill = false;
};

// A gunshot (kx_guns): a line from the muzzle that hits the first zombie within hit_radius of it.
struct shot_info
{
    bn::fixed_point origin;         // the muzzle, at body height
    bn::fixed_point direction;      // unit
    int range = 0;                  // px
    int hit_radius = 0;
    int damage = 0;
    bn::fixed knockback;
    int stagger_frames = 0;
};

// Up to 40 individually simulated zombies near the player (spec: Zombies); the rest of the world's zombies are
// head counts in the macro cells (kx_hordes).
class zombie_manager
{

public:
    static constexpr int max_zombies = 40;

    explicit zombie_manager(const bn::camera_ptr& camera);

    void spawn(const bn::fixed_point& position, bn::random& random);

    // A past survivor's zombie (kx_survivors), in the clothes they died in; false if the pool is full.
    bool spawn_survivor(const bn::fixed_point& position, int id, const item_id* worn, bn::random& random);

    // Test helper: the last spawned zombie starts as a crawler.
    void make_last_crawler();
    void clear();

    // Turns zombies farther than distance (pixels, on either axis) from a point, or standing in chunks that are
    // not generated, back into the head count of the macro cell they are in; dying ones finish first. At most
    // `limit` per call (each frees a sprite); returns how many went.
    int dissolve(const bn::fixed_point& center, int distance, int limit = max_zombies);

    // Simulation step (skipped during hitstop).
    void update(game& owner);

    // Visual step (every frame, also during hitstop): flash, sprites, depth sorting. Off-screen zombies only
    // move their sprite.
    void update_visuals(int camera_left, int camera_top);

    attack_result attack(game& owner, const attack_info& info);

    // Index of the downed or crawling zombie nearest to the feet within reach, or -1.
    [[nodiscard]] int stomp_target(const bn::fixed_point& feet, int reach) const;
    void stomp(game& owner, int index);

    // Zombies on screen that are after the player (panic).
    [[nodiscard]] int aware_on_screen(int camera_left, int camera_top) const;

    // Zombies anywhere around that are chasing or attacking the player (the music director; milestone 14).
    [[nodiscard]] int chasing() const;

    // Zombies on screen that have not noticed the player (Sneaking XP).
    [[nodiscard]] int unaware_on_screen(int camera_left, int camera_top) const;

    // True if a zombie on its feet is within the distance.
    [[nodiscard]] bool standing_within(const bn::fixed_point& feet, int distance) const;

    // True if a downed or crawling zombie lies under the feet (sprinting over it trips the player).
    [[nodiscard]] bool crawler_underfoot(const bn::fixed_point& feet) const;

    // Player broke free: every grabbing zombie staggers back.
    void release_grabs(game& owner);

    void hear(const bn::fixed_point& position, int radius_px);

    // Aiming: the nearest zombie standing or crawling within range and the arc round a direction, with a clear
    // line to it; -1 if none. origin is at body height (as a shot's).
    [[nodiscard]] int aim_target(const bn::fixed_point& origin, const bn::fixed_point& direction, int range,
                                 int arc_degrees) const;

    // A shot: hits = 1 if a zombie was hit (walls stop it).
    attack_result shoot(game& owner, const shot_info& info);

    // Fire (kx_throwables): zombies with their feet inside a box of world pixels (inclusive) burn for at least
    // `ticks` more burn ticks.
    void ignite(int left, int top, int right, int bottom, int ticks);

    // A burn tick: each burning zombie loses `damage` health (and a tick). Returns the kills; still_burning gets
    // how many burn on.
    int burn_tick(game& owner, int damage, int& still_burning);

    // Moves the zombies standing inside a box of world pixels (a car that has just parked) to its nearest edge.
    void push_out(int left, int top, int right, int bottom);

    [[nodiscard]] int count() const
    {
        return _zombies.size();
    }

    [[nodiscard]] const bn::fixed_point& position(int index) const
    {
        return _zombies[index].position;
    }

    // Benches: a zombie's walking speed (sprinter, fast shambler or shambler).
    [[nodiscard]] bn::fixed speed(int index) const
    {
        return _zombies[index].speed;
    }

    // The past survivor a zombie was, or -1.
    [[nodiscard]] int survivor(int index) const
    {
        return _zombies[index].survivor;
    }

    // How far (tiles, on the larger axis) the farthest chasing zombie is from a point; 0 if none chases. The flow
    // field only needs to reach that far.
    [[nodiscard]] int chase_reach(const bn::fixed_point& center) const;

    // Statistics for the bench log.
    [[nodiscard]] int total_hits() const
    {
        return _total_hits;
    }

    [[nodiscard]] int total_grabs() const
    {
        return _total_grabs;
    }

    [[nodiscard]] int total_knockdowns() const
    {
        return _total_knockdowns;
    }

    [[nodiscard]] int total_stomps() const
    {
        return _total_stomps;
    }

private:
    struct zombie
    {
        explicit zombie(bn::sprite_ptr&& sprite_ptr) :
            sprite(bn::move(sprite_ptr))
        {
        }

        bn::sprite_ptr sprite;
        bn::fixed_point position;
        bn::fixed_point velocity;
        bn::fixed_point knock;
        bn::fixed_point facing;         // unit vector
        bn::fixed_point target;
        bn::fixed_point lunge_direction;
        bn::fixed speed;
        int timer = 0;
        int cooldown = 0;
        int health = 1;
        int flash = 0;
        int anim = 0;
        int swing_id = -1;
        int reach_frames = 0;
        int sprite_frame = -1;
        int sprite_x = -32768;
        int sprite_y = -32768;
        zombie_state state = zombie_state::IDLE;
        zombie_outfit outfit = zombie_outfit::CIVILIAN;
        int8_t survivor = -1;                           // a past survivor's zombie (kx_survivors)
        uint8_t bash_ticks = 0;                         // decisions since its last hit on a window or barricade
        uint8_t burning = 0;                            // burn ticks left (kx_throwables)
        bn::optional<bn::sprite_palette_ptr> palette;   // its own clothes
        bool crawler = false;
        uint8_t body = 0;                               // playtest 1: which body of the sheet (kx_sprite_frames.h)
        bool mirror_front = false;                      // walking up or down drawn mirrored
    };

    // Tuning values read once per frame instead of once per zombie.
    struct frame_params
    {
        bn::fixed lunge_speed;
        bn::fixed crawl_speed;
        int interval;
        int lunge_range;
        int grab_range;
        int windup;
        int lunge_frames;
        int recover;
        int search;
        int search_max;
        int down;
        int track_tiles;
        int sight;
        int chase_sight;
        int cone;
        int heard_percent;
        int crawl_grab_delay;
    };

    bn::camera_ptr _camera;
    // Time spent searching the last known position: random between the tuned bounds.
    [[nodiscard]] int _search_frames();

    // Chasing a seen player straight (a unit direction) would walk into something solid next (milestone 13:
    // zombies don't swim).
    static constexpr int straight_probe_px = 12;

    // A dying zombie slides with the hit, then eases over its last frames onto the spot in its tile where the corpse
    // decal draws the body (playtest 1: the corpse used to jump to the 16 px grid).
    static constexpr int dying_frames = 24;
    static constexpr int dying_ease_frames = 10;
    [[nodiscard]] static bool _blocked_ahead(const bn::fixed_point& from, const bn::fixed_point& direction);

    bn::vector<bn::sprite_palette_ptr, int(zombie_outfit::COUNT)> _outfit_palettes;
    bn::sprite_palette_ptr _flash_palette;
    bn::vector<zombie, max_zombies> _zombies;
    bool _shaded_dark = false;      // the night shading the sprites have (kx_light)
    bn::random _random;
    bn::random _voice_random;       // the voices' timing and picks, apart from the game's randomness
    int _moan_timer = zombie_sound_tuning::moan_min_frames;
    int _breath_timer = zombie_sound_tuning::breath_min_frames;
    int _snarl_gap = 0;
    int _choke_gap = 0;
    int _frame = 0;
    int _total_hits = 0;
    int _total_grabs = 0;
    int _total_knockdowns = 0;
    int _total_stomps = 0;

    void _think(game& owner, zombie& value, bool decide, const frame_params& params);
    void _hit(game& owner, zombie& value, const attack_info& info, bool kill, bool knockdown);
    void _kill(game& owner, zombie& value);
    // Corpse sprites (playtest 2): on screen, zombie corpses are drawn as their zombie's lying frame in its outfit
    // and body, over the corpse decal (which has the same outline, in one set of colours). A pool of sprites goes
    // to the corpses in view; a killed zombie's corpse gets one at once, so the swap from the dying body can't be
    // seen.
    struct corpse_sprite
    {
        bn::sprite_ptr sprite;
        int16_t tile_x;
        int16_t tile_y;
        bool seen;
    };

    static constexpr int max_corpse_sprites = 16;
    static constexpr int corpse_scan_frames = 8;
    static constexpr int corpse_view_px = 136;      // a corpse's tile within this of the screen's middle (x), and
    static constexpr int corpse_view_y_px = 96;     // this (y), keeps or gets a sprite
    static constexpr int corpse_z_order = 3;        // behind everything sorted by depth (depth_z: -21 .. 2)
    bn::vector<corpse_sprite, max_corpse_sprites> _corpses;

    void _update_corpses(game& owner);
    void _show_corpse(game& owner, int tile_x, int tile_y, int outfit, int body);

    // The voices (playtest 2): the shared moan and breath timers; a voice from one zombie, if it can be heard.
    void _update_voices(game& owner);
    void _voice(game& owner, const zombie& value, sfx::id sound, bn::fixed volume) const;
    void _set_state(zombie& value, zombie_state state, int timer);
    // True if the zombie notices the player (heard or seen); in_sight tells whether it actually sees them.
    [[nodiscard]] bool _perceives(game& owner, const zombie& value, bool chasing, const frame_params& params,
                                  bool& in_sight) const;

    // Hot loops (IWRAM): movement with tile collision, knockback and separation.
    BN_CODE_IWRAM void _move_all(const bn::fixed_point& player_feet, int interval);
};

}

#endif
