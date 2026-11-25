#ifndef KX_ANIMALS_H
#define KX_ANIMALS_H

#include "bn_camera_ptr.h"
#include "bn_fixed_point.h"
#include "bn_random.h"
#include "bn_sprite_ptr.h"
#include "bn_vector.h"

#include "kx_bench.h"
#include "kx_combat_tuning.h"
#include "kx_guns.h"
#include "kx_sfx.h"

namespace kx
{

class game;

namespace save
{
    class reader;
    class writer;
}

// The wild (milestone 13, docs/milestone-13-plan.md stage 4). Deer graze in woods and fields away from towns and
// flee from the survivor and from loud noise; rabbits hop in grass and forest by day; flocks of birds sit in fields
// and on roads and fly off with a flutter (a noise of their own) when anyone comes near or a gun is fired. A deer
// killed leaves a carcass to butcher with a knife or an axe; a rabbit killed leaves its meat on the ground. Live
// animals are not saved (they come back from the seed); the cells where deer were killed are, so a hunted cell stays
// empty for a week.
namespace animal_tuning
{
    constexpr int max_deer = 2;
    constexpr int max_boar = 1;
    constexpr int max_animals = 6;              // the plan's cap on ground animals; a wolf pack needs 2-3 free
    constexpr int max_birds = 5;
    constexpr int least_birds = 3;
    constexpr int max_zombies_for_birds = 30;   // flocks only appear when fewer zombies are about (sprite budget)

    constexpr int spawn_interval = 90;          // frames between tries
    constexpr int deer_cell_percent = 50;       // of wild and farm cells, per day, that have deer
    constexpr int deer_spawn_near = 10;         // tiles from the survivor, off screen
    constexpr int deer_spawn_far = 14;
    constexpr int rabbit_percent = 30;          // of tries by day on grass or forest
    constexpr int rabbit_spawn_near = 7;
    constexpr int rabbit_spawn_far = 10;
    constexpr int bird_percent = 25;            // of tries
    constexpr int bird_spawn_near = 9;
    constexpr int bird_spawn_far = 12;
    constexpr int bird_cooldown = 1200;         // frames after a flock leaves before another may land
    constexpr int despawn_tiles = 22;           // live animals this far away go
    constexpr int carcass_despawn_tiles = 26;

    // Senses (tiles): the survivor standing, crouched; in forest and while a deer grazes head down they notice less.
    constexpr int deer_notice = 8;
    constexpr int deer_notice_crouched = 3;
    constexpr int deer_forest_percent = 75;
    constexpr int deer_grazing_percent = 50;
    constexpr int rabbit_notice = 4;
    constexpr int rabbit_notice_crouched = 1;
    constexpr int bird_notice = 5;
    constexpr int bird_notice_crouched = 3;
    constexpr int bird_zombie_px = 48;          // a zombie this close scares a flock
    constexpr int scare_noise = 6;              // noises this loud (radius, tiles) scare animals within them

    // Speeds (px per frame) and how long they flee.
    constexpr int deer_walk_milli = 350;
    constexpr int deer_flee_milli = 2250;       // sprinting speed
    constexpr int rabbit_hop_milli = 900;
    constexpr int rabbit_flee_milli = 2500;
    constexpr int bird_fly_milli = 1500;
    constexpr int flee_frames = 200;
    constexpr int bird_fly_frames = 150;
    constexpr int bird_noise = 6;               // a flock taking off is heard this far (tiles)

    // Health and damage: 2 rifle or shotgun hits, 3 pistol hits, 2 spear or axe hits, 4 other melee hits.
    constexpr int deer_health = 12;
    constexpr int rabbit_health = 1;
    constexpr int long_gun_damage = 6;
    constexpr int pistol_damage = 4;
    constexpr int heavy_melee_damage = 6;       // spear, axe
    constexpr int melee_damage = 3;
    constexpr int deer_hit_radius = 10;         // px round the body a shot must pass (a deer is a big target)
    constexpr int rabbit_hit_radius = 6;
    constexpr int body_height = 6;              // the body's centre above the feet

