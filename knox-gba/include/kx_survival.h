#ifndef KX_SURVIVAL_H
#define KX_SURVIVAL_H

#include "bn_array.h"
#include "bn_random.h"

#include "kx_items.h"
#include "kx_tuning.h"

namespace kx
{

class inventory;

// Daylight hours (day and night air temperatures); outside them the clock says NIGHT (mockup 02).
constexpr int day_start_hour = 6;
constexpr int night_start_hour = 20;

[[nodiscard]] constexpr bool is_night(int minute_of_day)
{
    int hour = minute_of_day / 60;
    return hour < day_start_hour || hour >= night_start_hour;
}

// ---------------------------------------------------------------------------------------------------------------------
// Tuning (spec: Health and survival). Needs are integer counters updated once per in-game minute; rates are per
// minute unless noted. Percent counters run 0..100000 (100000 = 100%).
// ---------------------------------------------------------------------------------------------------------------------

enum class survival_param : int
{
    FRAMES_PER_MINUTE,
    FOOD_DRAIN,
    WATER_DRAIN,
    FATIGUE_GAIN,
    SLEEP_RECOVERY,
    ENDURANCE_PER_POINT,
    ENDURANCE_RECOVERY,
    SPRINT_EXERTION,
    JOG_EXERTION,
    WADING_WETNESS,
    DRYING,
    DAY_TEMP,
    NIGHT_TEMP,
    BLOOD_REGEN,
    PANIC_PER_ZOMBIE,
    PANIC_DECAY,
    SCRATCH_INFECTION,
    LACERATION_INFECTION,
    WOUND_INFECTION,
    BANDAGE_DIRTY_HOURS,
    INFECTION_MIN_HOURS,
    INFECTION_MAX_HOURS,
    FRACTURE_CHANCE,
    PASS_OUT_HOURS,
    SWIM_EXERTION,
    COUNT
};

constexpr tuning_entry survival_tuning_defaults[] = {
    tune_int("Frames per minute", 150, 1, 600, 5),        // 150 = PZ's default 1-hour day
    tune_int("Food drain", 25, 0, 500, 5),                 // 100% lasts ~2.8 days
    tune_int("Water drain", 45, 0, 500, 5),                // ~1.5 days
    tune_int("Fatigue gain", 70, 0, 500, 5),               // ~24 h awake to 100%
    tune_int("Sleep recovery", 300, 0, 2000, 10),
    tune_int("Endurance per point", 10, 0, 100),           // exertion points -> endurance
    tune_int("Endurance recovery", 3000, 0, 20000, 250),
    tune_int("Sprint exertion/frame", 3, 0, 20),
    tune_int("Jog exertion/frame", 1, 0, 20),
    tune_int("Wading wetness", 10, 0, 100),                // shallow water, per minute
    tune_int("Drying", 1, 0, 20),
    tune_int("Day temp C", 25, -20, 45),                   // July (spec: the world starts on 1 July)
    tune_int("Night temp C", 16, -20, 45),
    tune_int("Blood regen ml", 5, 0, 100),
    tune_int("Panic per zombie", 4, 0, 50),
    tune_int("Panic decay", 3, 0, 50),
    tune_int("Scratch infection %", 7, 0, 100),            // spec: ~7%
    tune_int("Laceration infect. %", 25, 0, 100),          // spec: ~25%
    tune_int("Wound infection %/h", 3, 0, 100),            // regular infection of open wounds
    tune_int("Bandage dirty hours", 12, 1, 72),
    tune_int("Infection min hours", 48, 1, 240),           // spec: 2-3 in-game days to death
    tune_int("Infection max hours", 72, 1, 240),
    tune_int("Fall fracture %", 5, 0, 100),
    tune_int("Pass out hours", 4, 1, 24),
    tune_int("Swim exertion/frame", 3, 0, 20),             // milestone 13: like sprinting, moving or not
};

static_assert(sizeof(survival_tuning_defaults) / sizeof(tuning_entry) == int(survival_param::COUNT));

[[nodiscard]] tuning_set& survival_tuning();

// Tired (milestone 13): searching, crafting, building and reading take longer at Tired 2 and 3; Tired 3 warns once;
// at Tired 4 the screen dims and the survivor falls asleep where they stand.
// The Smoker (milestone 13): 8 in-game hours after the last cigarette the craving starts: unhappiness climbs a
// point every 10 minutes and panic is held up from 20, rising 5 an hour to 45. A cigarette (3 s, a lighter or
// matches) lifts the mood.
namespace smoker_tuning
{
    constexpr int craving_minutes = 8 * 60;
    constexpr int unhappy_every = 10;
    constexpr int panic_floor = 20;
    constexpr int panic_per_hour = 5;
    constexpr int panic_floor_max = 45;
    constexpr int cheer = 20;
    constexpr int calm = 10;
    constexpr int smoke_frames = 180;
}

namespace tired_tuning
{
    constexpr int action_percent[5] = { 100, 100, 125, 150, 150 };
    constexpr int warn_level = 3;
    constexpr int forced_sleep_level = 4;
    constexpr int forced_dim_frames = 180;
}

[[nodiscard]] inline int tuning(survival_param param)
{
    return survival_tuning().get(int(param)).integer();
}

// Frames per in-game minute: the tuning table's (an hour-long day) scaled by the sandbox's day length.
[[nodiscard]] int frames_per_minute();

// ---------------------------------------------------------------------------------------------------------------------
// Moodles, body parts and wounds.
// ---------------------------------------------------------------------------------------------------------------------

enum class moodle : uint8_t
{
    HUNGRY,
    THIRSTY,
    TIRED,
    EXHAUSTED,
    WET,
    COLD,
    HOT,
    PANIC,
    UNHAPPY,
    HEAVY,
    PAIN,
    BLEEDING,
    SICK,
    FEVER,
    COUNT
};

[[nodiscard]] const char* moodle_name(moodle value);

enum class body_part : uint8_t
{
    HEAD,
    TORSO,
    LEFT_ARM,
    RIGHT_ARM,
    LEFT_LEG,
    RIGHT_LEG,
    COUNT
};

[[nodiscard]] const char* body_part_name(body_part value);

enum class wound_type : uint8_t
{
    NONE,
    SCRATCH,
    LACERATION,
    DEEP_WOUND,
    FRACTURE,
    BURN,
    BITE
};

[[nodiscard]] const char* wound_name(wound_type value);

enum class bandage_state : uint8_t
{
    NONE,
    CLEAN,
    DIRTY
};

struct body_wound
{
    wound_type type = wound_type::NONE;
    bandage_state bandage = bandage_state::NONE;
    bool disinfected = false;
    bool splinted = false;
    bool stitched = false;          // a deep wound sewn shut: no bleeding, heals twice as fast (milestone 12.5)
    bool infected = false;          // regular wound infection (antibiotics or disinfectant cure it)
    uint16_t hours_left = 0;        // until healed
    uint16_t bandage_minutes = 0;   // since the bandage was applied
};

enum class death_cause : uint8_t
{
    NONE,
    BITTEN,
    BLED_OUT,
    STARVED,
    THIRST,
    COLD,
    HEAT,
    INFECTION,
    SICKNESS,
    DROWNED         // milestone 13
};

[[nodiscard]] const char* death_text(death_cause value);

// What the world reports to the minute tick.
struct survival_input
{
    int exertion = 0;               // points accumulated over the minute (sprinting, jogging, swings)
    int zombies_in_view = 0;
    int insulation = 0;             // worn clothing
    int load_percent = 0;           // carried weight vs capacity
    int minute_of_day = 0;
    bool wading = false;
    bool sleeping = false;

