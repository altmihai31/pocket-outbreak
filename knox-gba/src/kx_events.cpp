#include "kx_events.h"
#include "kx_light.h"

#include "bn_display.h"
#include "bn_math.h"
#include "bn_sprite_items_heli.h"
#include "bn_sprite_items_smoke.h"

#include "kx_game.h"
#include "kx_hash.h"
#include "kx_hud_tiles.h"
#include "kx_macro_map.h"
#include "kx_save.h"
#include "kx_sfx.h"
#include "kx_timeline.h"
#include "kx_vector.h"
#include "kx_world.h"

namespace kx
{

using namespace events_tuning;

namespace
{
    // Broadcasts by day (spec: survival tips, lore and an emergency channel through the first week, then silent
    // one by one). Up to 3 lines each.
    struct broadcast
    {
        uint8_t first_day;
        uint8_t last_day;
        const char* lines[3];
    };

    constexpr broadcast broadcasts[] = {
        { 1, 3, { "EMERGENCY BROADCAST.", "STAY INDOORS. LOCK YOUR DOORS.", "DO NOT APPROACH THE SICK." } },
        { 1, 4, { "THE INFECTION SPREADS BY BITES.", "A BITTEN PERSON WILL TURN.", "THERE IS NO CURE." } },
        { 1, 6, { "SURVIVAL TIP: NOISE DRAWS THEM.", "WALK, DON'T RUN. CROUCH IN", "LONG GRASS TO HIDE." } },
        { 2, 6, { "SURVIVAL TIP: FILL BOTTLES NOW.", "THE WATER SUPPLY MAY FAIL.", "BOIL WATER FROM RIVERS." } },
        { 2, 7, { "SURVIVAL TIP: BOARD UP WINDOWS.", "THEY BREAK GLASS TO GET IN.", "KEEP PLANKS AND NAILS." } },
        { 3, 7, { "THE ARMY HAS SEALED THE COUNTY.", "ROADS OUT ARE CLOSED.", "WAIT FOR FURTHER NEWS." } },
        { 4, 7, { "EVACUATION CENTRES ARE FULL.", "DO NOT TRAVEL TO THEM.", "SHELTER WHERE YOU ARE." } },
        { 5, 8, { "HELICOPTERS WILL SEARCH", "FOR SURVIVORS IN THE COMING", "DAYS. STAY OUT OF SIGHT." } },
        { 3, 8, { "SURVIVAL TIP: FRIDGES KEEP", "FOOD ONLY WHILE THE POWER", "LASTS. EAT FRESH FOOD FIRST." } },
        { 8, 10, { "...THIS IS AN AUTOMATED", "MESSAGE. REMAIN INDOORS.", "THIS MESSAGE WILL REPEAT..." } },
    };

    // Television only (milestone 13): pictures the radio can't show.
    constexpr broadcast tv_broadcasts[] = {
        { 1, 3, { "NEWS: CROWDS AT THE HOSPITAL.", "POLICE HAVE CLOSED THE ROADS", "INTO ASHFORD." } },
        { 2, 5, { "A MAP OF THE QUARANTINE ZONE.", "THE WHOLE COUNTY IS SHADED RED.", "STAY TUNED FOR UPDATES." } },
        { 3, 7, { "...THE EMERGENCY SERVICES ARE", "NO LONGER ABLE TO REACH ASHFORD.", "STAY INDOORS..." } },
        { 5, 9, { "A TEST CARD. A STEADY TONE.", "NOTHING ELSE ON ANY CHANNEL.", nullptr } },
    };

    constexpr int directions[8][2] = { { 1, 0 }, { 1, 1 }, { 0, 1 }, { -1, 1 }, { -1, 0 }, { -1, -1 }, { 0, -1 },
                                       { 1, -1 } };

