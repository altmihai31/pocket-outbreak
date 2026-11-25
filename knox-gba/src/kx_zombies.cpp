#include "kx_zombies.h"
#include "kx_sandbox.h"
#include "kx_light.h"

#include "bn_log.h"

#include "bn_display.h"
#include "bn_math.h"
#include "bn_sprite_items_zombie.h"
#include "bn_sprite_palette_item.h"

#include "kx_base.h"
#include "kx_flash.h"
#include "kx_flow_field.h"
#include "kx_game.h"
#include "kx_macro_map.h"
#include "kx_sfx.h"
#include "kx_sprite_frames.h"
#include "kx_survivors.h"
#include "kx_survival.h"
#include "kx_terrain.h"
#include "kx_vector.h"
#include "kx_world.h"
#include "kx_world_gen_tuning.h"

namespace kx
{

namespace
{
    constexpr int sprite_feet_offset = 8;
    // Lying frames hold the body in the sprite's top 9 rows: drawn this far down it lies over its feet, inside the
    // collision box, instead of 8-16 px above them where it showed over walls (playtest 1).
    constexpr int lying_feet_offset = -1;
    constexpr int wander_range_px = 48;
    constexpr int wander_chance = 90;           // 1/n per decision to start wandering
    constexpr int frames_per_step = 10;         // shamble animation rate at shambler speed
    constexpr int grab_offset_px = 6;
    constexpr int offscreen_margin = 24;

    [[nodiscard]] int zombie_param_int(zombie_param param)
    {
        return tuning(param).integer();
    }

    [[nodiscard]] bool lying(zombie_state state)
    {
        return state == zombie_state::DOWN || state == zombie_state::CRAWL || state == zombie_state::DYING;
    }

