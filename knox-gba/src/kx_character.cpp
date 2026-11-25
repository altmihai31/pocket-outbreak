#include "kx_character.h"

#include "bn_math.h"

namespace kx
{

using namespace character_tuning;

const skill_info skill_infos[skill_count] = {
    { "FITNESS", true, { "ENDURANCE LASTS", "10% LONGER PER", "LEVEL.", nullptr },
      { "SPRINTING AND", "FIGHTING.", nullptr } },
    { "STRENGTH", true, { "CARRY +1.5 KG AND", "KNOCKBACK +10%", "PER LEVEL.", nullptr },
      { "HITTING, AND", "CARRYING A HEAVY", "LOAD." } },
    { "BLUNT", true, { "BATS, PIPES,", "CROWBARS: MORE", "DAMAGE, FASTER", "SWINGS PER LEVEL." },
      { "HITTING WITH", "BLUNT WEAPONS.", nullptr } },
    { "BLADE", true, { "KNIVES: MORE", "DAMAGE, FASTER", "SWINGS PER LEVEL.", nullptr },
      { "HITTING WITH", "KNIVES.", nullptr } },
    { "AXE", true, { "AXE HITS: MORE", "DAMAGE, FASTER", "SWINGS PER LEVEL.", nullptr },
      { "HITTING WITH", "AXES.", nullptr } },
    { "SPEAR", true, { "SPEARS: MORE", "DAMAGE, FASTER", "THRUSTS.", nullptr },
      { "HITTING WITH", "SPEARS (CRAFTED:", "STICKS AND A KNIFE)." } },
    { "AIMING", true, { "THE AIM CONE IS", "2 DEGREES", "NARROWER PER", "LEVEL." },
      { "HITTING ZOMBIES", "WITH GUNS.", nullptr } },
    { "RELOADING", true, { "A SHORTER RELOAD", "BAR AND A WIDER", "SWEET SPOT PER", "LEVEL." },
      { "RELOADING GUNS,", "MORE AT THE", "SWEET SPOT." } },
    { "FIRST AID", true, { "TREATED WOUNDS", "HEAL 10% FASTER", "PER LEVEL. LEVEL 2:", "STITCHES." },
      { "TREATING", "WOUNDS.", nullptr } },
    { "SNEAKING", true, { "CROUCHED, ZOMBIES", "SEE AND HEAR YOU", "8% LESS PER", "LEVEL." },
      { "CROUCH-WALKING", "NEAR ZOMBIES", "UNNOTICED." } },
    { "LIGHTFOOT", true, { "QUIETER STEPS AT", "LEVELS 2 AND 4.", nullptr, nullptr },
      { "WALKING AND", "JOGGING.", nullptr } },
    { "FORAGING", true, { "AN EXTRA FIND", "EVERY 2 LEVELS", "WHEN FORAGING.", nullptr },
      { "FORAGING:", "CROUCH IN WOODS", "OR FIELDS, A." } },
    { "COOKING", true, { "MEALS COOK 10%", "FASTER PER LEVEL.", nullptr, nullptr },
      { "COOKING MEALS", "IN THE CRAFT TAB.", nullptr } },
    { "FARMING", true, { "A BIGGER HARVEST", "EVERY 2 LEVELS,", "MORE SEEDS BACK.", nullptr },
      { "DIGGING PLOTS,", "PLANTING, WATERING", "AND HARVESTING." } },
    { "CARPENTRY", true, { "STURDIER", "BARRICADES AT", "LEVEL 3.", nullptr },
      { "BUILDING IN", "BUILD MODE.", nullptr } },
    { "MECHANICS", false, { "HOTWIRING AT", "LEVEL 2.", nullptr, nullptr },
      { "NOT YET.", nullptr, nullptr } },
};

const trait_info trait_infos[trait_count] = {
    { "STRONG", 4, "STRENGTH +2: CARRY MORE, HIT HARDER" },
    { "WEAK", 4, "STRENGTH -2: CARRY LESS, HIT SOFTER" },
    { "ATHLETIC", 4, "FITNESS +2: ENDURANCE LASTS LONGER" },
    { "OUT OF SHAPE", 4, "FITNESS -2: TIRES SOONER" },
    { "FAST LEARNER", 6, "SKILLS GAIN 30% MORE XP" },
    { "SLOW LEARNER", 4, "SKILLS GAIN 30% LESS XP" },
    { "LIGHT EATER", 2, "HUNGRY 25% SLOWER" },
    { "BIG APPETITE", 2, "HUNGRY 25% SOONER" },
    { "FAST HEALER", 3, "WOUNDS HEAL 50% FASTER" },
    { "SLOW HEALER", 3, "WOUNDS HEAL AT HALF SPEED" },
    { "GRACEFUL", 2, "FOOTSTEPS A TILE QUIETER" },
    { "CLUMSY", 2, "FOOTSTEPS A TILE LOUDER" },
    { "UNNOTICED", 3, "ZOMBIES SEE YOU 25% LESS FAR" },
    { "NOTICEABLE", 3, "ZOMBIES SEE YOU 25% FARTHER" },
    { "BRAVE", 3, "PANICS AT HALF THE RATE" },
    { "COWARDLY", 3, "PANICS 50% FASTER" },
    { "KEEN HEARING", 3, "HEARS ZOMBIES 50% FARTHER AWAY" },
    { "HARD OF HEARING", 3, "NO STEREO; DISTANT SOUNDS LOST" },
    { "OUTDOORSMAN", 3, "RAIN AND COLD 50% LESS HARMFUL" },
    { "HOMEBODY", 3, "RAIN AND COLD 50% MORE HARMFUL" },
    { "EAGLE-EYED", 2, "LIGHT REACHES FARTHER; DUSK LATER" },
    { "SHORT-SIGHTED", 2, "LIGHT REACHES LESS; DUSK SOONER" },
    { "SMOKER", 2, "8 H WITHOUT A SMOKE: GLUM, JUMPY" },
};

const occupation_info occupation_infos[occupation_count] = {
    { "UNEMPLOYED", {}, 0, 8, nullptr },
    { "MECHANIC", { { skill::MECHANICS, 3 } }, 1, 0, "HOTWIRING (WITH CARS)" },
    { "CARPENTER", { { skill::CARPENTRY, 3 }, { skill::BLUNT, 1 } }, 2, 0, nullptr },
    { "NURSE", { { skill::FIRST_AID, 3 } }, 1, 0, "BANDAGES STAY CLEAN TWICE AS LONG" },
    { "POLICE OFFICER", { { skill::AIMING, 3 }, { skill::RELOADING, 2 } }, 2, 0, nullptr },
    { "FIRE OFFICER", { { skill::AXE, 1 }, { skill::FITNESS, 1 }, { skill::STRENGTH, 1 } }, 3, 0, nullptr },
    { "CHEF", { { skill::COOKING, 3 } }, 1, 0, "KNOWS CANNING" },
    { "FARMER", { { skill::FARMING, 3 } }, 1, 0, nullptr },
    { "BURGLAR", { { skill::SNEAKING, 2 }, { skill::LIGHTFOOT, 2 } }, 2, 0, "HOTWIRING (WITH CARS)" },
    { "PARK RANGER", { { skill::FORAGING, 2 }, { skill::CARPENTRY, 1 } }, 2, 0, "AN EXTRA FIND IN FORESTS" },
};

namespace
{
    [[nodiscard]] int trait_percent(const character& value, trait good, int good_percent, trait bad, int bad_percent)
    {
        return value.has(good) ? good_percent : value.has(bad) ? bad_percent : 100;
    }
}

void character::start(occupation job, uint32_t traits)
{
    *this = character();
    _job = job;
    _traits = traits;
    _levels[int(skill::FITNESS)] = body_base;
    _levels[int(skill::STRENGTH)] = body_base;

    const occupation_info& job_info = info(job);

    for(int index = 0; index < job_info.skill_count; ++index)
    {
        uint8_t& level = _levels[int(job_info.skills[index].value)];
        level = uint8_t(bn::min(level + job_info.skills[index].levels, max_level));
    }

    auto move = [this](skill value, int levels)
    {
        uint8_t& level = _levels[int(value)];
        level = uint8_t(bn::clamp(level + levels, 0, max_level));
    };

    move(skill::STRENGTH, has(trait::STRONG) ? trait_body_levels : has(trait::WEAK) ? -trait_body_levels : 0);
    move(skill::FITNESS, has(trait::ATHLETIC) ? trait_body_levels : has(trait::OUT_OF_SHAPE) ? -trait_body_levels : 0);
}

int character::xp_needed(skill value) const
{
    int current = level(value);

    if(current >= max_level)
    {
        return 0;
    }

    return body_stat(value) ? body_xp[current] : skill_xp[current];
}

int character::occupation_levels(skill value) const
{
    const occupation_info& job_info = info(_job);

    for(int index = 0; index < job_info.skill_count; ++index)
    {
        if(job_info.skills[index].value == value)
        {
            return job_info.skills[index].levels;
        }
    }

    return 0;
}

int character::book_multiplier(skill value) const
{
    int volume = current_volume(value);
    return volume && ! body_stat(value) && read(value, volume) ? character_tuning::book_multiplier[volume - 1] : 1;
}

bool character::add_xp(skill value, int amount, int mood_percent)
{
    int needed = xp_needed(value);

    if(needed <= 0 || amount <= 0)
    {
        return false;
    }

    int gained = (((amount * percent_xp() * book_multiplier(value) + 50) / 100) * mood_percent + 50) / 100;
    int total = _xp[int(value)] + bn::max(gained, 1);

    if(total < needed)
    {
        _xp[int(value)] = uint16_t(total);
        return false;
    }

    // One level at a time; the rest carries over.
    ++_levels[int(value)];
    int next = xp_needed(value);
    _xp[int(value)] = uint16_t(next ? bn::min(total - needed, next - 1) : 0);
    return true;
}

int character::percent_xp() const
{
    return trait_percent(*this, trait::FAST_LEARNER, fast_learner_percent, trait::SLOW_LEARNER, slow_learner_percent);
}

int character::hunger_percent() const
{
    return trait_percent(*this, trait::LIGHT_EATER, light_eater_percent, trait::BIG_APPETITE, big_appetite_percent);
}

int character::heal_percent() const
{
    return trait_percent(*this, trait::FAST_HEALER, fast_healer_percent, trait::SLOW_HEALER, slow_healer_percent);
}

int character::panic_percent() const
{
    return trait_percent(*this, trait::BRAVE, brave_percent, trait::COWARDLY, cowardly_percent);
}

int character::hearing_percent() const
{
    return trait_percent(*this, trait::KEEN_HEARING, keen_hearing_percent, trait::HARD_OF_HEARING,
                         hard_of_hearing_percent);
}

bool character::stereo() const
{
    return ! has(trait::HARD_OF_HEARING);
}

int character::weather_percent() const
{
    return trait_percent(*this, trait::OUTDOORSMAN, outdoorsman_percent, trait::HOMEBODY, homebody_percent);
}

int character::beam_percent() const
{
    return trait_percent(*this, trait::EAGLE_EYED, eagle_eyed_percent, trait::SHORT_SIGHTED,
                         short_sighted_percent);
}

int character::dusk_shift_minutes() const
{
    return has(trait::EAGLE_EYED) ? sight_dusk_minutes : has(trait::SHORT_SIGHTED) ? -sight_dusk_minutes : 0;
}

int character::endurance_percent() const
{
    return bn::max(100 - ((level(skill::FITNESS) - body_base) * endurance_percent_per_level), 20);
}

int character::bandage_percent() const
{
    return _job == occupation::NURSE ? nurse_bandage_percent : 100;
}

int character::capacity_bonus() const
{
    return (level(skill::STRENGTH) - body_base) * capacity_per_level;
}

int character::knockback_percent() const
{
    return 100 + ((level(skill::STRENGTH) - body_base) * knockback_percent_per_level);
}

int character::seen_percent(bool crouched) const
{
    int result = trait_percent(*this, trait::UNNOTICED, unnoticed_percent, trait::NOTICEABLE, noticeable_percent);

    if(crouched)
    {
        result = (result * (100 - (level(skill::SNEAKING) * sneak_percent))) / 100;
    }

    return result;
}

int character::heard_percent(bool crouched) const
{
    return crouched ? 100 - (level(skill::SNEAKING) * sneak_percent) : 100;
}

int character::footstep_noise_change() const
{
    int result = has(trait::GRACEFUL) ? -1 : has(trait::CLUMSY) ? 1 : 0;

    for(int threshold : lightfoot_quieter_levels)
    {
        result -= level(skill::LIGHTFOOT) >= threshold;
    }

    return result;
}

}