    [[nodiscard]] uint32_t event_roll(uint32_t salt, int hour)
    {
        return hash32(world::seed() ^ salt ^ (uint32_t(hour) * 2654435761u));
    }
}

void world_events::reset()
{
    _heli_done = false;
    _crash_count = 0;

    for(crash_site& value : _crashes)
    {
        value = crash_site();
    }

    hide();
}

void world_events::hide()
{
    _heli.reset();
    _smoke.reset();
    _heli_frames = 0;
    _alarm_frames = 0;
    _shots = 0;
}

bn::fixed_point world_events::_somewhere(game& owner, int near, int far) const
{
    // A spot in one of the 8 directions, near to far tiles from the player.
    int index = owner.random().get_int(8);
    bn::fixed_point direction = direction_unit(directions[index][0], directions[index][1]);
    int tiles = near + owner.random().get_int(far - near + 1);
    bn::fixed_point result = owner.hero().position() + (direction * (tiles * world::tile_size));
    return bn::fixed_point(bn::clamp(result.x(), bn::fixed(16), bn::fixed(world::width_px() - 16)),
                           bn::clamp(result.y(), bn::fixed(16), bn::fixed(world::height_px() - 16)));
}

void world_events::_sound(game& owner, int sound, const bn::fixed_point& position, int audible_tiles)
{
    // Distant sounds: quieter with distance, panned towards their side (spec: beyond the screen).
    bn::fixed_point offset = position - owner.hero().position();
    int tiles = approx_length(offset).integer() / world::tile_size;
    audible_tiles = (audible_tiles * sfx::hearing_percent()) / 100;

    if(tiles > audible_tiles)
    {
        return;
    }

    bn::fixed volume = bn::fixed(audible_tiles - tiles + 8) / (audible_tiles + 8);
    int pan = bn::clamp(offset.x().integer() / 4, -120, 120);
    sfx::play(sfx::id(sound), volume, pan);
}

void world_events::_start_heli(game& owner)
{
    // Across the player's area, from one side to the other.
    int index = int(hash32(world::seed() ^ 0x48454c49u) % 8);
    bn::fixed_point direction = direction_unit(directions[index][0], directions[index][1]);
    _heli_position = owner.hero().position() - (direction * (heli_half_path * world::tile_size));
    _heli_step = direction * heli_speed;
    _heli_frames = (heli_half_path * 2 * world::tile_size) / heli_speed;

    bn::sprite_ptr sprite = bn::sprite_items::heli.create_sprite(_heli_position);
    sprite.set_camera(owner.camera());
    sprite.set_bg_priority(1);
    sprite.set_horizontal_flip(direction.x() < 0);
    _heli = bn::move(sprite);
    _heli_done = true;
}

void world_events::update(game& owner)
{
    if(world::test_world())
    {
        return;
    }

    ++_frame;
    int day = (owner.world_minute() / (24 * 60)) + 1;

    // The helicopter, once, on its day and hour.
    if(! _heli_done && ! _heli)
    {
        uint32_t roll = hash32(world::seed() ^ 0x48454c48u);
        int heli_day = heli_first_day + int(roll % uint32_t(heli_last_day - heli_first_day + 1));
        int heli_hour = heli_first_hour + int((roll >> 8) % uint32_t(heli_last_hour - heli_first_hour + 1));

        if(day > heli_day || (day == heli_day && (owner.world_minute() / 60) % 24 >= heli_hour))
        {
            _start_heli(owner);
        }
    }

    if(_heli)
    {
        _heli_position += _heli_step;
        _heli->set_position(_heli_position);

        // Every horde it passes over follows the noise (spec: drags every horde along its path).
        if(_heli_frames % heli_noise_frames == 0)
        {
            owner.noise(_heli_position, heli_noise);
        }

        if(_heli_frames % 20 == 0)
        {
            _sound(owner, int(sfx::id::ROTOR), _heli_position, heli_audible_tiles);
        }

        if(--_heli_frames <= 0)
        {
            _heli.reset();
        }
    }

    // Gunshots in progress: a few shots, a second or so apart.
    if(_shots > 0 && --_shot_timer <= 0)
    {
        --_shots;
        _shot_timer = 30 + owner.random().get_int(60);
        _sound(owner, int(sfx::id::GUNSHOT), _event_position, far_tiles + 40);
        owner.noise(_event_position, gunshot_noise);
    }

    // An alarm going off.
    if(_alarm_frames > 0)
    {
        if(_alarm_frames % 60 == 0)
        {
            _sound(owner, int(sfx::id::ALARM), _event_position, far_tiles + 40);
        }

        if(_alarm_frames % 600 == 0)
        {
            owner.noise(_event_position, alarm_noise);
        }

        --_alarm_frames;
    }

    _update_smoke(owner);
}

void world_events::_update_smoke(game& owner)
{
    // The newest crash site's smoke column, for a few days: on it when on screen, else at the screen's top edge
    // towards it (spec: smoke columns drawn as tall sprites cut off at the screen edge).
    int day = (owner.world_minute() / (24 * 60)) + 1;
    const crash_site* site = nullptr;

    for(int index = 0; index < _crash_count; ++index)
    {
        if(day - _crashes[index].day < smoke_days)
        {
            site = &_crashes[index];
        }
    }

    bn::fixed_point position;

    if(site)
    {
        position = bn::fixed_point((site->tile_x * world::tile_size) + 8, (site->tile_y * world::tile_size) - 12);
        bn::fixed_point offset = position - owner.hero().position();

        if(approx_length(offset).integer() / world::tile_size > smoke_visible_tiles)
        {
            site = nullptr;
        }
    }

    if(! site)
    {
        _smoke.reset();
        return;
    }

    if(! _smoke)
    {
        _smoke = bn::sprite_items::smoke.create_sprite(0, 0);
        _smoke->set_bg_priority(1);
    }

    int x = owner.screen_x(position);
    int y = owner.screen_y(position);
    int half_width = bn::display::width() / 2;
    int half_height = bn::display::height() / 2;
    bool on_screen = bn::abs(x) < half_width + 8 && bn::abs(y) < half_height + 16;
    _smoke->set_position(on_screen ? x : bn::clamp(x, -half_width + 8, half_width - 8),
                         on_screen ? y : -half_height + 16);
    light::shade(*_smoke);

    if(_frame % 16 == 0)
    {
        _smoke->set_tiles(bn::sprite_items::smoke.tiles_item(), (_frame / 16) & 1);
    }
}

void world_events::hour(game& owner)
{
    if(world::test_world())
    {
        return;
    }

    int hour = owner.world_minute() / 60;
    int day = (hour / 24) + 1;
    uint32_t roll = event_roll(0x45564e54u, hour);
    int pick = int(roll % 100);

    if(pick < gunshot_percent)
    {
        _event_position = _somewhere(owner, near_tiles, far_tiles);
        _shots = 1 + int((roll >> 8) % 4);
        _shot_timer = 1;
    }
    else if(pick < gunshot_percent + alarm_percent)
    {
        _event_position = _somewhere(owner, near_tiles, far_tiles);
        _alarm_frames = alarm_frames;
        owner.noise(_event_position, alarm_noise);
    }
    else if(pick < gunshot_percent + alarm_percent + crash_percent && day >= crash_first_day &&
            _crash_count < max_crashes && (! _crash_count || day - _crashes[_crash_count - 1].day >= 2))
    {
        _crash(owner, day);
    }
}

void world_events::_crash(game& owner, int day)
{
    // A crash site: a military crate where it came down, soldiers about, smoke for days.
    bn::fixed_point position = _somewhere(owner, crash_near_tiles, crash_far_tiles);
    int tile_x = position.x().integer() >> world::tile_shift;
    int tile_y = position.y().integer() >> world::tile_shift;

    if(_crash_count >= max_crashes ||
       macro::land_at(tile_x >> macro::cell_shift, tile_y >> macro::cell_shift) == macro::land::WATER ||
       owner.containers().at(tile_x, tile_y) >= 0)
    {
        return;
    }

    crash_site& site = _crashes[_crash_count++];
    site.tile_x = int16_t(tile_x);
    site.tile_y = int16_t(tile_y);
    site.day = int16_t(day);
    static_cast<void>(world::set_wall_gfx(tile_x, tile_y, world_gfx::CRATE));
    owner.refresh_tile(tile_x, tile_y);
    static_cast<void>(owner.containers().add(container_kind::CORPSE, loot_table::MILITARY, tile_x, tile_y));

    uint8_t& count = macro::horde_count(tile_x >> macro::cell_shift, tile_y >> macro::cell_shift);
    count = uint8_t(bn::min(count + crash_zombies, 255));
    _sound(owner, int(sfx::id::THUNDER), position, crash_far_tiles + 60);
    owner.noise(position, crash_noise);
    owner.toast("SOMETHING CRASHED FAR AWAY", hud_tiles::bank_danger, 180);
}

void world_events::trigger(game& owner, int which)
{
    int day = (owner.world_minute() / (24 * 60)) + 1;

    switch(which)
    {

    case 0:
        _start_heli(owner);
        break;

    case 1:
        _event_position = _somewhere(owner, near_tiles, far_tiles);
        _shots = 3;
        _shot_timer = 1;
        break;

    case 2:
        _event_position = _somewhere(owner, near_tiles, far_tiles);
        _alarm_frames = alarm_frames;
        owner.noise(_event_position, alarm_noise);
        break;

    default:
        _crash(owner, day);
        break;
    }
}

void world_events::listen(game& owner)
{
    sfx::play(sfx::id::STATIC, bn::fixed(1) / 2);

    if(owner.hero().items().count(item_id::BATTERY) <= 0)
    {
        owner.toast("THE RADIO NEEDS A BATTERY", hud_tiles::bank_danger, 90);
        return;
    }

    int hour = owner.world_minute() / 60;
    int day = (hour / 24) + 1;
    const broadcast* candidates[int(sizeof(broadcasts) / sizeof(broadcast))];
    int count = 0;

    for(const broadcast& value : broadcasts)
    {
        if(day >= value.first_day && day <= value.last_day)
        {
            candidates[count++] = &value;
        }
    }

    if(! count)
    {
        owner.message("ONLY STATIC ON EVERY CHANNEL.", nullptr, nullptr, radio_frames);
        return;
    }

    // A different broadcast every couple of hours.
    const broadcast& chosen = *candidates[(day * 7 + (hour % 24) / 2) % count];
    owner.message(chosen.lines[0], chosen.lines[1], chosen.lines[2], radio_frames);
    owner.hero().body().cheer(radio_cheer);
}

void world_events::watch_tv(game& owner, bool powered)
{
    sfx::play(sfx::id::STATIC, bn::fixed(1) / 2);

    if(! powered)
    {
        owner.toast("NO POWER", hud_tiles::bank_danger, 60);
        return;
    }

    int hour = owner.world_minute() / 60;
    int day = (hour / 24) + 1;
    constexpr int radio_count = int(sizeof(broadcasts) / sizeof(broadcast));
    constexpr int tv_count = int(sizeof(tv_broadcasts) / sizeof(broadcast));
    const broadcast* candidates[radio_count + tv_count];
    int count = 0;

    for(const broadcast& value : tv_broadcasts)
    {
        if(day >= value.first_day && day <= value.last_day)
        {
            candidates[count++] = &value;
        }
    }

    for(const broadcast& value : broadcasts)
    {
        if(day >= value.first_day && day <= value.last_day)
        {
            candidates[count++] = &value;
        }
    }

    if(! count)
    {
        owner.message("STATIC ON EVERY CHANNEL.", nullptr, nullptr, radio_frames);
        return;
    }

    const broadcast& chosen = *candidates[(day * 5 + (hour % 24) / 2) % count];
    owner.message(chosen.lines[0], chosen.lines[1], chosen.lines[2], radio_frames);
    owner.hero().body().cheer(radio_cheer);
}

void world_events::save(save::writer& out) const
{
    out.put(_heli_done);
    out.put(_crash_count);
    out.put(_crashes);
}

void world_events::load(save::reader& in)
{
    in.get(_heli_done);
    in.get(_crash_count);
    in.get(_crashes);
    _crash_count = uint8_t(bn::min(int(_crash_count), max_crashes));
}

}
