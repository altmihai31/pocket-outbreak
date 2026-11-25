#include "kx_survival.h"
#include "kx_sandbox.h"

#include "bn_algorithm.h"

#include "kx_inventory.h"

namespace kx
{

namespace
{
    constexpr const char* moodle_names[] = {
        "HUNGRY", "THIRSTY", "TIRED", "EXHAUSTED", "WET", "COLD", "HOT", "PANIC", "UNHAPPY", "HEAVY LOAD",
        "PAIN", "BLEEDING", "SICK", "FEVER"
    };
    constexpr const char* part_names[] = { "HEAD", "TORSO", "L ARM", "R ARM", "L LEG", "R LEG" };
    constexpr const char* wound_names[] = { "", "SCRATCH", "LACERATION", "DEEP WOUND", "FRACTURE", "BURN", "BITE" };
    constexpr const char* death_texts[] = {
        "", "BITTEN", "BLED OUT", "STARVED", "DIED OF THIRST", "FROZE", "HEATSTROKE", "THE INFECTION", "SICKNESS",
        "DROWNED"
    };

    // Per wound type: healing hours, bleeding ml/min when unbandaged, pain.
    struct wound_info
    {
        uint16_t heal_hours;
        uint8_t bleed;
        uint8_t pain;
        bool open;              // can bleed and catch a wound infection
    };

    constexpr wound_info wound_infos[] = {
        { 0, 0, 0, false },     // none
        { 12, 2, 5, true },     // scratch
        { 48, 8, 15, true },    // laceration
        { 96, 15, 25, true },   // deep wound (stitched: no bleeding, heals twice as fast)
        { 504, 0, 30, false },  // fracture: 3 weeks
        { 72, 0, 20, true },    // burn
        { 120, 10, 25, true },  // bite
    };

    constexpr int minutes_per_day = 24 * 60;
    constexpr int fever_start_minutes = 12 * 60;      // spec: the infection is invisible at first

    // Moodle level (0-4) from a value and four ascending thresholds.
    [[nodiscard]] int level_above(int value, int t1, int t2, int t3, int t4)
    {
        return value >= t4 ? 4 : value >= t3 ? 3 : value >= t2 ? 2 : value >= t1 ? 1 : 0;
    }

    [[nodiscard]] int level_below(int value, int t1, int t2, int t3, int t4)
    {
        return value <= t4 ? 4 : value <= t3 ? 3 : value <= t2 ? 2 : value <= t1 ? 1 : 0;
    }