    [[nodiscard]] bool aware(zombie_state state)
    {
        return state != zombie_state::IDLE && state != zombie_state::WANDER && state != zombie_state::SEARCH;
    }
}

zombie_manager::zombie_manager(const bn::camera_ptr& camera) :
    _camera(camera),
    _flash_palette(flash_palette_item.create_palette())
{
    for(int index = 0; index < int(zombie_outfit::COUNT); ++index)
    {
        bn::sprite_palette_item item(outfit::zombie_palettes[index], bn::bpp_mode::BPP_4);
        _outfit_palettes.push_back(item.create_palette());
    }
}

bool zombie_manager::_blocked_ahead(const bn::fixed_point& from, const bn::fixed_point& direction)
{
    // The tile a few pixels ahead along a unit direction is solid (and not a window or barricade to break).
    bn::fixed_point ahead = from + (direction * straight_probe_px);
    int tile_x = ahead.x().integer() >> world::tile_shift;
    int tile_y = (ahead.y().integer() - 1) >> world::tile_shift;
    return world::solid(tile_x, tile_y) && ! world::breakable(tile_x, tile_y);
}

int zombie_manager::_search_frames()
{
    int low = zombie_param_int(zombie_param::SEARCH_FRAMES);
    int high = bn::max(zombie_param_int(zombie_param::SEARCH_MAX_FRAMES), low);
    return low + _random.get_int(high - low + 1);
}

void zombie_manager::spawn(const bn::fixed_point& position, bn::random& random)
{
    if(_zombies.full())
    {
        return;
    }

    bn::sprite_ptr sprite = bn::sprite_items::zombie.create_sprite(position.x(), position.y() - sprite_feet_offset);
    sprite.set_camera(_camera);
    sprite.set_bg_priority(2);
    light::shade(sprite);
    // Outfit by the zone it appears in.
    const uint8_t* weights = test_map_outfit_weights;

    if(! world::test_world())
    {
        int cell_x = (position.x().integer() >> world::tile_shift) >> macro::cell_shift;
        int cell_y = (position.y().integer() >> world::tile_shift) >> macro::cell_shift;
        weights = outfit_weights[int(macro::zone_at(cell_x, cell_y))];
    }

    int total = 0;

    for(int index = 0; index < int(zombie_outfit::COUNT); ++index)
    {
        total += weights[index];
    }

    int pick = random.get_int(bn::max(total, 1));
    int outfit = 0;

    while(outfit < int(zombie_outfit::COUNT) - 1 && pick >= weights[outfit])
    {
        pick -= weights[outfit];
        ++outfit;
    }

    // Milestone 14: by the police barricade and the checkpoint, most wear the uniform.
    if(! world::test_world())
    {
        world::road_piece piece = world::road_piece_near(position.x().integer() >> world::tile_shift,
                                                         position.y().integer() >> world::tile_shift,
                                                         world_gen::piece_uniform_radius);

        if(piece == world::road_piece::BARRICADE || piece == world::road_piece::CHECKPOINT)
        {
            if(random.get_int(100) < world_gen::piece_uniform_percent)
            {
                outfit = int(piece == world::road_piece::BARRICADE ? zombie_outfit::POLICE :
                                                                     zombie_outfit::SOLDIER);
            }
        }
    }

    sprite.set_palette(_outfit_palettes[outfit]);

    zombie value(bn::move(sprite));
    value.outfit = zombie_outfit(outfit);
    value.position = position;
    value.facing = bn::fixed_point(0, 1);
    value.target = position;
    value.anim = random.get_int(64);

    // Playtest 1: a body and which way it faces up or down, from the same roll (no new random draws, so the
    // benches' random sequences stay as they were).
    value.body = uint8_t(value.anim % sprite_frames::zombie_bodies);
    value.mirror_front = (value.anim / sprite_frames::zombie_bodies) & 1;

    // Per-individual stats (spec: ~80% shamblers, ~18% fast shamblers, ~2% sprinters; hidden 1-3 toughness).
    int roll = random.get_int(100);
    int sprinters = zombie_param_int(zombie_param::SPRINTER_PERCENT);
    int fast = zombie_param_int(zombie_param::FAST_PERCENT);
    int speed_setting = sandbox::value(sandbox::setting::ZOMBIE_SPEED);

    // The sandbox's zombie speed (milestone 13); normal keeps the tuning table's shares.
    if(sandbox::tuning::sprinter_percent[speed_setting] >= 0)
    {
        sprinters = sandbox::tuning::sprinter_percent[speed_setting];
        fast = sandbox::tuning::fast_percent[speed_setting];
    }

    if(roll < sprinters)
    {
        value.speed = tuning(zombie_param::SPRINTER_SPEED);
    }
    else if(roll < sprinters + fast)
    {
        value.speed = tuning(zombie_param::FAST_SPEED);
    }
    else
    {
        value.speed = tuning(zombie_param::SHAMBLER_SPEED);
    }

    int tier = random.get_int(3);
    value.health = zombie_param_int(tier == 0 ? zombie_param::HP_TIER_1 :
                                    (tier == 1 ? zombie_param::HP_TIER_2 : zombie_param::HP_TIER_3));
    _zombies.push_back(bn::move(value));
}

void zombie_manager::make_last_crawler()
{
    if(! _zombies.empty())
    {
        zombie& value = _zombies.back();
        value.crawler = true;
        _set_state(value, zombie_state::CRAWL, 0);
    }
}

bool zombie_manager::spawn_survivor(const bn::fixed_point& position, int id, const item_id* worn, bn::random& random)
{
    if(_zombies.full())
    {
        return false;
    }

    spawn(position, random);
    zombie& value = _zombies.back();
    value.survivor = int8_t(id);
    value.outfit = zombie_outfit::CIVILIAN;
    value.body = 0;                 // the clothes they died in are on the first body's areas
    value.health = zombie_param_int(zombie_param::HP_TIER_2);

    // The clothes they died in: the player's outfit colours on the zombie's clothing indices (the hat on the
    // hair; bags and gloves have no area on a zombie).
    bn::color colors[16];

    for(int index = 0; index < 16; ++index)
    {
        colors[index] = outfit::zombie_palettes[0][index];
    }

    for(int slot = 0; slot < int(equip_slot::COUNT); ++slot)
    {
        for(const outfit::look& look : outfit::looks)
        {
            if(look.item != worn[slot] || worn[slot] == item_id::NONE)
            {
                continue;
            }

            for(int area = 0; area < look.count; ++area)
            {
                int index = look.colours[area].index;
                int target = index == outfit::hat ? 4 : index == outfit::hips ? outfit::legs :
                             index == outfit::bag || index == outfit::hands ? -1 : index;

                if(target >= 0)
                {
                    colors[target] = look.colours[area].colour;
                }
            }
        }
    }

    value.palette = bn::sprite_palette_item(colors, bn::bpp_mode::BPP_4).create_new_palette();
    value.sprite.set_palette(*value.palette);
    return true;
}

void zombie_manager::clear()
{
    for(const zombie& value : _zombies)
    {
        if(value.survivor >= 0)
        {
            survivors::left(value.survivor, value.position);
        }
    }

    _zombies.clear();
}

int zombie_manager::dissolve(const bn::fixed_point& center, int distance, int limit)
{
    int dissolved = 0;

    for(int index = 0; index < _zombies.size() && dissolved < limit;)
    {
        zombie& value = _zombies[index];
        bn::fixed_point offset = value.position - center;
        int tile_x = value.position.x().integer() >> world::tile_shift;
        int tile_y = value.position.y().integer() >> world::tile_shift;

        if(value.state != zombie_state::DYING && (bn::abs(offset.x()) > distance || bn::abs(offset.y()) > distance ||
                                                  ! world::loaded(tile_x, tile_y)))
        {
            if(value.survivor >= 0)
            {
                survivors::left(value.survivor, value.position);
            }
            else
            {
                uint8_t& count = macro::horde_count(tile_x >> macro::cell_shift, tile_y >> macro::cell_shift);
                count = uint8_t(bn::min(count + 1, 255));
            }

            // The last zombie takes its place (no shifting the rest).
            if(index != _zombies.size() - 1)
            {
                value = bn::move(_zombies.back());
            }

            _zombies.pop_back();
            ++dissolved;
        }
        else
        {
            ++index;
        }
    }

    return dissolved;
}

void zombie_manager::update(game& owner)
{
    ++_frame;
    _update_voices(owner);

    if(_frame % corpse_scan_frames == 0)
    {
        _update_corpses(owner);
    }

    // Milestone 14 sound pass: the nearest zombie walking close by is heard dragging its feet, louder the nearer.
    if(_frame % zombie_sound_tuning::shuffle_interval == 0)
    {
        const bn::fixed_point& feet = owner.hero().position();
        constexpr int reach = zombie_sound_tuning::shuffle_px;
        int nearest = reach * reach;
        const zombie* heard = nullptr;

        for(const zombie& value : _zombies)
        {
            bool walking = value.state == zombie_state::WANDER || value.state == zombie_state::CHASE ||
                           value.state == zombie_state::SEARCH;
            int dx = (value.position.x() - feet.x()).integer();
            int dy = (value.position.y() - feet.y()).integer();
            int distance = (dx * dx) + (dy * dy);

            if(walking && bn::abs(dx) < reach && bn::abs(dy) < reach && distance < nearest)
            {
                nearest = distance;
                heard = &value;
            }
        }

        if(heard)
        {
            bn::fixed volume = bn::fixed(reach * reach - nearest) / (reach * reach);
            sfx::play(sfx::id::SHUFFLE, bn::max(volume, bn::fixed(1) / 5) / 2, owner.screen_x(heard->position));
        }
    }

    frame_params params;
    params.lunge_speed = tuning(zombie_param::LUNGE_SPEED);
    params.crawl_speed = tuning(zombie_param::CRAWL_SPEED);
    params.interval = bn::max(zombie_param_int(zombie_param::DECISION_INTERVAL), 1);
    params.lunge_range = zombie_param_int(zombie_param::LUNGE_RANGE);
    params.grab_range = zombie_param_int(zombie_param::GRAB_RANGE);
    params.windup = zombie_param_int(zombie_param::WINDUP_FRAMES);
    params.lunge_frames = zombie_param_int(zombie_param::LUNGE_FRAMES);
    params.recover = zombie_param_int(zombie_param::RECOVER_FRAMES);
    params.search = zombie_param_int(zombie_param::SEARCH_FRAMES);
    params.search_max = bn::max(zombie_param_int(zombie_param::SEARCH_MAX_FRAMES), params.search);
    params.down = zombie_param_int(zombie_param::DOWN_FRAMES);
    params.track_tiles = zombie_param_int(zombie_param::TRACK_TILES);
    // Sight: 8 tiles by day, 3 at night, 6 at night when the player is lit (a flashlight or headlights on, by a
    // campfire, in a lit building); a player crouched in forest, mud or crops is only seen up close.
    bool night = is_night(owner.minute_of_day());
    bool lit = night && owner.player_lit();
    params.sight = ! night ? zombie_param_int(zombie_param::SIGHT_PX) :
                   lit ? tuning(light_param::LIT_SIGHT_PX) : zombie_param_int(zombie_param::NIGHT_SIGHT_PX);
    params.chase_sight = ! night ? zombie_param_int(zombie_param::CHASE_SIGHT_PX) :
                         lit ? tuning(light_param::LIT_CHASE_SIGHT_PX) :
                               zombie_param_int(zombie_param::NIGHT_CHASE_SIGHT_PX);

    #if KX_BENCH == 10
        if(_frame % 300 == 1)
        {
            BN_LOG("KXNIGHT zombies night=", night, " lit=", lit, " sight=", params.sight, " chase=",
                   params.chase_sight);
        }
    #endif

    if(const player& hero = owner.hero(); hero.crouched())
    {
        int tile_x = hero.position().x().integer() >> world::tile_shift;
        int tile_y = (hero.position().y().integer() - 1) >> world::tile_shift;

        if(info(world::terrain_at(tile_x, tile_y)).hides)
        {
            int hide = zombie_param_int(zombie_param::HIDE_PX);
            params.sight = bn::min(params.sight, hide);
            params.chase_sight = bn::min(params.chase_sight, hide);
        }
    }
    params.cone = zombie_param_int(zombie_param::SIGHT_CONE);

    // Character: Sneaking (crouched), Unnoticed / Noticeable.
    {
        const player& hero = owner.hero();
        int seen = hero.skills().seen_percent(hero.crouched());
        params.sight = (params.sight * seen) / 100;
        params.chase_sight = (params.chase_sight * seen) / 100;
        params.heard_percent = hero.skills().heard_percent(hero.crouched());

        // Weather: rain halves noise (spec), fog cuts sight.
        if(weather::wet(owner.weather()))
        {
            params.heard_percent = (params.heard_percent * weather_tuning::noise_percent) / 100;
        }
        else if(owner.weather() == weather_kind::FOG)
        {
            params.sight = (params.sight * weather_tuning::fog_sight_percent) / 100;
            params.chase_sight = (params.chase_sight * weather_tuning::fog_sight_percent) / 100;
        }
    }
    params.crawl_grab_delay = zombie_param_int(zombie_param::CRAWL_GRAB_DELAY);

    for(int index = 0, limit = _zombies.size(); index < limit; ++index)
    {
        // Full decisions (perception, steering) are staggered: each zombie decides every `interval` frames.
        _think(owner, _zombies[index], (_frame + index) % params.interval == 0, params);
    }

    _move_all(owner.hero().position(), params.interval);

    for(auto it = _zombies.begin(); it != _zombies.end();)
    {
        if(it->state == zombie_state::DYING && it->timer <= 0)
        {
            #if KX_BENCH
                // Playtest 1: the body should end on its corpse decal's spot (feet 8 px across, 10 px down the tile).
                BN_LOG("KXCORPSE x=", it->position.x(), " y=", it->position.y(), " tile_x=",
                       it->position.x().floor_integer() >> world::tile_shift, " tile_y=",
                       (it->position.y().floor_integer() - 1) >> world::tile_shift, " mirrored=",
                       world::decal_mirrored(it->position.x().floor_integer() >> world::tile_shift,
                                             (it->position.y().floor_integer() - 1) >> world::tile_shift),
                       " sprite_flip=", it->sprite.horizontal_flip(), " sprite_y=", it->sprite.y());
            #endif

            owner.add_decal(it->position, world_decal::CORPSE);

            if(it->survivor >= 0)
            {
                survivors::killed(owner, it->survivor, it->position);
            }
            else
            {
                // Playtest 2: the corpse sprite takes over from the dying body in the same frame, on the same spot.
                if(owner.add_corpse(it->position, outfit_loot[int(it->outfit)], it->body))
                {
                    _show_corpse(owner, it->position.x().floor_integer() >> world::tile_shift,
                                 (it->position.y().floor_integer() - 1) >> world::tile_shift, int(it->outfit),
                                 it->body);
                }
            }

            it = _zombies.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void zombie_manager::update_visuals(int camera_left, int camera_top)
{
    // Night: every zombie is shaded when the night begins or ends; through the night, those on screen every frame
    // (a light may cover them; off screen Butano doesn't draw them, and they are shaded again before they show).
    bool reshade = light::sprites_dark() != _shaded_dark;
    bool night = light::sprites_dark();
    _shaded_dark = night;

    if(reshade || (night && light::lights_on()))
    {
        for(corpse_sprite& value : _corpses)
        {
            light::shade(value.sprite);
        }
    }

    for(zombie& value : _zombies)
    {
        // Flash: the reserved all-white palette for a couple of frames after a hit.
        if(value.flash > 0)
        {
            if(--value.flash == 0)
            {
                value.sprite.set_palette(value.palette ? *value.palette : _outfit_palettes[int(value.outfit)]);
            }
        }

        int x = value.position.x().floor_integer() + (value.state == zombie_state::GRAB ? ((value.anim >> 1) & 1) : 0);
        int y = value.position.y().floor_integer();
        int sprite_y = y - (lying(value.state) ? lying_feet_offset : sprite_feet_offset);

        if(x != value.sprite_x || sprite_y != value.sprite_y)
        {
            value.sprite_x = x;
            value.sprite_y = sprite_y;
            value.sprite.set_position(x, sprite_y);
        }

        if(reshade)
        {
            light::shade(value.sprite);
        }

        // Off screen: Butano culls the sprite, so skip the frame, flip and depth work.
        int screen_x = x - camera_left;
        int screen_y = y - camera_top;

        if(screen_x < -offscreen_margin || screen_x > bn::display::width() + offscreen_margin ||
                screen_y < -offscreen_margin || screen_y > bn::display::height() + offscreen_margin)
        {
            continue;
        }

        if(night && ! reshade)
        {
            light::shade(value.sprite);
        }

        // Facing block: side when mostly horizontal, else down/up.
        bn::fixed fx = value.facing.x();
        bn::fixed fy = value.facing.y();
        bool side = bn::abs(fx) * 10 >= bn::abs(fy) * 7;
        int block = side ? 2 : (fy < 0 ? 1 : 0);
        int frame;

        switch(value.state)
        {

        case zombie_state::WINDUP:
            frame = (block * sprite_frames::zombie_poses) + sprite_frames::zombie_windup;
            break;

        case zombie_state::LUNGE:
        case zombie_state::GRAB:
            frame = (block * sprite_frames::zombie_poses) + sprite_frames::zombie_lunge;
            break;

        case zombie_state::DOWN:
            frame = sprite_frames::zombie_crawl;
            break;

        case zombie_state::DYING:
            // Playtest 2: over its last frames the body splashes into its pool of blood (the corpse lies in the
            // full one), so a dead zombie doesn't read as one knocked down.
            frame = value.timer > dying_ease_frames ? sprite_frames::zombie_crawl :
                    sprite_frames::zombie_dead + (value.timer > dying_ease_frames / 2 ? 0 : 1);
            break;

        case zombie_state::CRAWL:
            frame = sprite_frames::zombie_crawl + ((value.anim / 16) & 1);
            break;

        default:
        {
            int step = (value.anim / frames_per_step) & 3;
            int pose = value.velocity == bn::fixed_point() ? sprite_frames::zombie_stand :
                       (step == 1 ? sprite_frames::zombie_step_a :
                       (step == 3 ? sprite_frames::zombie_step_b : sprite_frames::zombie_stand));
            frame = (block * sprite_frames::zombie_poses) + pose;
            break;
        }
        }

        frame += value.body * sprite_frames::zombie_body_frames;

        if(frame != value.sprite_frame)
        {
            value.sprite_frame = frame;
            value.sprite.set_tiles(bn::sprite_items::zombie.tiles_item(), frame);
        }

        // A dying body takes its corpse's side (the decal is mirrored per tile).
        bool flip = value.state == zombie_state::DYING ?
                    world::decal_mirrored(x >> world::tile_shift, (y - 1) >> world::tile_shift) :
                    lying(value.state) ? fx < 0 : side ? fx < 0 : value.mirror_front;

        if(value.sprite.horizontal_flip() != flip)
        {
            value.sprite.set_horizontal_flip(flip);
        }

        int z_order = depth_z(y, camera_top);

        if(value.sprite.z_order() != z_order)
        {
            value.sprite.set_z_order(z_order);
        }
    }
}

attack_result zombie_manager::attack(game& owner, const attack_info& info)
{
    attack_result result;

    // Collect candidates in reach and arc, nearest first (insertion sort; at most 40).
    int candidates[max_zombies];
    bn::fixed distances[max_zombies];
    int count = 0;

    for(int index = 0, limit = _zombies.size(); index < limit; ++index)
    {
        zombie& value = _zombies[index];

        // Swings also hit zombies on the ground (a floor attack); shoves pass over them.
        if(value.state == zombie_state::DYING || value.swing_id == info.swing_id || (info.shove && lying(value.state)))
        {
            continue;
        }

        bn::fixed_point offset = value.position - info.origin;
        bn::fixed distance = approx_length(offset);

        if(distance > info.reach || ! in_arc(info.direction, offset, info.arc_degrees))
        {
            continue;
        }

        int slot = count++;

        while(slot > 0 && distances[slot - 1] > distance)
        {
            candidates[slot] = candidates[slot - 1];
            distances[slot] = distances[slot - 1];
            --slot;
        }

        candidates[slot] = index;
        distances[slot] = distance;
    }

    for(int candidate = 0; candidate < count && result.hits < info.max_targets; ++candidate)
    {
        zombie& value = _zombies[candidates[candidate]];
        value.swing_id = info.swing_id;
        ++result.hits;
        ++_total_hits;

        if(info.shove)
        {
            _hit(owner, value, info, false, false);
            continue;
        }

        // Stealth kill: crouched, from behind, with a blade, on a zombie that has not noticed the player.
        bool behind = dot(value.facing, info.origin - value.position) < 0;
        bool stealth = info.stealth_capable && behind && ! aware(value.state);
        bool kill = stealth || info.damage >= 100 || value.health <= info.damage;
        bool knockdown = ! kill && (lying(value.state) || info.knockback >= tuning(combat_param::KNOCKDOWN));
        value.health -= info.damage;
        result.stealth_kill |= stealth;
        result.kills += kill;
        _hit(owner, value, info, kill, knockdown);
    }

    return result;
}

namespace
{
    // A zombie's middle, where shots aim: the feet position raised to body height.
    constexpr int body_height = 8;

    [[nodiscard]] bn::fixed_point body_centre(const bn::fixed_point& feet)
    {
        return bn::fixed_point(feet.x(), feet.y() - body_height);
    }
}

int zombie_manager::aim_target(const bn::fixed_point& origin, const bn::fixed_point& direction, int range,
                               int arc_degrees) const
{
    int result = -1;
    bn::fixed best = range + 1;

    for(int index = 0, limit = _zombies.size(); index < limit; ++index)
    {
        const zombie& value = _zombies[index];

        if(value.state == zombie_state::DYING)
        {
            continue;
        }

        bn::fixed_point centre = body_centre(value.position);
        bn::fixed_point offset = centre - origin;
        bn::fixed distance = approx_length(offset);

        if(distance < best && in_arc(direction, offset, arc_degrees) &&
                world::line_of_sight(origin.x().integer(), origin.y().integer(), centre.x().integer(),
                                     centre.y().integer()))
        {
            best = distance;
            result = index;
        }
    }

    return result;
}

attack_result zombie_manager::shoot(game& owner, const shot_info& info)
{
    attack_result result;
    zombie* struck = nullptr;
    bn::fixed best = info.range + 1;

    for(zombie& value : _zombies)
    {
        if(value.state == zombie_state::DYING)
        {
            continue;
        }

        // Along the line, and how far off it.
        bn::fixed_point offset = body_centre(value.position) - info.origin;
        bn::fixed along = dot(info.direction, offset);
        bn::fixed across = bn::abs((info.direction.x() * offset.y()) - (info.direction.y() * offset.x()));

        if(along >= 0 && along < best && across <= info.hit_radius)
        {
            best = along;
            struck = &value;
        }
    }

    if(! struck)
    {
        return result;
    }

    bn::fixed_point centre = body_centre(struck->position);

    if(! world::line_of_sight(info.origin.x().integer(), info.origin.y().integer(), centre.x().integer(),
                              centre.y().integer()))
    {
        return result;
    }

    attack_info hit;
    hit.origin = info.origin;
    hit.direction = info.direction;
    hit.damage = info.damage;
    hit.knockback = info.knockback;
    hit.stagger_frames = info.stagger_frames;
    bool kill = struck->health <= info.damage;
    bool knockdown = ! kill && (lying(struck->state) || info.knockback >= tuning(combat_param::KNOCKDOWN));
    struck->health -= info.damage;
    result.hits = 1;
    result.kills = kill;
    ++_total_hits;
    _hit(owner, *struck, hit, kill, knockdown);
    return result;
}

void zombie_manager::ignite(int left, int top, int right, int bottom, int ticks)
{
    for(zombie& value : _zombies)
    {
        int x = value.position.x().integer();
        int y = value.position.y().integer();

        if(value.state != zombie_state::DYING && x >= left && x <= right && y >= top && y <= bottom)
        {
            value.burning = uint8_t(bn::max(int(value.burning), ticks));
        }
    }
}

int zombie_manager::burn_tick(game& owner, int damage, int& still_burning)
{
    int kills = 0;
    still_burning = 0;

    for(zombie& value : _zombies)
    {
        if(value.burning == 0 || value.state == zombie_state::DYING)
        {
            continue;
        }

        // It flashes as it burns, but does not stop: fire does not stagger.
        --value.burning;
        value.health -= damage;
        value.flash = tuning(combat_param::FLASH_FRAMES).integer();

        if(value.flash > 0)
        {
            value.sprite.set_palette(_flash_palette);
        }

        if(value.health <= 0)
        {
            value.burning = 0;
            ++kills;
            _kill(owner, value);
        }
        else if(value.burning > 0)
        {
            ++still_burning;
        }
    }

    return kills;
}

int zombie_manager::stomp_target(const bn::fixed_point& feet, int reach) const
{
    int result = -1;
    bn::fixed best = reach + 1;

    for(int index = 0, limit = _zombies.size(); index < limit; ++index)
    {
        const zombie& value = _zombies[index];

        if(value.state == zombie_state::DOWN || value.state == zombie_state::CRAWL)
        {
            bn::fixed distance = approx_length(value.position - feet);

            if(distance < best)
            {
                best = distance;
                result = index;
            }
        }
    }

    return result;
}

void zombie_manager::stomp(game& owner, int index)
{
    if(index >= 0 && index < _zombies.size())
    {
        zombie& value = _zombies[index];

        if(value.state == zombie_state::DOWN || value.state == zombie_state::CRAWL)
        {
            value.flash = tuning(combat_param::FLASH_FRAMES).integer();
            value.sprite.set_palette(_flash_palette);
            owner.fx().spawn(fx_kind::BLOOD, value.position - bn::fixed_point(0, 3));
            owner.add_decal(value.position, world_decal::BLOOD_B);
            ++_total_stomps;
            _kill(owner, value);
        }
    }
}

int zombie_manager::aware_on_screen(int camera_left, int camera_top) const
{
    int result = 0;

    for(const zombie& value : _zombies)
    {
        int x = value.position.x().integer() - camera_left;
        int y = value.position.y().integer() - camera_top;

        if(x >= 0 && x < bn::display::width() && y >= 0 && y < bn::display::height() + 16 &&
                value.state != zombie_state::IDLE && value.state != zombie_state::WANDER &&
                value.state != zombie_state::DYING)
        {
            ++result;
        }
    }

    return result;
}

int zombie_manager::chasing() const
{
    int result = 0;

    for(const zombie& value : _zombies)
    {
        result += value.state == zombie_state::CHASE || value.state == zombie_state::WINDUP ||
                  value.state == zombie_state::LUNGE || value.state == zombie_state::RECOVER ||
                  value.state == zombie_state::GRAB;
    }

    return result;
}

int zombie_manager::unaware_on_screen(int camera_left, int camera_top) const
{
    int result = 0;

    for(const zombie& value : _zombies)
    {
        int x = value.position.x().integer() - camera_left;
        int y = value.position.y().integer() - camera_top;

        if(x >= 0 && x < bn::display::width() && y >= 0 && y < bn::display::height() + 16 &&
                (value.state == zombie_state::IDLE || value.state == zombie_state::WANDER))
        {
            ++result;
        }
    }

    return result;
}

int zombie_manager::chase_reach(const bn::fixed_point& center) const
{
    int result = 0;

    for(const zombie& value : _zombies)
    {
        if(value.state == zombie_state::CHASE)
        {
            bn::fixed_point offset = value.position - center;
            int tiles = (bn::max(bn::abs(offset.x()), bn::abs(offset.y())).integer() >> world::tile_shift) + 1;
            result = bn::max(result, tiles);
        }
    }

    return result;
}

bool zombie_manager::standing_within(const bn::fixed_point& feet, int distance) const
{
    for(const zombie& value : _zombies)
    {
        if(! lying(value.state) && approx_length(value.position - feet) <= distance)
        {
            return true;
        }
    }

    return false;
}

bool zombie_manager::crawler_underfoot(const bn::fixed_point& feet) const
{
    for(const zombie& value : _zombies)
    {
        if(value.state == zombie_state::DOWN || value.state == zombie_state::CRAWL)
        {
            bn::fixed_point offset = value.position - feet;

            if(bn::abs(offset.x()) < 7 && bn::abs(offset.y()) < 5)
            {
                return true;
            }
        }
    }

    return false;
}

void zombie_manager::release_grabs(game& owner)
{
    const bn::fixed_point& feet = owner.hero().position();
    bn::fixed knockback = tuning(combat_param::SHOVE_KNOCKBACK);

    for(zombie& value : _zombies)
    {
        if(value.state == zombie_state::GRAB)
        {
            bn::fixed_point away = normalized(value.position - feet);

            if(away == bn::fixed_point())
            {
                away = bn::fixed_point(0, 1);
            }

            value.knock = away * knockback;
            value.cooldown = zombie_param_int(zombie_param::RECOVER_FRAMES) * 2;
            _set_state(value, zombie_state::STAGGER, tuning(combat_param::SHOVE_STAGGER).integer());
        }
    }
}

void zombie_manager::hear(const bn::fixed_point& position, int radius_px)
{
    for(zombie& value : _zombies)
    {
        if(value.state == zombie_state::IDLE || value.state == zombie_state::WANDER ||
                value.state == zombie_state::SEARCH)
        {
            if(approx_length(value.position - position) <= radius_px)
            {
                value.target = position;
                value.facing = normalized(position - value.position);
                _set_state(value, zombie_state::SEARCH, _search_frames());
            }
        }
    }
}

void zombie_manager::push_out(int left, int top, int right, int bottom)
{
    // A zombie's feet box is 8 px wide and 5 high.
    constexpr int half_width = 5;
    constexpr int height = 6;

    for(zombie& value : _zombies)
    {
        int x = value.position.x().integer();
        int y = value.position.y().integer();

        if(x + half_width < left || x - half_width > right || y < top || y - height > bottom)
        {
            continue;
        }

        int to_left = x + half_width - left;
        int to_right = right - (x - half_width);
        int to_top = y - top;
        int to_bottom = bottom - (y - height);
        int nearest = bn::min(bn::min(to_left, to_right), bn::min(to_top, to_bottom));

        if(nearest == to_left)
        {
            value.position.set_x(left - half_width - 1);
        }
        else if(nearest == to_right)
        {
            value.position.set_x(right + half_width + 1);
        }
        else if(nearest == to_top)
        {
            value.position.set_y(top - 1);
        }
        else
        {
            value.position.set_y(bottom + height + 1);
        }
    }
}

bool zombie_manager::_perceives(game& owner, const zombie& value, bool chasing, const frame_params& params,
                                bool& in_sight) const
{
    const player& hero = owner.hero();
    in_sight = false;

    if(hero.dead())
    {
        return false;
    }

    bn::fixed_point offset = hero.position() - value.position;
    bn::fixed distance = approx_length(offset);

    // Hearing: the player's current noise radius (tiles).
    if(distance <= (hero.noise_radius() * world::tile_size * params.heard_percent) / 100)
    {
        return true;
    }

    int sight = chasing ? params.chase_sight : params.sight;

    if(distance > sight)
    {
        return false;
    }

    if(! chasing && ! in_arc(value.facing, offset, params.cone))
    {
        return false;
    }

    in_sight = world::line_of_sight(value.position.x().integer(), value.position.y().integer() - 8,
                                    hero.position().x().integer(), hero.position().y().integer() - 8);
    return in_sight;
}

void zombie_manager::_think(game& owner, zombie& value, bool decide, const frame_params& params)
{
    player& hero = owner.hero();
    const bn::fixed_point& feet = hero.position();
    ++value.anim;

    if(value.cooldown > 0)
    {
        --value.cooldown;
    }

    if(value.timer > 0)
    {
        --value.timer;
    }

    switch(value.state)
    {

    case zombie_state::IDLE:
    case zombie_state::WANDER:
    case zombie_state::SEARCH:
        if(! decide)
        {
            break;
        }

        bool in_sight;

        if(_perceives(owner, value, false, params, in_sight))
        {
            // Playtest 2: it growls as it notices you.
            if(_snarl_gap <= 0)
            {
                _voice(owner, value, sfx::id::SNARL, zombie_sound_tuning::snarl_volume);
                _snarl_gap = zombie_sound_tuning::snarl_gap_frames;
            }

            value.target = feet;
            _set_state(value, zombie_state::CHASE, 0);
            value.facing = normalized(value.target - value.position);
            value.velocity = value.facing * value.speed;
            break;
        }

        if(value.state == zombie_state::IDLE)
        {
            value.velocity = bn::fixed_point();

            if(owner.random().get_int(wander_chance) == 0)
            {
                bn::fixed_point offset(owner.random().get_int(wander_range_px * 2) - wander_range_px,
                                       owner.random().get_int(wander_range_px * 2) - wander_range_px);
                value.target = value.position + offset;
                _set_state(value, zombie_state::WANDER, 240);
            }
        }
        else
        {
            bn::fixed_point offset = value.target - value.position;

            if(approx_length(offset) < 4 || value.timer <= 0)
            {
                value.velocity = bn::fixed_point();
                _set_state(value, zombie_state::IDLE, 0);
            }
            else
            {
                value.facing = normalized(offset);
                value.velocity = value.facing * (value.speed / 2);
            }
        }
        break;

    case zombie_state::CHASE:
    {
        if(decide)
        {
            bool seen;
            bool perceived = _perceives(owner, value, true, params, seen);
            int tile_x = value.position.x().integer() >> world::tile_shift;
            int tile_y = (value.position.y().integer() - 1) >> world::tile_shift;

            // Out of sight and earshot, a chaser keeps after the player along the flow field for a few tiles,
            // then searches where it last knew them to be.
            if(perceived)
            {
                value.target = feet;
            }
            else if(flow::distance(tile_x, tile_y) > params.track_tiles)
            {
                _set_state(value, zombie_state::SEARCH, params.search + _random.get_int(params.search_max - params.search + 1));
                break;
            }

            // Straight at a player it can see; otherwise along the flow field, around walls and through doors. A
            // window or barricade in the way is attacked until it breaks (the flow field routes through them).
            bn::fixed_point goal = value.target;
            bn::fixed_point straight;
            bool have_straight = false;
            int next_x;
            int next_y;
            bool stepping = flow::next_step(tile_x, tile_y, next_x, next_y);

            if(stepping && world::breakable(next_x, next_y))
            {
                bn::fixed_point obstacle((next_x * world::tile_size) + (world::tile_size / 2),
                                         (next_y * world::tile_size) + 10);

                if(approx_length(obstacle - value.position) < base_tuning::bash_reach)
                {
                    value.facing = normalized(obstacle - value.position);
                    value.velocity = bn::fixed_point();

                    if(++value.bash_ticks * params.interval >= base_tuning::zombie_hit_frames)
                    {
                        value.bash_ticks = 0;
                        owner.bash(next_x, next_y, value.position);
                    }

                    break;
                }

                goal = obstacle;
            }
            else if(stepping)
            {
                // Out of sight, or in sight across something it can't cross (a pond the player swam, a fence):
                // around it along the flow field (milestone 13). The straight facing is kept when it is used.
                bool around = ! seen;

                if(seen)
                {
                    straight = normalized(goal - value.position);
                    have_straight = true;
                    around = _blocked_ahead(value.position, straight);
                }

                if(around)
                {
                    goal = bn::fixed_point((next_x * world::tile_size) + (world::tile_size / 2),
                                           (next_y * world::tile_size) + 10);
                    have_straight = false;
                }
            }

            value.facing = have_straight ? straight : normalized(goal - value.position);
            value.velocity = value.facing * value.speed;
        }

        bn::fixed_point to_player = feet - value.position;

        if(! hero.dead() && value.cooldown <= 0 && approx_length(to_player) <= params.lunge_range)
        {
            // Telegraphed lunge: the direction is locked at the start of the wind-up so it can be dodged.
            value.lunge_direction = normalized(to_player);
            value.facing = value.lunge_direction;
            value.velocity = bn::fixed_point();
            _set_state(value, zombie_state::WINDUP, params.windup);
        }
        break;
    }

    case zombie_state::WINDUP:
        value.velocity = bn::fixed_point();

        if(value.timer <= 0)
        {
            value.velocity = value.lunge_direction * params.lunge_speed;
            _set_state(value, zombie_state::LUNGE, params.lunge_frames);
        }
        break;

    case zombie_state::LUNGE:
        if(hero.ride() == ride_kind::CAR)
        {
            // The driver is out of reach: the lunge hits the car, and a battered car lets them drag the driver
            // out (milestone 12.5).
            if(approx_length(feet - value.position) <= params.grab_range + vehicle_tuning::besiege_reach_px)
            {
                value.velocity = bn::fixed_point();

                if(owner.zombie_hits_car(value.position) && hero.try_grab(owner))
                {
                    ++_total_grabs;
                    _set_state(value, zombie_state::GRAB, 0);
                }
                else
                {
                    value.cooldown = params.recover;
                    _set_state(value, zombie_state::RECOVER, params.recover);
                }
            }
            else if(value.timer <= 0)
            {
                value.velocity = bn::fixed_point();
                value.cooldown = params.recover;
                _set_state(value, zombie_state::RECOVER, params.recover);
            }
        }
        else if(approx_length(feet - value.position) <= params.grab_range && hero.try_grab(owner))
        {
            ++_total_grabs;
            value.velocity = bn::fixed_point();
            _set_state(value, zombie_state::GRAB, 0);
        }
        else if(value.timer <= 0)
        {
            value.velocity = bn::fixed_point();
            value.cooldown = params.recover;
            _set_state(value, zombie_state::RECOVER, params.recover);
        }
        break;

    case zombie_state::RECOVER:
        if(decide)
        {
            value.facing = normalized(feet - value.position);
            value.velocity = value.facing * (value.speed / 3);
        }

        if(value.timer <= 0)
        {
            value.target = feet;
            _set_state(value, zombie_state::CHASE, 0);
        }
        break;

    case zombie_state::GRAB:
        if(! hero.grabbed())
        {
            value.cooldown = params.recover;
            _set_state(value, zombie_state::RECOVER, params.recover);
            break;
        }

        // Hold on next to the player.
        value.velocity = bn::fixed_point();

        if(decide)
        {
            value.facing = normalized(feet - value.position);
        }

        value.position = feet - (value.facing * grab_offset_px);
        break;

    case zombie_state::STAGGER:
        value.velocity = bn::fixed_point();

        if(value.timer <= 0)
        {
            value.target = feet;
            _set_state(value, zombie_state::CHASE, 0);
            value.facing = normalized(value.target - value.position);
            value.velocity = value.facing * value.speed;
        }
        break;

    case zombie_state::DOWN:
        value.velocity = bn::fixed_point();

        if(value.timer <= 0)
        {
            value.crawler = true;
            value.target = feet;
            _set_state(value, zombie_state::CRAWL, 0);
        }
        break;

    case zombie_state::CRAWL:
    {
        if(decide && ! hero.dead())
        {
            value.target = feet;
            value.facing = normalized(value.target - value.position);
            value.velocity = value.facing * params.crawl_speed;
        }

        // Crawlers grab ankles if the player lingers next to them. Next to the player they stop (crawling onto
        // the feet would leave no direction to face), and they only take hold if nobody else has.
        if(! hero.dead() && approx_length(feet - value.position) <= params.grab_range + 2)
        {
            value.velocity = bn::fixed_point();

            if(value.cooldown <= 0 && ! hero.grabbed() && ++value.reach_frames >= params.crawl_grab_delay &&
                    hero.try_grab(owner))
            {
                value.reach_frames = 0;
                value.cooldown = params.recover * 3;
            }
        }
        else
        {
            value.reach_frames = 0;
        }
        break;
    }

    case zombie_state::DYING:
    default:
        value.velocity = bn::fixed_point();
        break;
    }
}

void zombie_manager::_hit(game& owner, zombie& value, const attack_info& info, bool kill, bool knockdown)
{
    bool was_lying = lying(value.state);
    bn::fixed_point away = normalized(value.position - info.origin);

    if(away == bn::fixed_point())
    {
        away = info.direction;
    }

    // Zombies on the ground barely slide.
    value.knock = away * (was_lying ? info.knockback / 4 : info.knockback);
    value.velocity = bn::fixed_point();
    value.flash = tuning(combat_param::FLASH_FRAMES).integer();

    if(value.flash > 0)
    {
        value.sprite.set_palette(_flash_palette);
    }

    if(info.shove)
    {
        _set_state(value, zombie_state::STAGGER, info.stagger_frames);
        return;
    }

    value.facing = bn::fixed_point() - away;
    owner.fx().spawn(fx_kind::IMPACT, value.position - bn::fixed_point(0, 8));
    owner.fx().spawn(fx_kind::BLOOD, value.position - bn::fixed_point(0, 5) + (away * 6));

    if(owner.random().get_int(2) == 0)
    {
        owner.add_decal(value.position + (away * 10), owner.random().get_int(2) ? world_decal::BLOOD_A :
                                                                                  world_decal::BLOOD_B);
    }

    if(kill)
    {
        _kill(owner, value);
    }
    else if(_choke_gap <= 0)
    {
        // Playtest 2: hurt and still standing, it chokes.
        _voice(owner, value, sfx::id::CHOKE, zombie_sound_tuning::choke_volume);
        _choke_gap = zombie_sound_tuning::choke_gap_frames;
    }
    else if(was_lying)
    {
        // A floor hit pins a crawler down again for a moment.
        _set_state(value, zombie_state::DOWN, zombie_param_int(zombie_param::DOWN_FRAMES) / 2);
    }
    else if(knockdown)
    {
        ++_total_knockdowns;
        _set_state(value, zombie_state::DOWN, zombie_param_int(zombie_param::DOWN_FRAMES));
    }
    else
    {
        _set_state(value, zombie_state::STAGGER, info.stagger_frames);
    }
}

namespace
{
    // The outfit a zombie corpse's loot table stands for, or -1 (a survivor's body, a soldier from an event).
    [[nodiscard]] int corpse_outfit(loot_table table)
    {
        for(int index = 0; index < int(zombie_outfit::COUNT); ++index)
        {
            if(outfit_loot[index] == table)
            {
                return index;
            }
        }

        return -1;
    }
}

void zombie_manager::_show_corpse(game& owner, int tile_x, int tile_y, int outfit, int body)
{
    for(const corpse_sprite& value : _corpses)
    {
        if(value.tile_x == tile_x && value.tile_y == tile_y)
        {
            return;
        }
    }

    // Full: the corpse farthest from the player gives its sprite up (it keeps its decal).
    if(_corpses.full())
    {
        const bn::fixed_point& feet = owner.hero().position();
        int farthest = -1;
        int farthest_distance = -1;

        for(int index = 0, limit = _corpses.size(); index < limit; ++index)
        {
            int dx = ((_corpses[index].tile_x << world::tile_shift) + 8) - feet.x().integer();
            int dy = ((_corpses[index].tile_y << world::tile_shift) + 10) - feet.y().integer();
            int distance = (dx * dx) + (dy * dy);

            if(distance > farthest_distance)
            {
                farthest_distance = distance;
                farthest = index;
            }
        }

        _corpses.erase(_corpses.begin() + farthest);
    }

    // Where the dying body eased to (8 px across the tile, the feet 10 px down) and drawn as lying bodies are.
    int frame = (body * sprite_frames::zombie_body_frames) + sprite_frames::zombie_dead +
                sprite_frames::zombie_dead_frames - 1;      // in its full pool of blood
    bn::sprite_ptr sprite = bn::sprite_items::zombie.create_sprite(
                (tile_x << world::tile_shift) + 8, (tile_y << world::tile_shift) + 10 - lying_feet_offset, frame);
    sprite.set_camera(_camera);
    sprite.set_bg_priority(2);
    sprite.set_z_order(corpse_z_order);
    sprite.set_palette(_outfit_palettes[outfit]);
    sprite.set_horizontal_flip(world::decal_mirrored(tile_x, tile_y));
    light::shade(sprite);
    _corpses.push_back(corpse_sprite{ bn::move(sprite), int16_t(tile_x), int16_t(tile_y), true });

    #if KX_BENCH
        int index = owner.containers().at(tile_x, tile_y);
        BN_LOG("KXCORPSE sprite tile=", tile_x, ",", tile_y, " outfit=", outfit, " body=", body, " record body=",
               index >= 0 ? int(owner.containers().record(index).body) : -1, " frame=", frame, " sprites=",
               _corpses.size());
    #endif
}

void zombie_manager::_update_corpses(game& owner)
{
    // One pass over the containers: the zombie corpses in view keep their sprites or get one; sprites whose corpse
    // went out of view (or is gone) are given back.
    for(corpse_sprite& value : _corpses)
    {
        value.seen = false;
        light::shade(value.sprite);
    }

    const container_store& store = owner.containers();

    for(int index = 0, limit = store.count(); index < limit; ++index)
    {
        const container_record& record = store.record(index);

        if(record.kind != container_kind::CORPSE)
        {
            continue;
        }

        int outfit = corpse_outfit(record.table);
        bn::fixed_point centre((record.tile_x << world::tile_shift) + 8, (record.tile_y << world::tile_shift) + 8);

        if(outfit < 0 || bn::abs(owner.screen_x(centre)) > corpse_view_px ||
           bn::abs(owner.screen_y(centre)) > corpse_view_y_px)
        {
            continue;
        }

        bool shown = false;

        for(corpse_sprite& value : _corpses)
        {
            if(value.tile_x == record.tile_x && value.tile_y == record.tile_y)
            {
                value.seen = true;
                shown = true;
                break;
            }
        }

        if(! shown && ! _corpses.full())
        {
            _show_corpse(owner, record.tile_x, record.tile_y, outfit, record.body);
        }
    }

    for(auto it = _corpses.begin(); it != _corpses.end();)
    {
        it = it->seen ? it + 1 : _corpses.erase(it);
    }
}

void zombie_manager::_voice(game& owner, const zombie& value, sfx::id sound, bn::fixed volume) const
{
    int screen_x = owner.screen_x(value.position);
    int screen_y = owner.screen_y(value.position);
    int reach = (zombie_sound_tuning::heard_reach_px * sfx::hearing_percent()) / 100;

    if(bn::abs(screen_x) < reach && bn::abs(screen_y) < reach)
    {
        sfx::play(sound, volume, screen_x);
    }
}

void zombie_manager::_update_voices(game& owner)
{
    if(_snarl_gap > 0)
    {
        --_snarl_gap;
    }

    if(_choke_gap > 0)
    {
        --_choke_gap;
    }

    // A moan every 8-15 s from one zombie in earshot (all of them on their own clocks would drown everything else in
    // the 6 sound channels).
    if(--_moan_timer <= 0)
    {
        _moan_timer = zombie_sound_tuning::moan_min_frames +
                      _voice_random.get_int(zombie_sound_tuning::moan_extra_frames + 1);
        int reach = (zombie_sound_tuning::moan_reach_px * sfx::hearing_percent()) / 100;
        int heard = 0;

        for(const zombie& value : _zombies)
        {
            heard += ! lying(value.state) && bn::abs(owner.screen_x(value.position)) < reach &&
                     bn::abs(owner.screen_y(value.position)) < reach;
        }

        if(heard)
        {
            int pick = _voice_random.get_int(heard);

            for(const zombie& value : _zombies)
            {
                if(! lying(value.state) && bn::abs(owner.screen_x(value.position)) < reach &&
                   bn::abs(owner.screen_y(value.position)) < reach && pick-- == 0)
                {
                    sfx::play(sfx::id::GROAN, zombie_sound_tuning::moan_volume, owner.screen_x(value.position));
                    break;
                }
            }
        }
    }

    // Every 4-7 s the nearest zombie that hasn't noticed you, if it is close, is heard breathing.
    if(--_breath_timer <= 0)
    {
        _breath_timer = zombie_sound_tuning::breath_min_frames +
                        _voice_random.get_int(zombie_sound_tuning::breath_extra_frames + 1);
        const bn::fixed_point& feet = owner.hero().position();
        int reach = (zombie_sound_tuning::breath_reach_px * sfx::hearing_percent()) / 100;
        int nearest = (reach * reach) + 1;
        const zombie* heard = nullptr;

        for(const zombie& value : _zombies)
        {
            if(aware(value.state) || lying(value.state))
            {
                continue;
            }

            int dx = (value.position.x() - feet.x()).integer();
            int dy = (value.position.y() - feet.y()).integer();
            int distance = (dx * dx) + (dy * dy);

            if(bn::abs(dx) <= reach && bn::abs(dy) <= reach && distance < nearest)
            {
                nearest = distance;
                heard = &value;
            }
        }

        if(heard)
        {
            sfx::play(sfx::id::BREATH, zombie_sound_tuning::breath_volume, owner.screen_x(heard->position));
        }
    }
}

void zombie_manager::_kill(game& owner, zombie& value)
{
    // Playtest 2: its death groan.
    _voice(owner, value, sfx::id::DEATH_GROAN, zombie_sound_tuning::death_volume);
    value.health = 0;
    _set_state(value, zombie_state::DYING, dying_frames);
    owner.add_kill();
}

void zombie_manager::_set_state(zombie& value, zombie_state state, int timer)
{
    value.state = state;
    value.timer = timer;
    value.reach_frames = 0;
}

}
