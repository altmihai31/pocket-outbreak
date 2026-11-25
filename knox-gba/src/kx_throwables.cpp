#include "kx_throwables.h"

#include "bn_log.h"
#include "bn_sprite_items_fire.h"
#include "bn_sprite_items_fx.h"

#include "kx_bench.h"
#include "kx_game.h"
#include "kx_hud_tiles.h"
#include "kx_light.h"
#include "kx_sfx.h"
#include "kx_survival.h"
#include "kx_ui.h"
#include "kx_vector.h"
#include "kx_world.h"

namespace kx
{

namespace
{
    // fx frames (tools/assets/fx.py): a bottle, a Molotov, an alarm clock, the shadow under a flying item.
    constexpr int bottle_frame = 23;
    constexpr int molotov_frame = 24;
    constexpr int clock_frame = 25;
    constexpr int shadow_frame = 26;
    constexpr int fire_frames_count = 3;
    constexpr int flicker_frames = 8;
    constexpr int sprite_bg_priority = 2;       // with the characters
    constexpr int flying_bg_priority = 1;       // over the walls while in the air
    constexpr int landing_step = 4;             // px along the throw when looking for walls

    static_assert(bn::sprite_items::fx.tiles_item().graphics_count() > shadow_frame);

    [[nodiscard]] int frame_of(item_id id)
    {
        return id == item_id::MOLOTOV ? molotov_frame : id == item_id::ALARM_CLOCK ? clock_frame : bottle_frame;
    }

    [[nodiscard]] int tile_of(bn::fixed value)
    {
        return value.floor_integer() >> world::tile_shift;
    }

