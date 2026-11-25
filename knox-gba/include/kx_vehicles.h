#ifndef KX_VEHICLES_H
#define KX_VEHICLES_H

#include "bn_camera_ptr.h"
#include "bn_fixed_point.h"
#include "bn_optional.h"
#include "bn_sprite_ptr.h"

#include "kx_items.h"

namespace kx
{

class game;
struct input_frame;

namespace save
{
    class reader;
    class writer;
}

// Vehicles (spec: Vehicles). Cars are everywhere but about 90% are wrecks; a working one is a mid-game project. A
// bicycle comes first.
namespace vehicle_tuning
{
    // Bookkeeping.
    constexpr int max_tracked = 24;             // spec: up to 24 tracked cars (moved, repaired or stripped), 16 B each
    constexpr int reserved_slots = 4;           // wrecks stop being stripped while only these are left, so a car or
                                                // bicycle can always still be moved
    constexpr int max_active = 8;                // vehicles simulated around the player (each car is an obstacle)

    // Placement from the seed: one spot per block of 16x16 tiles, tried a few times, used this often by what the
    // spot is. Pavements only exist in settlements, so parked cars gather there.
    constexpr int block_shift = 4;              // tiles
    constexpr int block_offset = 8;             // blocks start half a block in: roads run along multiples of 16
                                                // tiles, and so through the middle of these blocks
    constexpr int placement_tries = 4;
    constexpr int road_percent = 26;
    constexpr int pavement_percent = 80;
    constexpr int bicycle_percent = 40;         // a concrete floor under a roof (garages, barns, petrol stations)
    constexpr int wreck_percent = 90;           // spec: about 90% are wrecks
    constexpr int crashed_percent = 40;         // wrecks on the road left at an odd angle
    constexpr int piece_clearance = 7;          // tiles kept clear round the middle of a set piece (milestone 14)
    constexpr int moss_percent = 30;            // cars the seed put out with moss grown on them (wrecks too)
    constexpr int alarm_percent = 20;           // spec: opening one can set off its alarm
    constexpr int key_blocks = 3;               // a key starts the cars parked within this many blocks of its find

    // Bicycle (spec: 2.5 px/frame, quiet, no fuel, fast on roads and paths, poor off-road; you fall off if you hit
    // a zombie).
    constexpr int bike_speed_milli = 2500;
    constexpr int bike_rough_milli = 1200;      // off the roads, paths and floors
    constexpr int bike_water_milli = 600;
    constexpr int bike_accel_frames = 28;
    constexpr int bike_stop_frames = 36;
    constexpr int bike_noise = 1;
    constexpr int bike_crash_px = 9;            // a standing zombie this close...
    constexpr int bike_crash_milli = 1200;      // ...at this speed or more throws you off
    constexpr int bike_exertion = 1;            // per frame while pedalling
    constexpr int bike_reach_px = 14;

    // Car (spec: heading in 32 steps; velocity along the heading; a lateral-friction factor gives grip or drift;
    // tile collisions bounce and damage; top speed 4.5 px/frame on road, about half off-road).
    constexpr int headings = 32;
    constexpr int top_milli = 4500;
    constexpr int offroad_percent = 50;
    constexpr int mud_percent = 33;             // forest floor, mud and crops
    constexpr int water_percent = 22;           // shallow water
    constexpr int accel_milli = 45;             // per frame
    constexpr int brake_milli = 130;
    constexpr int slow_milli = 80;              // back down to the ground's top speed
    constexpr int reverse_milli = 1500;
    constexpr int coast_256 = 252;              // of the speed kept each frame without throttle
    constexpr int grip_road_256 = 190;          // of the sideways speed kept each frame: lower grips harder
    constexpr int grip_rough_256 = 228;
    constexpr int turn_256 = 62;                // heading steps per frame at full lock (a full circle in ~2 s)
    constexpr int turn_full_milli = 1500;       // speed at which the steering is at full effect
    constexpr int half_length = 11;             // collision box of a car, before turning
    constexpr int half_width = 6;
    constexpr int car_reach_px = 8;             // A reaches a car this far from its box