    [[nodiscard]] bool sweet(item_id id)
    {
        return id == item_id::CHOCOLATE || id == item_id::SODA || id == item_id::PEANUT_BUTTER || id == item_id::CHIPS ||
               id == item_id::CANNED_PEACHES || id == item_id::COOKIES || id == item_id::HONEY || id == item_id::JUICE ||
               id == item_id::BEER;
    }
}

const char* moodle_name(moodle value)
{
    return moodle_names[int(value)];
}

const char* body_part_name(body_part value)
{
    return part_names[int(value)];
}

const char* wound_name(wound_type value)
{
    return wound_names[int(value)];
}

int frames_per_minute()
{
    int minutes = sandbox::tuning::day_minutes[sandbox::value(sandbox::setting::DAY_LENGTH)];
    return bn::max((tuning(survival_param::FRAMES_PER_MINUTE) * minutes) / 60, 1);
}

const char* death_text(death_cause value)
{
    return death_texts[int(value)];
}

survival::survival()
{
    _update_levels(survival_input());
}

int survival::_pain() const
{
    int result = 0;

    for(const body_wound& value : _wounds)
    {
        result += wound_infos[int(value.type)].pain + (value.infected ? 10 : 0);
    }

    if(_painkiller_minutes > 0)
    {
        result -= 40;
    }

    return bn::clamp(result, 0, 100);
}

int survival::_bleeding() const
{
    int result = 0;

    for(const body_wound& value : _wounds)
    {
        if(value.bandage == bandage_state::NONE && ! value.stitched)
        {
            result += wound_infos[int(value.type)].bleed;
        }
    }

    return result;
}

void survival::tick_minute(const survival_input& input, bn::random& random)
{
    if(_death != death_cause::NONE)
    {
        return;
    }

    ++_minute;

    // Food, water and fatigue (sleep halves the needs and recovers fatigue).
    int exertion_extra = input.exertion / 150;
    int food_drain = tuning(survival_param::FOOD_DRAIN) * (input.sleeping ? 1 : 2 + exertion_extra) *
                     input.hunger_percent / 200;
    int water_drain = tuning(survival_param::WATER_DRAIN) * (input.sleeping ? 1 : 2 + exertion_extra) / 2;
    _food = bn::max(_food - food_drain, 0);
    _water = bn::max(_water - water_drain, 0);

    if(input.sleeping)
    {
        _fatigue = bn::max(_fatigue - tuning(survival_param::SLEEP_RECOVERY), 0);
    }
    else
    {
        _fatigue = bn::min(_fatigue + tuning(survival_param::FATIGUE_GAIN) + (input.exertion / 20), full);
    }

    // Endurance: exertion drains it; rest recovers it (slower while carrying too much).
    int endurance_cost = input.exertion * tuning(survival_param::ENDURANCE_PER_POINT) * input.endurance_percent / 100;

    if(input.load_percent > 100)
    {
        endurance_cost = (endurance_cost * input.load_percent) / 100;
    }

    if(endurance_cost > 0 && input.exertion >= 50)
    {
        _endurance = bn::max(_endurance - endurance_cost, 0);
    }
    else
    {
        int recovery = tuning(survival_param::ENDURANCE_RECOVERY);
        _endurance = bn::min(_endurance + (input.sleeping ? recovery * 2 : recovery) - endurance_cost, full);
    }

    // Wetness and body temperature: day/night air, clothing, exertion and wet clothes.
    if(input.wading)
    {
        _wetness = bn::min(_wetness + tuning(survival_param::WADING_WETNESS), 100);
    }
    else if(input.rain_wetness > 0)
    {
        _wetness = bn::min(_wetness + bn::max((input.rain_wetness * input.weather_percent) / 100, 1), 100);
    }
    else
    {
        _wetness = bn::max(_wetness - (tuning(survival_param::DRYING) * input.drying_percent / 100), 0);
    }

    bool day = ! is_night(input.minute_of_day);
    // Felt temperature: air, clothing (summer clothes keep you comfortable), exertion, wet clothes.
    int felt = (day ? tuning(survival_param::DAY_TEMP) : tuning(survival_param::NIGHT_TEMP)) + input.season_temp +
               input.insulation +
               (input.exertion / 100) - (_wetness / 8);

    // Cold bites less on an Outdoorsman and more on a Homebody (below a comfortable 18 degrees).
    constexpr int comfortable = 18;

    if(felt < comfortable)
    {
        felt = comfortable - (((comfortable - felt) * input.weather_percent) / 100);
    }
    int target = felt < 8 ? 340 : felt < 12 ? 350 : felt < 15 ? 356 : felt < 18 ? 362 :
                 felt > 42 ? 392 : felt > 39 ? 386 : felt > 36 ? 381 : felt > 33 ? 376 : 370;

    // Zombie infection: hidden for the first hours, then fever and sickness build until death.
    if(_infection_minutes >= 0)
    {
        ++_infection_minutes;

        int incubation = bn::min(fever_start_minutes, _infection_length / 4);

        if(_infection_minutes > incubation)
        {
            int progress = ((_infection_minutes - incubation) * 100) / bn::max(_infection_length - incubation, 1);
            _infection_progress = progress;
            target += bn::min(progress / 4, 25);
            _sickness = bn::max(_sickness, bn::min(progress, 100));
        }

        if(_infection_minutes >= _infection_length)
        {
            _death = death_cause::INFECTION;
        }
    }

    if(_minute % 4 == 0)
    {
        _temperature += _temperature < target ? 1 : (_temperature > target ? -1 : 0);
    }

    // Panic from zombies in view; decays when safe.
    _panic = bn::clamp(_panic + (input.zombies_in_view * tuning(survival_param::PANIC_PER_ZOMBIE) *
                                 input.panic_percent / 100) -
                       tuning(survival_param::PANIC_DECAY), 0, 100);

    // Medicine.
    _painkiller_minutes = bn::max(_painkiller_minutes - 1, 0);

    if(_antibiotic_minutes > 0 && --_antibiotic_minutes == 0)
    {
        for(body_wound& value : _wounds)
        {
            value.infected = false;
        }
    }

    // Wounds: once an hour, healing, dirty bandages and wound infections.
    if(_minute % 60 == 0)
    {
        _update_wounds(random, input.heal_percent);
    }

    for(body_wound& value : _wounds)
    {
        if(value.bandage != bandage_state::NONE &&
                ++value.bandage_minutes >= tuning(survival_param::BANDAGE_DIRTY_HOURS) * 60 * input.bandage_percent / 100)
        {
            value.bandage = bandage_state::DIRTY;
        }
    }

    // Blood: bleeding drains it; it slowly comes back when fed and watered.
    int bleeding = _bleeding();

    if(bleeding > 0)
    {
        _blood -= bleeding;
    }
    else if(_food > 25000 && _water > 25000)
    {
        _blood = bn::min(_blood + tuning(survival_param::BLOOD_REGEN), 5000);
    }

    // Sickness from wound infections; otherwise it fades.
    bool infected_wound = false;

    for(const body_wound& value : _wounds)
    {
        infected_wound |= value.infected;
    }

    if(infected_wound && _minute % 30 == 0)
    {
        _sickness = bn::min(_sickness + 2, 100);
    }
    else if(_infection_minutes < 0 && _minute % 10 == 0)
    {
        _sickness = bn::max(_sickness - 1, 0);
    }

    // Unhappiness: pain, hunger, thirst and sickness wear morale down; it recovers slowly otherwise.
    if(_minute % 30 == 0)
    {
        bool miserable = _pain() > 30 || _food < 25000 || _water < 25000 || _sickness > 40;
        _unhappiness = bn::clamp(_unhappiness + (miserable ? 2 : -1), 0, 100);
    }

    // The Smoker's craving (milestone 13): glum and jumpy after 8 hours without a cigarette.
    if(input.smoker)
    {
        _since_smoke = bn::min(_since_smoke + 1, 30000);

        if(craving())
        {
            int over_hours = (_since_smoke - smoker_tuning::craving_minutes) / 60;

            if(_minute % smoker_tuning::unhappy_every == 0)
            {
                _unhappiness = bn::min(_unhappiness + 1, 100);
            }

            _panic = bn::max(_panic, bn::min(smoker_tuning::panic_floor + (over_hours * smoker_tuning::panic_per_hour),
                                             smoker_tuning::panic_floor_max));
        }
    }

    _update_levels(input);

    // Health: starvation, thirst, cold, heat, sickness and blood loss hurt; rest and food heal.
    int damage_rate = 0;
    death_cause cause = death_cause::NONE;

    auto hurt = [&](bool condition, int per_minutes, death_cause reason)
    {
        if(condition && _minute % per_minutes == 0)
        {
            ++damage_rate;
            cause = reason;
            --_health;
        }
    };

    hurt(_food == 0, 20, death_cause::STARVED);
    hurt(_water == 0, 8, death_cause::THIRST);
    hurt(level(moodle::COLD) >= 4, 10, death_cause::COLD);
    hurt(level(moodle::HOT) >= 4 && _infection_minutes < 0, 10, death_cause::HEAT);
    hurt(level(moodle::SICK) >= 4 && _infection_minutes < 0, 15, death_cause::SICKNESS);
    hurt(_blood < 3000, 10, death_cause::BLED_OUT);

    if(damage_rate == 0 && _minute % (input.sleeping ? 10 : 20) == 0 && _food > 30000 && _water > 30000)
    {
        _health = bn::min(_health + 1, 100);
    }

    if(_blood <= 2000)
    {
        _death = death_cause::BLED_OUT;
    }
    else if(_health <= 0)
    {
        _death = cause != death_cause::NONE ? cause : death_cause::SICKNESS;
    }
}

void survival::_update_wounds(bn::random& random, int heal_percent)
{
    int infection_chance = tuning(survival_param::WOUND_INFECTION);

    for(body_wound& value : _wounds)
    {
        if(value.type == wound_type::NONE)
        {
            continue;
        }

        const wound_info& info = wound_infos[int(value.type)];

        // Open wounds that are unbandaged or under a dirty bandage can get infected.
        if(info.open && ! value.disinfected && ! value.infected && value.bandage != bandage_state::CLEAN &&
                random.get_int(100) < infection_chance)
        {
            value.infected = true;
        }

        // Healing: scratches always, other open wounds only when bandaged, fractures faster when splinted.
        // Stitched wounds heal too, twice as fast.
        bool heals = value.type == wound_type::SCRATCH || value.bandage == bandage_state::CLEAN || value.stitched ||
                     (value.type == wound_type::FRACTURE && (value.splinted || random.get_int(2) == 0));

        if(heals && ! value.infected && value.hours_left > 0 && value.type != wound_type::BITE)
        {
            // Fast healers sometimes heal two hours in one, slow healers sometimes none.
            int hours = heal_percent / 100;
            hours += random.get_int(100) < heal_percent % 100;
            hours *= value.stitched ? 2 : 1;
            value.hours_left = uint16_t(bn::max(int(value.hours_left) - hours, 0));

            if(value.hours_left == 0)
            {
                value = body_wound();
            }
        }
    }
}

void survival::_update_levels(const survival_input& input)
{
    auto set = [this](moodle value, int level)
    {
        _levels[int(value)] = uint8_t(level);
    };

    set(moodle::HUNGRY, level_below(_food, 50000, 30000, 15000, 5000));
    set(moodle::THIRSTY, level_below(_water, 50000, 30000, 15000, 5000));
    set(moodle::TIRED, level_above(_fatigue, 50000, 70000, 85000, 95000));
    set(moodle::EXHAUSTED, level_below(_endurance, 75000, 50000, 25000, 10000));
    set(moodle::WET, level_above(_wetness, 20, 45, 70, 90));
    set(moodle::COLD, level_below(_temperature, 365, 360, 355, 350));
    set(moodle::HOT, _infection_minutes >= 0 ? 0 : level_above(_temperature, 375, 380, 385, 390));
    set(moodle::FEVER, _infection_minutes >= 0 ? level_above(_infection_progress, 1, 30, 60, 85) : 0);
    set(moodle::PANIC, level_above(_panic, 20, 40, 60, 80));
    set(moodle::UNHAPPY, level_above(_unhappiness, 40, 60, 75, 90));
    set(moodle::HEAVY, level_above(input.load_percent, 101, 125, 150, 175));
    set(moodle::PAIN, level_above(_pain(), 10, 30, 50, 75));
    set(moodle::BLEEDING, level_above(_bleeding(), 1, 8, 15, 25));
    set(moodle::SICK, level_above(_sickness, 20, 40, 60, 80));
}

int survival::attack_slowdown() const
{
    return (level(moodle::EXHAUSTED) >= 3 ? 3 : 0) + (level(moodle::TIRED) >= 3 ? 2 : 0) +
           (level(moodle::PAIN) >= 3 ? 1 : 0);
}

int survival::attack_damage_penalty() const
{
    return level(moodle::EXHAUSTED) >= 4 ? 1 : 0;
}

bn::fixed survival::speed_multiplier() const
{
    bn::fixed result = 1;

    for(int part = int(body_part::LEFT_LEG); part <= int(body_part::RIGHT_LEG); ++part)
    {
        if(_wounds[part].type == wound_type::FRACTURE)
        {
            result *= _wounds[part].splinted ? milli(850) : milli(700);
        }
    }

    return result;
}

void survival::add_wound(body_part part, wound_type type, bool from_zombie, bn::random& random)
{
    body_wound& value = _wounds[int(part)];

    // A worse wound replaces a lighter one on the same part.
    if(value.type == wound_type::NONE || int(type) > int(value.type) || type == wound_type::BITE)
    {
        value = body_wound();
        value.type = type;
        value.hours_left = wound_infos[int(type)].heal_hours;
    }

    if(from_zombie && _infection_minutes < 0)
    {
        int chance = type == wound_type::BITE ? 100 :
                     type == wound_type::LACERATION ? tuning(survival_param::LACERATION_INFECTION) :
                     type == wound_type::SCRATCH ? tuning(survival_param::SCRATCH_INFECTION) : 0;

        if(random.get_int(100) < chance)
        {
            int min_hours = tuning(survival_param::INFECTION_MIN_HOURS);
            int max_hours = bn::max(tuning(survival_param::INFECTION_MAX_HOURS), min_hours);
            _infection_minutes = 0;
            _infection_length = (min_hours + random.get_int(max_hours - min_hours + 1)) * 60;
        }
    }

    _unhappiness = bn::min(_unhappiness + 10, 100);
    _update_levels(survival_input());
}

void survival::add_panic(int amount)
{
    _panic = bn::clamp(_panic + amount, 0, 100);
}

void survival::consume(item_id id, int age_days, bn::random& random)
{
    const item_info& item = info(id);
    int fresh_days = item.fresh_days;
    bool rotten = fresh_days > 0 && age_days > fresh_days * 2;
    bool stale = fresh_days > 0 && age_days > fresh_days;
    int food = item.food * 1000;

    // Stale food gives less; rotten and raw food can make you sick (spec: raw meat risks Sick).
    if(stale)
    {
        food /= 2;
    }

    _food = bn::clamp(_food + food, 0, full);
    _water = bn::clamp(_water + (item.water * 1000), 0, full);

    if(item.flags & item_flag::poison)
    {
        _sickness = bn::min(_sickness + 60, 100);
        _unhappiness = bn::min(_unhappiness + 10, 100);
    }
    else if(rotten || ((item.flags & item_flag::raw) && random.get_int(2) == 0))
    {
        _sickness = bn::min(_sickness + (rotten ? 40 : 25), 100);
        _unhappiness = bn::min(_unhappiness + 10, 100);
    }
    else if(sweet(id) || (item.flags & item_flag::cooked))
    {
        _unhappiness = bn::max(_unhappiness - 15, 0);
    }
    else
    {
        _unhappiness = bn::max(_unhappiness - 3, 0);
    }

    _update_levels(survival_input());
}

void survival::drink(int water, int sick_percent, int sick_amount, bn::random& random)
{
    _water = bn::clamp(_water + (water * 1000), 0, full);

    if(sick_percent > 0 && random.get_int(100) < sick_percent)
    {
        _sickness = bn::min(_sickness + sick_amount, 100);
    }

    _update_levels(survival_input());
}

void survival::take_painkillers()
{
    _painkiller_minutes = 4 * 60;
}

void survival::take_antibiotics()
{
    _antibiotic_minutes = 12 * 60;
}

void survival::ease_sickness(int amount)
{
    _sickness = bn::max(_sickness - amount, 0);
    _update_levels(survival_input());
}

void survival::take_sleeping_pills()
{
    _fatigue = bn::max(_fatigue, item_tuning::sleeping_pills_fatigue * 1000);
    _update_levels(survival_input());
}

item_id survival::treatment(body_part part, const inventory& items, const char** label, bool can_stitch) const
{
    const body_wound& value = _wounds[int(part)];
    *label = "NOTHING TO TREAT";

    if(value.type == wound_type::NONE)
    {
        return item_id::NONE;
    }

    bool needs_bandage = value.type != wound_type::FRACTURE && value.bandage != bandage_state::CLEAN;

    if(value.type == wound_type::FRACTURE && ! value.splinted)
    {
        *label = "APPLY SPLINT";
        return items.find(item_id::SPLINT) >= 0 ? item_id::SPLINT : item_id::NONE;
    }

    // Milestone 14: alcohol wipes disinfect too, and a sterile bandage bandages and disinfects at once (so the
    // wipe is saved when one will be used).
    bool unclean = (value.infected || ! value.disinfected) && wound_infos[int(value.type)].open &&
                   value.type != wound_type::BITE;
    bool sterile = items.find(item_id::STERILE_BANDAGE) >= 0;
    item_id wipe = items.find(item_id::DISINFECTANT) >= 0 ? item_id::DISINFECTANT :
                   items.find(item_id::ALCOHOL_WIPES) >= 0 ? item_id::ALCOHOL_WIPES : item_id::NONE;

    if(unclean && wipe != item_id::NONE && ! (needs_bandage && sterile))
    {
        *label = "DISINFECT";
        return wipe;
    }

    // A deep wound sewn shut stops bleeding (spec: stitch at First Aid 2, then bandage).
    bool needle = value.type == wound_type::DEEP_WOUND && ! value.stitched && items.find(item_id::SUTURE_NEEDLE) >= 0;

    if(needle && can_stitch)
    {
        *label = "STITCH";
        return item_id::SUTURE_NEEDLE;
    }

    if(needs_bandage)
    {
        if(unclean && sterile)
        {
            *label = "STERILE BANDAGE";
            return item_id::STERILE_BANDAGE;
        }

        *label = value.bandage == bandage_state::DIRTY ? "NEW BANDAGE" : "APPLY BANDAGE";

        if(items.find(item_id::BANDAGE) >= 0)
        {
            return item_id::BANDAGE;
        }

        if(sterile)
        {
            return item_id::STERILE_BANDAGE;
        }

        if(items.find(item_id::RAG) >= 0)
        {
            *label = value.bandage == bandage_state::DIRTY ? "NEW RAG BANDAGE" : "BANDAGE WITH RAG";
            return item_id::RAG;
        }
    }

    if(needle)
    {
        *label = "STITCHING NEEDS FIRST AID 2";
    }

    return item_id::NONE;
}

bool survival::treat(body_part part, inventory& items, bool can_stitch)
{
    const char* label;
    item_id used = treatment(part, items, &label, can_stitch);

    if(used == item_id::NONE)
    {
        return false;
    }

    body_wound& value = _wounds[int(part)];

    switch(used)
    {

    case item_id::SPLINT:
        value.splinted = true;
        break;

    case item_id::SUTURE_NEEDLE:
        value.stitched = true;
        break;

    case item_id::DISINFECTANT:
    case item_id::ALCOHOL_WIPES:
        value.disinfected = true;
        value.infected = false;
        break;

    case item_id::STERILE_BANDAGE:
        value.disinfected = true;
        value.infected = false;
        value.bandage = bandage_state::CLEAN;
        value.bandage_minutes = 0;
        break;

    default:
        value.bandage = bandage_state::CLEAN;
        value.bandage_minutes = 0;
        break;
    }

    // Disinfectant is a bottle that lasts; bandages, rags, splints and needles (with their thread) are used up.
    if(used != item_id::DISINFECTANT)
    {
        items.consume_one(items.find(used));
    }

    _update_levels(survival_input());
    return true;
}

void survival::quicken_healing(body_part part, int percent)
{
    body_wound& value = _wounds[int(part)];

    if(value.hours_left > 1)
    {
        value.hours_left = uint16_t(bn::max((int(value.hours_left) * (100 - percent)) / 100, 1));
    }
}

int food_age_days(const item_stack& stack, int today)
{
    return bn::max(today - int(stack.extra), 0);
}

const char* freshness_name(const item_stack& stack, int today)
{
    int fresh_days = info(stack.id).fresh_days;

    if(stack.empty() || fresh_days == 0)
    {
        return nullptr;
    }

    int age = food_age_days(stack, today);
    return age > fresh_days * 2 ? "ROTTEN" : age > fresh_days ? "STALE" : "FRESH";
}

}