    // Butchering a deer (hold A with a knife or an axe in hand).
    constexpr int butcher_frames = 240;
    constexpr int butcher_reach = 14;
    constexpr int venison = 4;                  // raw meat
    constexpr int hides = 1;
    constexpr int butcher_xp = 15;              // Foraging

    // A cell where a deer was killed has none for this many days; the list keeps the latest kills (the world save
    // block has room for 8).
    constexpr int deer_return_days = 7;
    constexpr int max_kills = 8;

    // The wild boar (animals plan stage 2): alone in forest, day and night. It walks off from afar like a deer, but
    // within 2 tiles, or when wounded near you, it lowers its head and charges in a straight line: a gore (a leg
    // laceration, knocked down and back), then it turns away and won't charge again for a while.
    constexpr int boar_cell_percent = 35;       // of wild cells, per day, with a boar (only on forest ground)
    constexpr int boar_spawn_near = 10;
    constexpr int boar_spawn_far = 14;
    constexpr int boar_notice = 6;              // tiles: it moves off
    constexpr int boar_notice_crouched = 2;
    constexpr int boar_charge_tiles = 2;        // closer than this and it charges, crouched or not
    constexpr int boar_wounded_charge_tiles = 7;   // wounded from closer than this it charges, else it runs
    constexpr int boar_walk_milli = 300;
    constexpr int boar_flee_milli = 1700;
    constexpr int boar_charge_milli = 2700;     // faster than a sprint: step aside, don't outrun it
    constexpr int boar_windup_frames = 24;      // head down, pawing (a wounded boar: half)
    constexpr int boar_charge_frames = 48;      // the run, about 8 tiles, in the direction it set off
    constexpr int boar_gore_px = 11;            // its feet this close to yours
    constexpr int boar_knock_milli = 3000;      // the survivor thrown back along the charge
    constexpr int boar_knock_frames = 30;       // and down this long
    constexpr int boar_turn_frames = 120;       // it trots off after a gore or a miss
    constexpr int boar_calm_frames = 300;       // and charges no more for this long
    constexpr int boar_trouser_percent = 50;    // of the trousers' scratch defence a tusk meets
    constexpr int boar_health = 16;             // 3 long-gun or 4 pistol hits, 3 spear or axe hits, 6 other melee
    constexpr int boar_hit_radius = 9;
    constexpr int boar_meat = 6;
    constexpr int boar_hides = 1;

