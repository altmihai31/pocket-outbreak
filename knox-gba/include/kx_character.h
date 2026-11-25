#ifndef KX_CHARACTER_H
#define KX_CHARACTER_H

#include "bn_common.h"

#include "kx_items.h"
#include "kx_recipes.h"

namespace kx
{

// The survivor's character (spec: Character): an occupation and traits chosen at the start, then skills that
// improve by doing. Under 100 bytes of save.

// Skills in the Skills tab's order: the two body stats first (spec: Fitness and Strength), then the 14 skills.
enum class skill : uint8_t
{
    FITNESS,
    STRENGTH,
    BLUNT,
    BLADE,
    AXE,
    SPEAR,
    AIMING,
    RELOADING,
    FIRST_AID,
    SNEAKING,
    LIGHTFOOT,
    FORAGING,
    COOKING,
    FARMING,
    CARPENTRY,
    MECHANICS,
    COUNT
};

constexpr int skill_count = int(skill::COUNT);
constexpr int max_level = 5;
constexpr int book_volumes = 5;

[[nodiscard]] constexpr bool body_stat(skill value)
{
    return value == skill::FITNESS || value == skill::STRENGTH;
}

// Good traits cost points, bad ones give them; they come in opposite pairs (a good one, then its opposite).
enum class trait : uint8_t
{
    STRONG,
    WEAK,
    ATHLETIC,
    OUT_OF_SHAPE,
    FAST_LEARNER,
    SLOW_LEARNER,
    LIGHT_EATER,
    BIG_APPETITE,
    FAST_HEALER,
    SLOW_HEALER,
    GRACEFUL,
    CLUMSY,
    UNNOTICED,
    NOTICEABLE,
    BRAVE,
    COWARDLY,
    KEEN_HEARING,       // milestone 12.5
    HARD_OF_HEARING,
    OUTDOORSMAN,
    HOMEBODY,
    EAGLE_EYED,
    SHORT_SIGHTED,
    SMOKER,             // milestone 13: a bad trait with no good opposite (the last row has it alone)
    COUNT
};

static_assert(int(trait::COUNT) <= 32);

constexpr int trait_count = int(trait::COUNT);

// A trait with no opposite (the Smoker): bad, and its own opposite.
[[nodiscard]] constexpr bool lone_trait(trait value)
{
    return value == trait::SMOKER;
}

[[nodiscard]] constexpr bool good_trait(trait value)
{
    return (int(value) & 1) == 0 && ! lone_trait(value);
}

[[nodiscard]] constexpr trait opposite(trait value)
{
    return lone_trait(value) ? value : trait(int(value) ^ 1);
}

enum class occupation : uint8_t
{
    UNEMPLOYED,
    MECHANIC,
    CARPENTER,
    NURSE,
    POLICE_OFFICER,
    FIRE_OFFICER,
    CHEF,
    FARMER,
    BURGLAR,
    PARK_RANGER,
    COUNT
};

constexpr int occupation_count = int(occupation::COUNT);

struct skill_info
{
    const char* name;
    bool trains_now;                // something in the game gives it XP yet
    const char* effect[4];          // what each level does (Skills tab, up to 4 lines)
    const char* training[3];        // how it trains
};

struct trait_info
{
    const char* name;
    int8_t points;                  // spent (good) or given back (bad)
    const char* effect;             // one line for the creation screen
};

struct starting_skill
{
    skill value;
    uint8_t levels;
};

struct occupation_info
{
    const char* name;
    starting_skill skills[3];
    uint8_t skill_count;
    int8_t bonus_points;            // Unemployed: +8 trait points
    const char* perk;               // nullptr if none
};

// ---------------------------------------------------------------------------------------------------------------------
// Tuning: XP, books and what levels and traits do. Fixed values (the character is chosen once).
// ---------------------------------------------------------------------------------------------------------------------
namespace character_tuning
{
    // XP to go from each level to the next (skills, then the slower body stats).
    constexpr int skill_xp[max_level] = { 60, 120, 240, 480, 960 };
    constexpr int body_xp[max_level] = { 200, 400, 800, 1600, 3200 };

    // Body stats start here; traits and occupations move them.
    constexpr int body_base = 2;

    // Skill books (spec: Books): volume n covers level n-1 to n and multiplies its XP once read.
    constexpr int book_multiplier[book_volumes] = { 2, 3, 4, 5, 5 };