    constexpr int bounce_256 = 77;              // of the speed kept, reversed, on hitting something
    constexpr int crash_milli = 800;            // impacts faster than this hurt the car...
    constexpr int crash_damage_per_px = 8;      // ...by this much of its 100 condition per px/frame over it
    constexpr int crash_noise = 14;
    constexpr int injury_milli = 3000;          // spec: crashes can cause fractures or deep wounds
    constexpr int injury_percent = 35;
    constexpr int fracture_percent = 30;        // of those; the rest are deep wounds
    constexpr int worn_condition = 30;          // below this the car is slow
    constexpr int worn_percent = 60;

    // Zombies (spec: a thump with hitstop and shake; it damages the car and slows it; a crowd can stall or wreck
    // it).
    constexpr int ram_milli = 800;              // slower than this just pushes
    constexpr int ram_kill_milli = 2500;
    constexpr int ram_reach_px = 17;
    constexpr int ram_frames = 12;              // a zombie is hit at most once in this long
    constexpr int ram_damage = 2;               // to the car, per zombie
    constexpr int ram_slow_256 = 215;           // of the speed kept per zombie
    constexpr int stall_hits = 6;               // this many in quick succession stall the engine
    constexpr int stall_decay_frames = 20;

    // Zombies attacking the car the player sits in (milestone 12.5): each hit takes this much of its 100
    // condition and is heard this far; below pull_out_condition they can drag the driver out.
    constexpr int besiege_damage = 2;
    constexpr int besiege_noise_tiles = 6;
    constexpr int pull_out_condition = 25;
    constexpr int besiege_reach_px = 10;        // past the grab range: the car's side is between them
    constexpr int stall_frames = 90;

    // Engine, fuel and battery.
    constexpr int engine_noise = 15;            // spec: engine noise radius 15
    constexpr int engine_noise_frames = 30;
    constexpr int engine_sound_frames = 13;
    constexpr int tank_dl = 400;                // decilitres: a 40 l tank
    constexpr int fuel_unit = 1500;             // burn units per decilitre: about 1 l per 12 s at full speed
    constexpr int fuel_idle = 2;                // units per frame at a standstill
    constexpr int fuel_per_px = 4;              // more per px/frame of speed
    constexpr int start_charge = 2;             // battery % per start
    constexpr int charge_frames = 600;          // +1% while the engine runs
    constexpr int lights_drain_frames = 300;    // spec: headlights drain the battery when the engine is off
    constexpr int loot_charge = 80;             // a found battery

    // Alarms (spec: noise radius 60).
    constexpr int alarm_noise = 60;
    constexpr int alarm_frames = 20 * 60;
    constexpr int alarm_sound_frames = 100;

    // Work on a car (in-game minutes; 10% faster per Mechanics level) and what it teaches.
    constexpr int part_minutes = 20;
    constexpr int fuel_minutes = 5;
    constexpr int siphon_minutes = 10;
    constexpr int hotwire_minutes = 30;
    constexpr int faster_percent_per_level = 10;
    constexpr int part_xp = 12;
    constexpr int hotwire_xp = 25;
    constexpr int hotwire_level = 2;            // spec: Mechanics 2, or the Mechanic or Burglar occupation
    constexpr int hotwire_percent = 45;         // chance of success...
    constexpr int hotwire_percent_per_level = 10;
    constexpr int hotwire_job_percent = 25;     // ...more for the two occupations
    constexpr int hotwire_noise = 8;
    constexpr int needed_tyres = 3;             // spec: at least 3 good tyres
    constexpr int trunk_capacity = 6000;        // spec: the trunk is mobile storage (60 kg), in 10 g
}

enum class vehicle_kind : uint8_t
{
    BICYCLE,
    CAR,
    WRECK
};

// The checklist (spec: a working car needs 5 parts, shown as a tick or a cross).
enum class car_part : uint8_t
{
    BATTERY,
    SPARK_PLUG,
    RADIATOR,
    TYRES,
    FUEL,
    COUNT
};

enum class car_action : uint8_t
{
    TRUNK,
    GLOVE_BOX,
    SIPHON,
    HOTWIRE,
    REPAIR,         // milestone 14: a radiator or brake pads put condition back
    DRIVE,
    COUNT
};

class vehicle_manager
{

public:
    explicit vehicle_manager(const bn::camera_ptr& camera);

    // A new world: nothing tracked (the test map gets a bicycle, a wreck and a car that runs).
    void reset();

    // Simulation step (skipped during hitstop): vehicles around the player come and go, the car being driven
    // moves, engines and alarms are heard.
    void update(game& owner, const input_frame& input);