    // Wolves (animals plan stage 3): packs of 2-3 in wild cells at night. They roam; one noticing you sets the
    // pack on you. They circle a few tiles out, and every few seconds one darts in to bite (a laceration on an arm
    // or a leg, never infected) and backs off. A wolf killed sends the rest running; a fire within 4 tiles of you
    // keeps them circling further out, and none comes within 3 tiles of one. Zombies and wolves ignore each other.
    constexpr int max_wolves = 3;
    constexpr int least_wolves = 2;
    constexpr int wolf_cell_percent = 40;       // of wild cells, per night, with a pack
    constexpr int wolf_spawn_near = 11;
    constexpr int wolf_spawn_far = 14;
    constexpr int wolf_notice = 7;              // tiles
    constexpr int wolf_notice_crouched = 3;
    constexpr int wolf_lose_tiles = 16;         // the survivor this far from every wolf: the hunt is off
    constexpr int wolf_walk_milli = 500;
    constexpr int wolf_run_milli = 2000;        // getting into place round the survivor
    constexpr int wolf_circle_milli = 900;      // pacing round
    constexpr int wolf_dart_milli = 2600;
    constexpr int wolf_back_milli = 1600;
    constexpr int wolf_flee_milli = 2400;
    constexpr int wolf_circle_tiles = 4;        // the ring they pace, round the survivor
    // Playtest 3 (they moved erratically): a circling wolf runs at a point going round the ring, at a speed that
    // grows with the gap (gap / wolf_pursuit_frames a frame, up to wolf_circle_milli, wolf_run_milli when far), and
    // its speed turns a wolf_turn_frames-th of the way to that each frame, so it never stops and starts.
    constexpr int wolf_pursuit_frames = 8;
    constexpr int wolf_turn_frames = 4;
    constexpr int wolf_face_milli = 300;        // it only turns its picture round when moving across this fast
    constexpr int view_still_milli = 80;        // any animal moving slower keeps its view (side, front, back)
    constexpr int wolf_fire_ring_tiles = 6;     // the ring when a fire burns within wolf_fire_tiles of the survivor
    constexpr int wolf_fire_tiles = 4;
    constexpr int wolf_fire_shy_tiles = 3;      // a wolf this close to a fire backs off
    constexpr int wolf_first_dart = 120;        // frames from noticing to the first dart
    constexpr int wolf_dart_gap = 150;          // and between darts (plus up to wolf_dart_gap_random)
    constexpr int wolf_dart_gap_random = 90;
    constexpr int wolf_dart_frames = 45;        // a dart that hasn't reached you by then turns back
    constexpr int wolf_back_frames = 45;
    constexpr int wolf_bite_px = 10;
    constexpr int wolf_bite_push_milli = 1500;  // a small shove, no knock-down
    constexpr int wolf_cover_percent = 100;     // of sleeves' and trousers' scratch defence a bite meets
    constexpr int wolf_fear_frames = 600;       // a wolf killed: the rest run this long, then leave
    constexpr int wolf_health = 8;              // 2 gun hits, 2 spear or axe hits, 3 other melee
    constexpr int wolf_hit_radius = 8;
    constexpr int wolf_meat = 2;
    constexpr int wolf_hides = 1;

    // Farm animals (animals plan stage 4): most farms keep 2-4 hens and 1-2 goats about the barn door. Hens peck
    // about by day and flap off slowly when you're close; walk up crouched (or run one down) and A catches one: raw
    // meat. Goats graze and trot off from someone running at them, but let you walk up; A beside one with an empty
    // bottle milks it, once a day each. A goat killed is butchered like a deer. Hens and goats killed or caught
    // stay gone (the farm's tally is saved), and foraging near a farm with hens left finds eggs.
    constexpr int farm_animal_percent = 75;     // of farms with hens and goats
    constexpr int least_hens = 2;
    constexpr int most_hens = 4;
    constexpr int least_goats = 1;
    constexpr int most_goats = 2;
    constexpr int farm_spawn_tiles = 18;        // the yard this close: its animals come out
    // Stage 7: a farm's animals come out one a frame (a new sprite costs up to 250 of a frame's 4389 ticks; six at
    // once with the yard's lookup made a quarter-frame spike).
    constexpr int hen_spread_tiles = 3;         // round the barn door
    constexpr int goat_spread_tiles = 5;
    constexpr int hen_leash_tiles = 5;          // they amble back towards the door beyond this
    constexpr int goat_leash_tiles = 7;
    constexpr int hen_notice = 2;               // tiles
    constexpr int hen_notice_crouched = 1;
    constexpr int goat_notice = 2;              // tiles, but only from someone jogging or sprinting at it
    constexpr int goat_calm_milli = 1250;       // the survivor's speed (px per frame) a goat lets come up to it
    constexpr int hen_walk_milli = 350;
    constexpr int hen_flee_milli = 1000;        // a flapping run, slower than the survivor's jog
    constexpr int hen_flee_frames = 45;
    constexpr int goat_walk_milli = 300;
    constexpr int goat_flee_milli = 1300;
    constexpr int goat_flee_frames = 90;
    constexpr int hen_health = 1;
    constexpr int goat_health = 8;              // 2 gun hits, 2 spear or axe hits, 3 other melee
    constexpr int hen_hit_radius = 6;
    constexpr int goat_hit_radius = 9;
    constexpr int hen_body_frames = 600;        // a dead hen lies this long
    constexpr int catch_reach = 14;             // px from the point in front of the survivor
    constexpr int milk_reach = 16;
    constexpr int goat_meat = 3;
    constexpr int goat_hides = 1;
    constexpr int egg_tiles = 10;               // foraging this close to a yard with hens left finds eggs
    constexpr int max_tallies = 8;              // farms whose losses and milking days are saved (oldest goes)
    constexpr int max_known_farms = 4;          // farms near the survivor this session (from the seed, not saved)