    // Loudness of a sound heard from a world position (the player hears it fainter farther away).
    [[nodiscard]] bn::fixed heard_volume(game& owner, const bn::fixed_point& at, bn::fixed loudest)
    {
        bn::fixed distance = approx_length(at - owner.hero().position());
        constexpr int silent_px = 320;

        if(distance >= silent_px)
        {
            return 0;
        }

        return loudest * (silent_px - distance) / silent_px;
    }
}

throw_manager::throw_manager(const bn::camera_ptr& camera) :
    _camera(camera)
{
}

bn::fixed_point throw_manager::landing(const bn::fixed_point& from, const bn::fixed_point& towards)
{
    bn::fixed_point offset = towards - from;
    bn::fixed distance = approx_length(offset);

    if(distance <= 0)
    {
        return from;
    }

    bn::fixed_point direction = offset / distance;
    int reach = bn::min(distance.integer(), throw_tuning::range_px);
    bn::fixed_point result = from;

    // Walk along the throw: it stops short of the first solid tile.
    for(int along = landing_step; along <= reach; along += landing_step)
    {
        bn::fixed_point next = from + (direction * along);

        if(world::solid(tile_of(next.x()), tile_of(next.y() - 1)))
        {
            return result;
        }

        result = next;
    }

    bn::fixed_point end = from + (direction * reach);
    return world::solid(tile_of(end.x()), tile_of(end.y() - 1)) ? result : end;
}

void throw_manager::launch(game& owner, item_id id, const bn::fixed_point& from, const bn::fixed_point& to)
{
    if(_flying.full())
    {
        // Two in the air at most: the oldest lands at once.
        flight& oldest = _flying.front();
        item_id landed = oldest.id;
        bn::fixed_point at = oldest.to;
        _flying.erase(_flying.begin());
        _land(owner, landed, at);
    }

    int distance = approx_length(to - from).integer();
    int frames = bn::max(throw_tuning::least_frames, (distance * throw_tuning::frames_per_tile) / world::tile_size);
    int arc = bn::max(4, (throw_tuning::arc_px * distance) / throw_tuning::range_px);

    bn::sprite_ptr sprite = bn::sprite_items::fx.create_sprite(from.x(), from.y() - throw_tuning::hand_height,
                                                               frame_of(id));
    sprite.set_camera(_camera);
    sprite.set_bg_priority(flying_bg_priority);
    bn::sprite_ptr shadow = bn::sprite_items::fx.create_sprite(from.x(), from.y() - 2, shadow_frame);
    shadow.set_camera(_camera);
    shadow.set_bg_priority(sprite_bg_priority);
    shadow.set_z_order(32767);
    _flying.push_back(flight{ bn::move(sprite), bn::move(shadow), from, to, id, 0, frames, arc });
    ++_thrown;
    sfx::play(sfx::id::THROW, milli(700), owner.screen_x(from));

    #if KX_BENCH == 11
        BN_LOG("KXTHROW launch ", info(id).name, " distance=", distance, " frames=", frames, " to=",
               to.x().integer(), ",", to.y().integer());
    #endif
}

void throw_manager::update(game& owner)
{
    ++_frame;

    // In the air: along the line, up and down an arc, tumbling.
    for(int index = 0; index < _flying.size();)
    {
        flight& value = _flying[index];

        if(++value.frame >= value.frames)
        {
            item_id id = value.id;
            bn::fixed_point at = value.to;
            _flying.erase(_flying.begin() + index);
            _land(owner, id, at);
            continue;
        }

        bn::fixed_point ground = value.from + (((value.to - value.from) * value.frame) / value.frames);
        int left = value.frames - value.frame;
        int height = ((throw_tuning::hand_height * left) / value.frames) +
                     ((4 * value.arc * value.frame * left) / (value.frames * value.frames));
        value.sprite.set_position(ground.x(), ground.y() - height - 4);
        value.sprite.set_horizontal_flip((value.frame / 4) & 1);
        value.sprite.set_vertical_flip((value.frame / 8) & 1);
        value.shadow.set_position(ground.x(), ground.y() - 2);
        light::shade(value.sprite);
        ++index;
    }

    // Ringing clocks: noise every 2 s; then the clock lies where it fell.
    for(int index = 0; index < _alarms.size();)
    {
        alarm& value = _alarms[index];
        int elapsed = throw_tuning::alarm_frames - value.frames_left;

        if(elapsed % throw_tuning::alarm_interval == 0)
        {
            owner.noise(value.position, throw_tuning::alarm_noise);
            sfx::play(sfx::id::RING, heard_volume(owner, value.position, milli(700)), owner.screen_x(value.position));
            ++_rings;
        }

        // It shakes while the bell rings.
        bool shaking = (elapsed % throw_tuning::alarm_interval) < 20;
        value.sprite.set_x(value.position.x() + (shaking ? ((elapsed >> 1) & 1) * 2 - 1 : 0));
        light::shade(value.sprite);

        if(--value.frames_left <= 0)
        {
            _stop_alarm(owner, index);
            continue;
        }

        ++index;
    }

    _update_fires(owner);
}

void throw_manager::settle(game& owner)
{
    while(! _flying.empty())
    {
        item_id id = _flying.front().id;
        bn::fixed_point at = _flying.front().to;
        _flying.erase(_flying.begin());
        _land(owner, id, at);
    }

    while(! _alarms.empty())
    {
        _stop_alarm(owner, 0);
    }

    _fires.clear();
    _zombies_burning = false;
}

void throw_manager::_land(game& owner, item_id id, const bn::fixed_point& at)
{
    int pan = owner.screen_x(at);

    #if KX_BENCH == 11
        BN_LOG("KXTHROW landed ", info(id).name, " at ", at.x().integer(), ",", at.y().integer());
    #endif

    if(id == item_id::ALARM_CLOCK)
    {
        if(_alarms.full())
        {
            _stop_alarm(owner, 0);
        }

        sfx::play(sfx::id::DROP, milli(600), pan);
        bn::sprite_ptr sprite = bn::sprite_items::fx.create_sprite(at.x(), at.y() - 4, clock_frame);
        sprite.set_camera(_camera);
        sprite.set_bg_priority(sprite_bg_priority);
        _alarms.push_back(alarm{ bn::move(sprite), at, throw_tuning::alarm_frames });
        return;
    }

    // Glass: a bottle or a Molotov smashes.
    ++_smashed;
    sfx::play(sfx::id::GLASS, 1, pan);
    owner.noise(at, throw_tuning::glass_noise);
    owner.fx().spawn(fx_kind::IMPACT, at - bn::fixed_point(0, 3));

    if(id == item_id::MOLOTOV)
    {
        _start_fire(owner, tile_of(at.x()), tile_of(at.y() - 1));
    }
}

void throw_manager::_stop_alarm(game& owner, int index)
{
    // The clock stays on the ground to be picked up again (lost if there is no room for a pile there).
    const bn::fixed_point& at = _alarms[index].position;
    [[maybe_unused]] bool dropped = owner.drop_item_at(make_stack(item_id::ALARM_CLOCK), tile_of(at.x()),
                                                       tile_of(at.y() - 1));

    #if KX_BENCH == 11
        BN_LOG("KXTHROW alarm clock left on the ground: ", dropped);
    #endif

    _alarms.erase(_alarms.begin() + index);
}

void throw_manager::_start_fire(game& owner, int tile_x, int tile_y)
{
    if(_fires.full())
    {
        _fires.erase(_fires.begin());
    }

    fire value{ {}, tile_x, tile_y, throw_tuning::fire_frames };

    // A flame on each tile of the patch that is not a wall.
    for(int dy = -throw_tuning::fire_radius; dy <= throw_tuning::fire_radius; ++dy)
    {
        for(int dx = -throw_tuning::fire_radius; dx <= throw_tuning::fire_radius; ++dx)
        {
            if(world::solid(tile_x + dx, tile_y + dy))
            {
                continue;
            }

            int x = ((tile_x + dx) * world::tile_size) + (world::tile_size / 2);
            int y = ((tile_y + dy) * world::tile_size) + (world::tile_size / 2) - 2;
            bn::sprite_ptr flame = bn::sprite_items::fire.create_sprite(x, y, value.flames.size() % fire_frames_count);
            flame.set_camera(_camera);
            flame.set_bg_priority(sprite_bg_priority);
            value.flames.push_back(bn::move(flame));
        }
    }

    _fires.push_back(bn::move(value));
    ++_fires_started;
    sfx::play(sfx::id::FIRE, 1, owner.screen_x(bn::fixed_point(tile_x * world::tile_size, tile_y * world::tile_size)));

    #if KX_BENCH == 11
        BN_LOG("KXTHROW fire at ", tile_x, ",", tile_y, " flames=", _fires.back().flames.size());
    #endif
}

void throw_manager::_update_fires(game& owner)
{
    bool burn_tick = _frame % throw_tuning::burn_interval == 0;
    player& hero = owner.hero();
    int player_tile_x = tile_of(hero.position().x());
    int player_tile_y = tile_of(hero.position().y() - 1);
    bool player_in_fire = false;

    for(int index = 0; index < _fires.size();)
    {
        fire& value = _fires[index];

        if(--value.frames_left <= 0)
        {
            _fires.erase(_fires.begin() + index);
            continue;
        }

        // Flames flicker through their three pictures, each a step apart.
        if(_frame % flicker_frames == 0)
        {
            int step = _frame / flicker_frames;

            for(int flame = 0; flame < value.flames.size(); ++flame)
            {
                value.flames[flame].set_tiles(bn::sprite_items::fire.tiles_item(),
                                              (step + flame) % fire_frames_count);
            }
        }

        int left = (value.tile_x - throw_tuning::fire_radius) * world::tile_size;
        int top = (value.tile_y - throw_tuning::fire_radius) * world::tile_size;
        int size = ((throw_tuning::fire_radius * 2) + 1) * world::tile_size;

        if(burn_tick)
        {
            owner.zombies().ignite(left, top, left + size - 1, top + size, throw_tuning::burn_ticks_after);
            _zombies_burning = true;
        }

        if(bn::abs(player_tile_x - value.tile_x) <= throw_tuning::fire_radius &&
           bn::abs(player_tile_y - value.tile_y) <= throw_tuning::fire_radius)
        {
            player_in_fire = true;
        }

        if(_frame % throw_tuning::crackle_interval == 0)
        {
            bn::fixed_point centre(left + (size / 2), top + (size / 2));
            bn::fixed volume = heard_volume(owner, centre, milli(350));

            if(volume > 0)
            {
                sfx::play(sfx::id::FIRE, volume, owner.screen_x(centre));
            }
        }

        ++index;
    }

    // Burning zombies lose health every half second, in the fire and for a while after.
    if(burn_tick && _zombies_burning)
    {
        int still_burning = 0;
        int kills = owner.zombies().burn_tick(owner, throw_tuning::burn_damage, still_burning);
        _burn_kills += kills;
        _zombies_burning = still_burning > 0;

        #if KX_BENCH == 11
            if(kills)
            {
                BN_LOG("KXTHROW burned to death: ", kills, " (", _burn_kills, " in all)");
            }
        #endif
    }

    // The survivor standing in a fire: a Burn on a leg at once, and another each second.
    if(_player_burn_timer > 0)
    {
        --_player_burn_timer;
    }

    if(player_in_fire && _player_burn_timer <= 0 && hero.ride() == ride_kind::NONE && ! hero.dead())
    {
        _player_burn_timer = throw_tuning::player_burn_frames;
        body_part leg = owner.random().get_int(2) ? body_part::LEFT_LEG : body_part::RIGHT_LEG;
        hero.body().add_wound(leg, wound_type::BURN, false, owner.random());
        ++_player_burns;
        char text[32];
        int length = ui::append(text, 0, wound_name(wound_type::BURN));
        length = ui::append(text, length, ": ");
        ui::append(text, length, body_part_name(leg));
        owner.toast(text, hud_tiles::bank_danger, 90);
        sfx::play(sfx::id::FIRE, milli(800));

        #if KX_BENCH == 11
            BN_LOG("KXTHROW player burned: ", body_part_name(leg), " (", _player_burns, ")");
        #endif
    }
    else if(! player_in_fire)
    {
        _player_burn_timer = 0;
    }
}

int throw_manager::fire_centres(bn::fixed_point* centres, int capacity) const
{
    int count = 0;

    for(const fire& value : _fires)
    {
        if(count < capacity)
        {
            centres[count++] = bn::fixed_point((value.tile_x * world::tile_size) + (world::tile_size / 2),
                                               (value.tile_y * world::tile_size) + (world::tile_size / 2));
        }
    }

    return count;
}

bool throw_manager::fire_near(int tile_x, int tile_y, int tiles) const
{
    for(const fire& value : _fires)
    {
        if(bn::abs(tile_x - value.tile_x) <= tiles && bn::abs(tile_y - value.tile_y) <= tiles)
        {
            return true;
        }
    }

    return false;
}

}