    // Visual step (every frame).
    void update_visuals(game& owner, int camera_left, int camera_top);

    // ----- on foot -----

    // The vehicle A would use from here (feet and the point in front of them), or -1.
    [[nodiscard]] int reachable(const bn::fixed_point& feet, const bn::fixed_point& probe) const;

    [[nodiscard]] vehicle_kind kind(int index) const;
    [[nodiscard]] bn::fixed_point position(int index) const;

    // "BLUE SEDAN", "WRECK", "BICYCLE".
    [[nodiscard]] const char* name(int index) const;

    // Gets on a bicycle. False (with a message) if it can't be kept track of.
    bool mount(game& owner, int index);

    // ----- the car screen (mockup 10) -----

    [[nodiscard]] bool part_ok(int index, car_part part) const;

    // Any of this part is there to take (a tyre, some fuel): what a wreck gives (playtest 6).
    [[nodiscard]] bool part_some(int index, car_part part) const
    {
        return part == car_part::TYRES ? _active[index].data.tyres > 0 : part_ok(index, part);
    }

    // Nothing more of this part can go in (fitted, full, all four tyres).
    [[nodiscard]] bool part_full(int index, car_part part) const;

    // What the checklist shows beside a part ("MISSING", "3/4", "12 L").
    void part_note(int index, car_part part, char* output) const;

    // Whether A (fit, fill, add fuel) or R1 (take out) would work on a part now, else why not.
    [[nodiscard]] bool can_fit(game& owner, int index, car_part part, const char*& problem) const;
    [[nodiscard]] bool can_remove(game& owner, int index, car_part part, const char*& problem) const;
    [[nodiscard]] bool can_do(game& owner, int index, car_action action, const char*& problem) const;

    // Starts the work: what it uses goes, time passes (game::begin_car_work), then finish_work() applies it.
    void fit(game& owner, int index, car_part part);
    void remove(game& owner, int index, car_part part);
    void act(game& owner, int index, car_action action);

    // Applies the finished work; returns the vehicle it was on (to show its screen again), or -1.
    int finish_work(game& owner);

    // Minutes a job takes at the survivor's Mechanics level.
    [[nodiscard]] int minutes(game& owner, int base) const;

    // A part's work before Mechanics (a car jack speeds tyres; milestone 14).
    [[nodiscard]] int part_work_minutes(game& owner, car_part part) const;

    // The trunk or glove box as a container (registered on first use), or -1.
    [[nodiscard]] int container_of(game& owner, int index, bool trunk);

    // Opening a car can set off its alarm (once).
    void opened(game& owner, int index);

    // A key carried that starts this car, or the car is hotwired.
    [[nodiscard]] bool has_key(game& owner, int index) const;
    [[nodiscard]] bool hotwired(int index) const;

    [[nodiscard]] int condition(int index) const;
    [[nodiscard]] int tracked_count() const;

    // ----- riding and driving -----

    // The car being driven, or -1.
    [[nodiscard]] int driven() const
    {
        return _driven;
    }

    [[nodiscard]] bool riding_bicycle() const
    {
        return _ridden >= 0;
    }

    // Gets into a car that is ready and starts it.
    void enter(game& owner, int index);

    // Leaves the car or bicycle where it is (getting out, falling off, dying, a teleport).
    bool leave(game& owner, bool place_player);

    // Brakes to a halt at once (the driver passed out).
    void halt();

    // Headlights on: light to read by.
    [[nodiscard]] bool lit() const;

    // The beam of a car with its headlights on (the one being driven first): the middle of its front, world
    // pixels, and its heading in degrees counterclockwise from east. False when no car near has its lights on.
    [[nodiscard]] bool headlights(bn::fixed_point& front, int& angle) const;

    // The car being driven has its headlights on.
    [[nodiscard]] bool driven_lit() const;

    // A zombie hits the car being driven (it can't reach the driver): its condition drops; returns what is left.
    int hit_driven(int damage);

    // Debug: switches a vehicle's headlights (an index spawn returned).
    void set_lights(int index, bool on);

    // Two short lines for the HUD while driving ("FUEL 12 L", "CAR 85%"); false when not driving.
    [[nodiscard]] bool status(char* line1, char* line2) const;

    // Forgets the vehicles around the player (a teleport), so they are looked up again.
    void rescan();