    // Crows (animals plan stage 5): by day, 2-4 glide down to a zombie corpse lying 5-14 tiles from you (they take
    // the flock's slots, so never with a songbird flock), hop and peck about it, and take off cawing when you come
    // within 4 tiles (2 crouched) or at a loud noise: a noise of their own, heard further than a flock's, so zombies
    // come. Zombies don't bother them. Nothing to gain; a warning, and a giveaway.
    constexpr int crow_percent = 40;            // of tries by day with a corpse in range
    constexpr int crow_corpse_near = 5;         // tiles from the survivor
    constexpr int crow_corpse_far = 14;
    constexpr int least_crows = 2;
    constexpr int most_crows = 4;
    constexpr int crow_spread_px = 12;          // round the corpse
    constexpr int crow_landing_frames = 60;     // gliding down from the sky
    constexpr int crow_notice = 4;              // tiles
    constexpr int crow_notice_crouched = 2;
    constexpr int crow_noise = 9;               // tiles: the caw
    constexpr int crow_cooldown = 1800;         // frames after crows leave before any land again
    constexpr int crow_idle_frames = 40;        // between a crow's pecks and hops (plus up to as many again)
    constexpr int crow_hop_frames = 6;
    constexpr int crow_hop_milli = 600;

    // Calls (animals plan stage 6, tools/assets/animal_sounds.py: the user picked all the A's). The files peak at
    // -3 dB; each plays at its final volume (grunt -8 dB, howl -10, snarl -6, cluck -12, bleat -10, caw -8), that is
    // at 10^((final + 3) / 20). A call is heard from within its reach of the screen's middle (scaled by the
    // survivor's hearing), at half volume from off screen. Times in frames: the nearest of each kind calls now and
    // then besides the calls things set off (a boar noticing you or winding up, a pack coming out, a wolf darting
    // in, a hen or goat running off, crows taking off).
    inline constexpr bn::fixed grunt_volume = bn::fixed(0.562);
    inline constexpr bn::fixed howl_volume = bn::fixed(0.447);
    inline constexpr bn::fixed snarl_volume = bn::fixed(0.708);
    inline constexpr bn::fixed cluck_volume = bn::fixed(0.355);
    inline constexpr bn::fixed bleat_volume = bn::fixed(0.447);
    inline constexpr bn::fixed caw_volume = bn::fixed(0.562);
    constexpr int call_reach_px = 200;
    constexpr int howl_reach_px = 640;          // a howl carries
    constexpr int screen_half_x = 128;          // "on screen", for the volume
    constexpr int screen_half_y = 88;
    constexpr int grunt_frames = 10 * 60;       // every 10-20 s
    constexpr int howl_frames = 25 * 60;        // every 25-45 s, from a pack roaming at night
    constexpr int cluck_frames = 5 * 60;        // every 5-11 s
    constexpr int bleat_frames = 8 * 60;        // every 8-16 s
    constexpr int caw_frames = 6 * 60;          // every 6-12 s, from crows on the ground
}

enum class animal_kind : uint8_t
{
    DEER,
    RABBIT,
    BOAR,
    WOLF,
    HEN,
    GOAT
};

class animal_manager
{

public:
    static constexpr int save_bytes = 1 + (animal_tuning::max_kills * 4) + 2 + (animal_tuning::max_tallies * 8);