    // Found skill books: how often each volume turns up, and the in-game minutes reading one takes per volume.
    constexpr int volume_weights[book_volumes] = { 40, 26, 18, 10, 6 };
    constexpr int reading_minutes_per_volume = 60;

    // XP gains.
    constexpr int hit_xp = 2;                   // per zombie hit with a weapon of that skill
    constexpr int kill_xp = 6;                  // per kill
    constexpr int treat_xp = 15;                // per wound treated
    constexpr int sneak_xp = 3;                 // per minute crouch-walking near zombies that have not noticed you
    constexpr int lightfoot_xp = 1;             // per minute walking or jogging (not sprinting)
    constexpr int fitness_xp_per = 50;          // 1 Fitness XP per this many exertion points
    constexpr int strength_heavy_xp = 1;        // per minute carrying 75% or more of capacity
    constexpr int strength_hit_xp = 1;          // per melee hit
    constexpr int heavy_load_percent = 75;
    constexpr int forage_xp = 8;                // per patch foraged
    constexpr int forage_levels_per_find = 2;   // an extra find every this many Foraging levels

    // Melee skills: a chance per level of +1 damage, and swings recover a frame faster per level.
    constexpr int extra_damage_percent = 12;
    constexpr int recovery_frames_per_level = 1;

    // First Aid: a treated wound heals this much faster per level.
    constexpr int first_aid_heal_percent = 10;
    constexpr int stitch_level = 2;             // spec: stitching wounds at First Aid 2
    constexpr int reload_xp = 3;                // a reload finished at the end of its bar
    constexpr int fast_reload_xp = 8;           // finished at the sweet spot

    // Sneaking: while crouched, zombies see and hear you this much less per level.
    constexpr int sneak_percent = 8;

    // Lightfoot: footsteps make a tile less noise at these levels.
    constexpr int lightfoot_quieter_levels[2] = { 2, 4 };

    // Fitness and Strength around the base level: endurance cost, carry capacity (10 g units) and knockback.
    constexpr int endurance_percent_per_level = 10;
    constexpr int capacity_per_level = 150;
    constexpr int knockback_percent_per_level = 10;

    // Traits.
    constexpr int trait_body_levels = 2;        // Strong, Weak, Athletic, Out of Shape
    constexpr int fast_learner_percent = 130;
    constexpr int slow_learner_percent = 70;
    constexpr int light_eater_percent = 75;
    constexpr int big_appetite_percent = 125;
    constexpr int fast_healer_percent = 150;
    constexpr int slow_healer_percent = 50;
    constexpr int unnoticed_percent = 75;       // zombie sight against you
    constexpr int noticeable_percent = 125;
    constexpr int brave_percent = 50;           // panic gained
    constexpr int cowardly_percent = 150;
    constexpr int keen_hearing_percent = 150;    // sounds carry this much farther (milestone 12.5)
    constexpr int hard_of_hearing_percent = 60;
    constexpr int outdoorsman_percent = 50;      // of what rain and cold do
    constexpr int homebody_percent = 150;
    constexpr int eagle_eyed_percent = 125;      // of a flashlight's reach
    constexpr int short_sighted_percent = 75;
    constexpr int sight_dusk_minutes = 60;       // dusk this much later (Eagle-eyed) or sooner (Short-sighted)
    constexpr int nurse_bandage_percent = 200;  // perk: bandages stay clean twice as long

    // Unhappy (milestone 13): XP gained at Unhappy 0-4, applied after traits and books.
    constexpr int unhappy_xp_percent[5] = { 100, 90, 75, 60, 50 };
}

// The skill a book teaches; false for other items.
[[nodiscard]] constexpr bool book_skill(item_id id, skill& result)
{
    if(id < item_id::BOOK_BLUNT || id > item_id::BOOK_MECHANICS)
    {
        return false;
    }

    result = skill(int(skill::BLUNT) + int(id) - int(item_id::BOOK_BLUNT));
    return true;
}

static_assert(int(item_id::BOOK_MECHANICS) - int(item_id::BOOK_BLUNT) == int(skill::MECHANICS) - int(skill::BLUNT));

extern const skill_info skill_infos[skill_count];
extern const trait_info trait_infos[trait_count];
extern const occupation_info occupation_infos[occupation_count];

[[nodiscard]] inline const skill_info& info(skill value)
{
    return skill_infos[int(value)];
}

[[nodiscard]] inline const trait_info& info(trait value)
{
    return trait_infos[int(value)];
}

[[nodiscard]] inline const occupation_info& info(occupation value)
{
    return occupation_infos[int(value)];
}

// What the creation screen chose.
struct character_choice
{
    occupation job = occupation::UNEMPLOYED;
    uint32_t traits = 0;            // bit per trait
};

// The saved character: plain data (copied as is into the save).
class character
{

public:
    // A fresh survivor: the occupation's skills, then the traits (body stats first moved by them).
    void start(occupation job, uint32_t traits);