    // Debug and benches: a car that runs (or a bicycle) beside the player.
    int spawn(game& owner, vehicle_kind which);

    // Vehicles around the player now; a checksum of the tracked ones (save bench).
    [[nodiscard]] int active_count() const;

    // Vehicles in play with moss grown on them (the seed's; for the save bench's log).
    [[nodiscard]] int mossy_count() const;
    [[nodiscard]] int tracked_checksum() const;

    // Bench: the speed of the car being driven, in thousandths of a pixel per frame; the first vehicle of a kind in
    // play, or -1; and a vehicle's numbers.
    [[nodiscard]] int speed_milli() const;
    [[nodiscard]] int first(vehicle_kind which) const;
    void numbers(int index, int& fuel_dl, int& heading, int& tyres, int& charge) const;

    void save(save::writer& out) const;
    void load(save::reader& in);

    static constexpr int save_bytes = (vehicle_tuning::max_tracked * 16) + 8;

private:
    // A vehicle as tracked and saved: 16 bytes.
    struct record
    {
        uint16_t x = 0;             // centre, world pixels
        uint16_t y = 0;
        uint16_t origin = 0;        // the block the seed put it in (its copy there is not shown again); no_origin
                                    // for spawned ones
        uint16_t fuel = 0;          // decilitres
        uint8_t heading = 0;        // 0..31, 0 east, clockwise
        uint8_t bits = 0;
        uint8_t parts = 0;
        uint8_t tyres = 0;          // good ones
        uint8_t condition = 0;      // 0..100
        uint8_t charge = 0;         // battery, 0..100
        uint8_t spare[2] = {};
    };

    static_assert(sizeof(record) == 16);

    struct active
    {
        record data;
        int8_t slot = -1;           // in the tracked list, or -1 while it is still as the seed made it
        bool live = false;
        int16_t frame = -1;
        bn::optional<bn::sprite_ptr> sprite;
    };

    bn::camera_ptr _camera;
    record _tracked[vehicle_tuning::max_tracked];
    active _active[vehicle_tuning::max_active];

    // Which blocks around the player have been looked at.
    int _scan_block_x = -1000;
    int _scan_block_y = -1000;
    int _scan_step = 0;
    bool _scan_again = false;       // a block's chunks were not generated yet
    int _frame = 0;
    bool _obstacles_dirty = true;

    // The car being driven.
    int _driven = -1;
    bn::fixed_point _position;
    bn::fixed_point _velocity;
    bn::fixed _heading;
    bool _engine = false;
    int _fuel_burn = 0;
    int _stall_timer = 0;
    int _recent_hits = 0;
    int _ram_id = 1 << 20;
    int _ram_timer = 0;
    int _crash_cooldown = 0;

    // The bicycle being ridden.
    int _ridden = -1;

    // Load: the tracked vehicle the survivor was in or on (entered again once it is active).
    int _resume_slot = -1;

    // An alarm going off.
    int _alarm_frames = 0;
    bn::fixed_point _alarm_position;
    int _alarm_slot = -1;

    // Work in progress on a car (applied by finish_work).
    enum class job : uint8_t
    {
        NONE,
        FIT,
        REMOVE,
        SIPHON,
        HOTWIRE,
        REPAIR
    };

    job _job = job::NONE;
    car_part _job_part = car_part::BATTERY;
    int _job_vehicle = -1;
    int _job_amount = 0;

    [[nodiscard]] bool _seeded(int block_x, int block_y, record& output, bool& ready) const;
    [[nodiscard]] int _find_active(uint16_t origin, int slot) const;
    int _activate(const record& data, int slot);
    void _scan(game& owner);
    bool _track(game& owner, active& value, bool quiet = false);
    void _commit(active& value);
    void _rebuild_obstacles(game& owner);
    void _drive(game& owner, const input_frame& input);
    void _start_engine(game& owner, active& car);
    void _crash(game& owner, active& car, bn::fixed impact);
    [[nodiscard]] bool _car_blocked(const bn::fixed_point& centre, bn::fixed heading) const;
    [[nodiscard]] bool _ready(const active& car) const;
    [[nodiscard]] uint16_t _key_id(const active& car) const;
    [[nodiscard]] int _key_slot(game& owner, const active& car) const;
    void _stop_alarm();
};

}

#endif