    explicit animal_manager(const bn::camera_ptr& camera);

    // A new world, a load or a death: no animals, no flock; reset_kills also forgets the hunted cells.
    void clear();
    void reset_kills();

    // Once a frame while playing (skipped during hitstop, like the zombies): spawning, senses, movement.
    void update(game& owner);

    // Every frame: sprite positions, frames and depth.
    void update_visuals(int camera_left, int camera_top);

    // A noise (game::noise): loud ones scare deer, rabbits and birds within their radius.
    void hear(game& owner, const bn::fixed_point& position, int radius_tiles);

    // A shot along a line (after it missed every zombie): the first animal it passes hit; true if one was.
    bool shoot(game& owner, const bn::fixed_point& origin, const bn::fixed_point& direction, int range, gun kind);

    // A melee swing: the nearest animal in reach and arc hit; true if one was.
    bool strike(game& owner, const bn::fixed_point& origin, const bn::fixed_point& direction, int reach,
                int arc_degrees, weapon used);

    // Soft auto-aim when no zombie is in front: the nearest live animal within range and arc; false if none.
    [[nodiscard]] bool aim_target(const bn::fixed_point& origin, const bn::fixed_point& direction, int range,
                                  int arc_degrees, bn::fixed_point& feet) const;

    // A deer carcass within reach of a point: its id, or -1.
    [[nodiscard]] int carcass_at(const bn::fixed_point& point, int reach) const;
    [[nodiscard]] bool carcass_exists(int id) const;
    [[nodiscard]] animal_kind kind_of_id(int id) const;
    [[nodiscard]] bn::fixed_point position_of(int id) const;
    void remove(int id);

    // Farm animals (stage 4): a live hen within reach of a point (to catch), a live goat not milked today (to milk);
    // their ids or -1. Catching gives raw meat; milking turns an empty bottle into milk. Both true if done.
    [[nodiscard]] int hen_at(const bn::fixed_point& point, int reach) const;
    [[nodiscard]] int goat_to_milk(game& owner, const bn::fixed_point& point, int reach) const;
    bool catch_hen(game& owner, int id);
    bool milk_goat(game& owner, int id);

    // Eggs foraging finds at a tile: 1 or 2 near a yard whose farm has hens left, else 0.
    [[nodiscard]] int eggs_near(int tile_x, int tile_y) const;

    // Debug and benches: an animal or a flock placed now (on the test world nothing spawns by itself).
    int spawn(game& owner, animal_kind kind, const bn::fixed_point& feet);
    void spawn_flock(game& owner, const bn::fixed_point& centre);

    // Crows coming down to a corpse (its tile's middle); 0 if there was no room.
    int spawn_crows(game& owner, const bn::fixed_point& corpse);

    [[nodiscard]] bool crows() const
    {
        return _crows && ! _birds.empty();
    }

    [[nodiscard]] bool crows_landing() const
    {
        return _landing > 0;
    }

    // Benches: the corpse crows would come to now (what the spawn tries look for); false if none is in range.
    [[nodiscard]] bool carrion_for(game& owner, bn::fixed_point& corpse) const
    {
        return _carrion_spot(owner, corpse);
    }
    int spawn_pack(game& owner, const bn::fixed_point& centre, int count);

    // Benches: a farm (as if its macro cell had a yard at the tile), its hens and goats brought out at once.
    void bench_farm(game& owner, int cell_x, int cell_y, int yard_x, int yard_y, int hens, int goats);

    [[nodiscard]] int farm_lost(int cell_x, int cell_y, bool goats) const;

    // Benches: the farms round the survivor bring their animals out now (as the spawn tries do every 90 frames).
    void spawn_farms_now(game& owner)
    {
        _spawn_farm(owner);

        while(_pending_farm >= 0 && _bring_out_next(owner))
        {
        }
    }