    // Character (percent of normal): hunger, wound healing, panic, endurance cost, bandage life.
    int hunger_percent = 100;
    int heal_percent = 100;
    int panic_percent = 100;
    int endurance_percent = 100;
    int bandage_percent = 100;
    int drying_percent = 100;       // by a campfire: clothes dry faster
    int rain_wetness = 0;           // per minute outdoors in rain
    int season_temp = 0;            // degrees the season takes off the air temperature
    int weather_percent = 100;      // how much rain and cold affect the survivor (Outdoorsman, Homebody)
    bool smoker = false;            // milestone 13: the Smoker trait
};

// ---------------------------------------------------------------------------------------------------------------------
// The survivor's body: needs, vitals, wounds and the infections (spec: Health and survival).
// ---------------------------------------------------------------------------------------------------------------------
class survival
{

public:
    static constexpr int full = 100000;

    survival();

    void tick_minute(const survival_input& input, bn::random& random);

    [[nodiscard]] int level(moodle value) const
    {
        return _levels[int(value)];
    }

    // Vitals for the Health tab.
    [[nodiscard]] int food_percent() const
    {
        return _food / 1000;
    }

    [[nodiscard]] int water_percent() const
    {
        return _water / 1000;
    }

    [[nodiscard]] int fatigue_percent() const
    {
        return _fatigue / 1000;
    }

    [[nodiscard]] int endurance_percent() const
    {
        return _endurance / 1000;
    }

    [[nodiscard]] int blood() const
    {
        return _blood;
    }

    [[nodiscard]] int body_temperature() const      // tenths of a degree
    {
        return _temperature;
    }

    [[nodiscard]] int health() const
    {
        return _health;
    }

    [[nodiscard]] const body_wound& wound(body_part part) const
    {
        return _wounds[int(part)];
    }

    [[nodiscard]] bool zombie_infected() const
    {
        return _infection_minutes >= 0;
    }

    [[nodiscard]] death_cause cause_of_death() const
    {
        return _death;
    }

    [[nodiscard]] bool can_sprint() const
    {
        return level(moodle::EXHAUSTED) < 3;
    }