    [[nodiscard]] occupation job() const
    {
        return _job;
    }

    [[nodiscard]] bool has(trait value) const
    {
        return _traits & (1u << int(value));
    }

    [[nodiscard]] int level(skill value) const
    {
        return _levels[int(value)];
    }

    // XP into the current level, and what the next level needs (0 at the top).
    [[nodiscard]] int xp(skill value) const
    {
        return _xp[int(value)];
    }

    [[nodiscard]] int xp_needed(skill value) const;

    // Levels from the occupation (shown in the Skills tab).
    [[nodiscard]] int occupation_levels(skill value) const;

    // Books (spec: Books): volume 1-5 of a skill read.
    [[nodiscard]] bool read(skill value, int volume) const
    {
        return _books[int(value)] & (1 << (volume - 1));
    }

    void mark_read(skill value, int volume)
    {
        _books[int(value)] = uint8_t(_books[int(value)] | (1 << (volume - 1)));
    }

    // The book volume that covers the current level (1-5), or 0 at the top.
    [[nodiscard]] int current_volume(skill value) const
    {
        return level(value) < max_level ? level(value) + 1 : 0;
    }

    // Magazines (spec: they teach single recipes). The Chef knows canning from the start (spec: recipes start known).
    [[nodiscard]] bool knows(magazine value) const
    {
        return value == magazine::NONE || (_magazines & (1 << int(value))) ||
               (value == magazine::CANNING && _job == occupation::CHEF);
    }

    void learn(magazine value)
    {
        _magazines = uint8_t(_magazines | (1 << int(value)));
    }

    // XP multiplier from books read for the current level (1 without).
    [[nodiscard]] int book_multiplier(skill value) const;

    // Adds XP (traits and books apply, then mood_percent: Unhappy); returns true when a level was gained.
    bool add_xp(skill value, int amount, int mood_percent = 100);

    // ----- effects -----

    [[nodiscard]] int percent_xp() const;
    [[nodiscard]] int hunger_percent() const;
    [[nodiscard]] int heal_percent() const;
    [[nodiscard]] int panic_percent() const;
    [[nodiscard]] int endurance_percent() const;
    [[nodiscard]] int bandage_percent() const;

    // Milestone 12.5 traits. Hearing: how far away sounds carry (Keen Hearing 150%, Hard of Hearing 60%), and
    // whether they come from their side. Weather: how much rain and cold affect you (Outdoorsman 50%, Homebody
    // 150%). Sight: how far a flashlight reaches (Eagle-eyed 125%, Short-sighted 75%) and how many minutes later
    // dusk falls for you (60 or -60; dawn comes as much earlier).
    [[nodiscard]] int hearing_percent() const;
    [[nodiscard]] bool stereo() const;
    [[nodiscard]] int weather_percent() const;
    [[nodiscard]] int beam_percent() const;
    [[nodiscard]] int dusk_shift_minutes() const;
    [[nodiscard]] int capacity_bonus() const;           // 10 g units
    [[nodiscard]] int knockback_percent() const;
    [[nodiscard]] int seen_percent(bool crouched) const;    // zombie sight against the survivor
    [[nodiscard]] int heard_percent(bool crouched) const;   // zombie hearing of the survivor
    [[nodiscard]] int footstep_noise_change() const;        // tiles added to footstep noise (negative: quieter)

private:
    uint8_t _levels[skill_count] = {};
    uint16_t _xp[skill_count] = {};
    uint8_t _books[skill_count] = {};
    uint32_t _traits = 0;
    occupation _job = occupation::UNEMPLOYED;
    uint8_t _magazines = 0;         // magazines read (bit per kx::magazine)
};

static_assert(sizeof(character) < 100);

}

#endif