    [[nodiscard]] bool pack_hunting() const
    {
        return _pack_hunting;
    }

    [[nodiscard]] bool kept_off() const
    {
        return _kept_off;
    }

    // Benches: what is about.
    [[nodiscard]] int count() const
    {
        return _animals.size();
    }

    [[nodiscard]] int birds() const
    {
        return _birds.size();
    }

    [[nodiscard]] bool flock_flying() const
    {
        return _flying;
    }

    [[nodiscard]] int health_of(int index) const
    {
        return _animals[index].health;
    }

    [[nodiscard]] int state_of(int index) const
    {
        return int(_animals[index].current);
    }

    [[nodiscard]] const bn::fixed_point& position(int index) const
    {
        return _animals[index].position;
    }

    [[nodiscard]] animal_kind kind_of(int index) const
    {
        return _animals[index].kind;
    }

    [[nodiscard]] int kills_listed() const;

    #if KX_BENCH == 15
        // Playtest 3: every wolf's state, position, speed, picture and side this frame (KXWTRACE lines, read by
        // tools/wolf_trace.py).
        void trace_wolves(int frame) const;
    #endif

    void save(save::writer& out) const;
    void load(save::reader& in);

private:
    enum class state : uint8_t
    {
        GRAZE,          // still, head down
        LOOK,           // still, head up
        WALK,
        FLEE,
        DEAD,           // a deer or boar carcass
        WINDUP,         // a boar about to charge: still, facing you
        CHARGE,         // a boar running at where you stood
        CIRCLE,         // a wolf pacing round the survivor
        DART,           // a wolf running in to bite
        RETREAT         // a wolf backing off after a dart
    };

    struct animal
    {
        explicit animal(bn::sprite_ptr&& sprite_ptr) :
            sprite(bn::move(sprite_ptr))
        {
        }

        bn::sprite_ptr sprite;
        bn::fixed_point position;
        bn::fixed_point velocity;
        bn::fixed_point moved;      // how far it really goes a frame (smoothed; sliding along a wall, less): its picture
        animal_kind kind = animal_kind::DEER;
        state current = state::GRAZE;
        int timer = 0;
        int health = 1;
        int anim = 0;
        int frame = -1;
        int flash = 0;
        int calm = 0;               // a boar: frames before it may charge again
        int orbit = 0;              // a wolf: where on the ring it paces (1/4 of a heading step)
        int detour = 0;             // frames left going round something in the way
        bn::fixed_point home;       // a farm animal: its barn door
        int8_t farm = -1;           // a farm animal: its farm in _farms
        uint8_t slot = 0;           // a goat: which of its farm's goats (for the milking day)
        bool clockwise = false;
        int sprite_x = -32768;
        int sprite_y = -32768;
        int sprite_z = 1000;
        uint8_t id = 0;
        bool left = false;
        bool mirrored = false;
    };

    struct bird
    {
        explicit bird(bn::sprite_ptr&& sprite_ptr) :
            sprite(bn::move(sprite_ptr))
        {
        }

        bn::sprite_ptr sprite;
        bn::fixed_point position;
        bn::fixed_point velocity;
        int frame = -1;
        int timer = 0;              // a crow: frames to its next peck or hop
        int hop = 0;                // a crow: frames left of a hop
        bool pecking = false;
        bool left = false;
        bool mirrored = false;
        int sprite_x = -32768;
        int sprite_y = -32768;
        int sprite_z = 1000;
    };

    struct kill
    {
        int8_t cell_x = -1;
        int8_t cell_y = -1;
        int16_t day = -1;
    };

    // A farm near the survivor: its yard and stock from the seed.
    struct known_farm
    {
        int8_t cell_x = -1;
        int8_t cell_y = -1;
        int16_t yard_x = 0;         // tiles
        int16_t yard_y = 0;
        uint8_t hens = 0;
        uint8_t goats = 0;
    };