    // Wind-up frames added to swings when exhausted or exhausted-tired (spec: weaker, slower attacks).
    [[nodiscard]] int attack_slowdown() const;
    [[nodiscard]] int attack_damage_penalty() const;

    // Leg fractures slow walking (splints help).
    [[nodiscard]] bn::fixed speed_multiplier() const;

    // Tired 4 (or fatigue past the limit): forced sleep.
    [[nodiscard]] bool must_pass_out() const
    {
        return _fatigue >= full || level(moodle::TIRED) >= tired_tuning::forced_sleep_level;
    }

    // A timed action (frames or minutes) as long as it takes this tired.
    [[nodiscard]] int action_time(int value) const
    {
        return (value * tired_tuning::action_percent[level(moodle::TIRED)] + 50) / 100;
    }

    // ----- events -----

    // A new wound; rolls the zombie infection for scratches, lacerations and bites from zombies.
    void add_wound(body_part part, wound_type type, bool from_zombie, bn::random& random);

    void add_panic(int amount);

    void bitten_to_death()
    {
        _death = death_cause::BITTEN;
    }

    // Eats or drinks one unit; `age_days` is how old the food is.
    void consume(item_id id, int age_days, bn::random& random);

    void take_painkillers();
    void take_antibiotics();

    // Milestone 14: vitamins and cold pills take some sickness away; sleeping pills make you tired enough to sleep.
    void ease_sickness(int amount);
    void take_sleeping_pills();

    // Treatment on a body part with what is carried: bandage, replace a dirty bandage, disinfect or splint.
    // `describe` fills a short label ("APPLY BANDAGE") and returns the item it would use (NONE if nothing).
    // can_stitch: First Aid 2, needed to stitch a deep wound with a suture needle.
    [[nodiscard]] item_id treatment(body_part part, const inventory& items, const char** label,
                                    bool can_stitch = false) const;
    bool treat(body_part part, inventory& items, bool can_stitch = false);

    // Something lifts the mood (the radio): unhappiness down by this much.
    void cheer(int amount)
    {
        _unhappiness = bn::max(_unhappiness - amount, 0);
    }

    // Water from a tap, a rain collector, a river or a pond (points of 100); unboiled water may make you sick.
    void drink(int water, int sick_percent, int sick_amount, bn::random& random);

    // First Aid: the wound on a body part heals this much sooner.
    void quicken_healing(body_part part, int percent);

    // A cigarette (milestone 13): happier and calmer; the craving starts over.
    void smoke()
    {
        _unhappiness = bn::max(_unhappiness - smoker_tuning::cheer, 0);
        _panic = bn::max(_panic - smoker_tuning::calm, 0);
        _since_smoke = 0;
    }

    // The Smoker's craving: past 8 hours without a cigarette.
    [[nodiscard]] bool craving() const
    {
        return _since_smoke > smoker_tuning::craving_minutes;
    }

    [[nodiscard]] int minutes_since_smoke() const
    {
        return _since_smoke;
    }

    // Swimming (milestone 13): soaked through at once.
    void soak()
    {
        _wetness = 100;
    }

    // Drowning (milestone 13): a point of health; at none the survivor drowns.
    void drown()
    {
        if(--_health <= 0 && _death == death_cause::NONE)
        {
            _death = death_cause::DROWNED;
        }
    }

    // Wakes up from sleep: fatigue recovered by the minute ticks; this clears forced sleep.
    void woke_up()
    {
        _fatigue = bn::min(_fatigue, full - 1);
    }

private:
    int _food = 80000;
    int _water = 80000;
    int _fatigue = 10000;
    int _endurance = full;
    int _wetness = 0;               // 0..100
    int _temperature = 370;         // tenths of a degree
    int _panic = 0;                 // 0..100
    int _unhappiness = 10;          // 0..100
    int _sickness = 0;              // 0..100
    int _blood = 5000;              // ml
    int _health = 100;
    int _painkiller_minutes = 0;
    int _antibiotic_minutes = 0;
    int _infection_minutes = -1;    // since the zombie infection, -1 if not infected
    int _infection_length = 0;      // minutes from infection to death
    int _infection_progress = 0;    // 0-100 once the fever shows
    int _minute = 0;
    int _since_smoke = 0;           // the Smoker's minutes since the last cigarette (milestone 13)
    bn::array<body_wound, int(body_part::COUNT)> _wounds;
    bn::array<uint8_t, int(moodle::COUNT)> _levels = {};
    death_cause _death = death_cause::NONE;

    [[nodiscard]] int _pain() const;
    [[nodiscard]] int _bleeding() const;
    void _update_wounds(bn::random& random, int heal_percent);
    void _update_levels(const survival_input& input);
};

[[nodiscard]] int food_age_days(const item_stack& stack, int today);

// "FRESH", "STALE" or "ROTTEN" for perishable food, nullptr otherwise.
[[nodiscard]] const char* freshness_name(const item_stack& stack, int today);

}

#endif