    // What is saved of a farm: hens and goats lost, and the day each goat was last milked.
    struct tally
    {
        int8_t cell_x = -1;
        int8_t cell_y = -1;
        uint8_t hens_lost = 0;
        uint8_t goats_lost = 0;
        int16_t milked[2] = { -1, -1 };
    };

    bn::camera_ptr _camera;
    bn::vector<animal, animal_tuning::max_animals> _animals;
    bn::vector<bird, animal_tuning::max_birds> _birds;
    kill _kills[animal_tuning::max_kills];
    int _next_kill = 0;
    known_farm _farms[animal_tuning::max_known_farms];
    int _next_farm = 0;
    int _pending_farm = -1;         // a farm whose animals are coming out, one a frame
    static constexpr int max_no_yard = 8;
    uint16_t _no_yard[max_no_yard] = { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF };
    int _next_no_yard = 0;          // farm cells found to have no yard (cell y << 8 | x)
    tally _tallies[animal_tuning::max_tallies];
    int _next_tally = 0;
    int _frame = 0;
    int _bird_cooldown = 0;
    int _fly_timer = 0;
    uint8_t _next_id = 1;
    int _dart_cooldown = 0;         // the pack: frames to the next dart
    int _pack_fear = 0;             // the pack: frames it keeps running after a wolf is killed
    bool _pack_hunting = false;
    bool _kept_off = false;         // a fire burns near the survivor
    int _grunt_timer = animal_tuning::grunt_frames;
    int _howl_timer = animal_tuning::howl_frames;
    int _cluck_timer = animal_tuning::cluck_frames;
    int _bleat_timer = animal_tuning::bleat_frames;
    int _caw_timer = animal_tuning::caw_frames;
    bool _flying = false;
    bool _crows = false;            // the flock is crows (stage 5)
    int _landing = 0;               // crows: frames left gliding down
    bn::fixed_point _carrion;       // crows: the corpse they came for
    bool _shaded_dark = false;
    mutable bn::random _random;     // their own, so the zombies' rolls stay as they were

    void _spawn_tries(game& owner);
    [[nodiscard]] bool _deer_cell(game& owner, int tile_x, int tile_y) const;
    [[nodiscard]] bool _boar_cell(game& owner, int tile_x, int tile_y) const;
    [[nodiscard]] bool _wolf_cell(game& owner, int tile_x, int tile_y) const;
    void _spawn_farm(game& owner);
    bool _bring_out(game& owner, int farm, int hens_out, int goats_out, int most);
    bool _bring_out_next(game& owner);
    [[nodiscard]] int _known_farm(int cell_x, int cell_y) const;
    [[nodiscard]] tally* _tally(int cell_x, int cell_y, bool create);
    void _farm_loss(const animal& value);
    void _call(game& owner, const bn::fixed_point& at, sfx::id sound, bn::fixed volume, int reach_px) const;
    void _update_calls(game& owner);
    [[nodiscard]] bool _ring_spot(game& owner, int near, int far, bool deer, int& tile_x, int& tile_y) const;
    void _think(game& owner, animal& value);
    void _flee(animal& value, const bn::fixed_point& from);
    void _wind_up(animal& value, const bn::fixed_point& target, int frames);
    void _update_charge(game& owner, animal& value);
    void _update_pack(game& owner);
    void _think_wolf(game& owner, animal& value);
    void _retreat(animal& value, const bn::fixed_point& from);
    void _scare_pack(const bn::fixed_point& from);
    void _move(animal& value);
    void _hit(game& owner, animal& value, int damage, const bn::fixed_point& from);
    void _scatter(game& owner, const bn::fixed_point& from);
    void _update_flock(game& owner);
    void _update_crows(game& owner);
    [[nodiscard]] bool _carrion_spot(game& owner, bn::fixed_point& corpse) const;
    [[nodiscard]] int _index_of(int id) const;
};

}

#endif
