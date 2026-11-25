#include "kx_animals.h"

#include "bn_display.h"
#include "bn_log.h"
#include "bn_sprite_items_animals.h"
#include "bn_sprite_items_animals_wide.h"
#include "bn_sprite_items_birds.h"

#include "kx_animal_frames.h"
#include "kx_base.h"
#include "kx_containers.h"
#include "kx_fx.h"
#include "kx_game.h"
#include "kx_hash.h"
#include "kx_hud_tiles.h"
#include "kx_light.h"
#include "kx_macro_map.h"
#include "kx_save.h"
#include "kx_sfx.h"
#include "kx_survival.h"
#include "kx_vector.h"
#include "kx_world.h"
#include "kx_zombies.h"

namespace kx
{

using namespace animal_tuning;

namespace
{
    constexpr int sprite_bg_priority = 2;       // with the characters
    constexpr int sprite_feet_offset = 8;
    constexpr int flying_z = -24;               // birds in the air: in front of everything on the ground
    constexpr int fx_body_offset = 6;
    constexpr uint8_t farm_marker = 0x46;       // starts the farms' part of the save (stage 4)

    [[nodiscard]] int tile_of(bn::fixed value)
    {
        return value.floor_integer() >> world::tile_shift;
    }

    [[nodiscard]] int feet_tile_y(const bn::fixed_point& feet)
    {
        return (feet.y().floor_integer() - 1) >> world::tile_shift;
    }

    [[nodiscard]] bool box_free(const bn::fixed_point& feet)
    {
        int x = feet.x().floor_integer();
        int y = feet.y().floor_integer();
        return ! world::box_blocked(x - 4, y - 4, x + 3, y - 1);
    }

    [[nodiscard]] bool open_ground(terrain ground)
    {
        return ground == terrain::GRASS || ground == terrain::FOREST || ground == terrain::CROPS ||
               ground == terrain::SOIL || ground == terrain::MUD;
    }

    [[nodiscard]] bool bird_ground(terrain ground)
    {
        return ground == terrain::GRASS || ground == terrain::ROAD || ground == terrain::CROPS ||
               ground == terrain::SOIL || ground == terrain::PAVEMENT;
    }

    // The deer and the boar are 32x16 sprites (animals_wide.bmp), the rabbit 16x16 (animals.bmp).
    [[nodiscard]] const bn::sprite_item& sprite_item_of(animal_kind kind)
    {
        return kind == animal_kind::RABBIT || kind == animal_kind::HEN ? bn::sprite_items::animals :
                                                                         bn::sprite_items::animals_wide;
    }

    [[nodiscard]] [[maybe_unused]] const char* kind_name(animal_kind kind)
    {
        return kind == animal_kind::DEER ? "deer" : kind == animal_kind::BOAR ? "boar" :
               kind == animal_kind::WOLF ? "wolf" : kind == animal_kind::HEN ? "hen" :
               kind == animal_kind::GOAT ? "goat" : "rabbit";
    }

    // A rough unit vector at an angle from 0-255 (a quarter turn is 64).
    [[nodiscard]] bn::fixed_point heading(int angle)
    {
        int degrees = ((angle & 255) * 360) / 256;
        return bn::fixed_point(bn::degrees_lut_cos(degrees), bn::degrees_lut_sin(degrees));
    }

    // A wolf's place on its ring: orbit counts 1024 to a turn (a whole degree every 2.8, so the point glides).
    [[nodiscard]] bn::fixed_point ring_heading(int orbit)
    {
        int degrees = ((orbit & 1023) * 360) / 1024;
        return bn::fixed_point(bn::degrees_lut_cos(degrees), bn::degrees_lut_sin(degrees));
    }
}

animal_manager::animal_manager(const bn::camera_ptr& camera) :
    _camera(camera)
{
    reset_kills();
}

void animal_manager::clear()
{
    _animals.clear();
    _birds.clear();
    _flying = false;
    _crows = false;
    _landing = 0;
    _fly_timer = 0;
    _bird_cooldown = 0;
    _pack_hunting = false;
    _pack_fear = 0;
    _dart_cooldown = 0;
    _kept_off = false;
    _pending_farm = -1;
}

void animal_manager::reset_kills()
{
    for(kill& value : _kills)
    {
        value = kill();
    }

    _next_kill = 0;

    for(tally& value : _tallies)
    {
        value = tally();
    }

    _next_tally = 0;

    for(known_farm& value : _farms)
    {
        value = known_farm();
    }

    _next_farm = 0;

    for(uint16_t& cell : _no_yard)
    {
        cell = 0xFFFF;
    }

    _next_no_yard = 0;
}

int animal_manager::kills_listed() const
{
    int result = 0;

    for(const kill& value : _kills)
    {
        result += value.day >= 0;
    }

    return result;
}

// ---------------------------------------------------------------------------------------------------------------------
// Spawning
// ---------------------------------------------------------------------------------------------------------------------

bool animal_manager::_deer_cell(game& owner, int tile_x, int tile_y) const
{
    int cell_x = tile_x >> macro::cell_shift;
    int cell_y = tile_y >> macro::cell_shift;
    macro::zone zone = macro::zone_at(cell_x, cell_y);
    macro::land land = macro::land_at(cell_x, cell_y);

    if((zone != macro::zone::WILDERNESS && zone != macro::zone::FARM) || land == macro::land::WATER ||
       land == macro::land::COAST)
    {
        return false;
    }

    // Hunted this week: none.
    for(const kill& value : _kills)
    {
        if(value.cell_x == cell_x && value.cell_y == cell_y && owner.day() - value.day < deer_return_days)
        {
            return false;
        }
    }

    // From the seed and the day: some cells have deer today.
    uint32_t roll = hash32(world::seed() ^ hash_xy(0x44454552u, cell_x, cell_y) ^ (uint32_t(owner.day()) * 0x9e3779b9u));
    return int(roll % 100) < deer_cell_percent;
}

bool animal_manager::_boar_cell(game& owner, int tile_x, int tile_y) const
{
    int cell_x = tile_x >> macro::cell_shift;
    int cell_y = tile_y >> macro::cell_shift;

    if(macro::zone_at(cell_x, cell_y) != macro::zone::WILDERNESS || world::terrain_at(tile_x, tile_y) != terrain::FOREST)
    {
        return false;
    }

    uint32_t roll = hash32(world::seed() ^ hash_xy(0x424f4152u, cell_x, cell_y) ^ (uint32_t(owner.day()) * 0x85ebca6bu));
    return int(roll % 100) < boar_cell_percent;
}

bool animal_manager::_wolf_cell(game& owner, int tile_x, int tile_y) const
{
    int cell_x = tile_x >> macro::cell_shift;
    int cell_y = tile_y >> macro::cell_shift;
    macro::land land = macro::land_at(cell_x, cell_y);

    if(macro::zone_at(cell_x, cell_y) != macro::zone::WILDERNESS || land == macro::land::WATER ||
       land == macro::land::COAST)
    {
        return false;
    }

    uint32_t roll = hash32(world::seed() ^ hash_xy(0x574f4c46u, cell_x, cell_y) ^ (uint32_t(owner.day()) * 0xc2b2ae35u));
    return int(roll % 100) < wolf_cell_percent;
}

// ---------------------------------------------------------------------------------------------------------------------
// Calls (stage 6)
// ---------------------------------------------------------------------------------------------------------------------

void animal_manager::_call(game& owner, const bn::fixed_point& at, sfx::id sound, bn::fixed volume, int reach_px) const
{
    int screen_x = owner.screen_x(at);
    int screen_y = owner.screen_y(at);
    int reach = (reach_px * sfx::hearing_percent()) / 100;

    if(bn::abs(screen_x) >= reach || bn::abs(screen_y) >= reach)
    {
        return;
    }

    bool seen = bn::abs(screen_x) <= screen_half_x && bn::abs(screen_y) <= screen_half_y;
    sfx::play(sound, seen ? volume : volume / 2, screen_x);
}

void animal_manager::_update_calls(game& owner)
{
    bn::random& random = _random;
    const bn::fixed_point& feet = owner.hero().position();

    // The nearest of a kind, alive and calm, or -1.
    auto nearest = [this, &feet](animal_kind kind)
    {
        int best = -1;
        bn::fixed best_distance = 32767;

        for(int index = 0; index < _animals.size(); ++index)
        {
            const animal& value = _animals[index];

            if(value.kind == kind && value.current != state::DEAD && value.current != state::FLEE)
            {
                bn::fixed distance = approx_length(value.position - feet);

                if(distance < best_distance)
                {
                    best = index;
                    best_distance = distance;
                }
            }
        }

        return best;
    };

    if(--_grunt_timer <= 0)
    {
        _grunt_timer = grunt_frames + random.get_int(grunt_frames);
        int index = nearest(animal_kind::BOAR);

        if(index >= 0)
        {
            _call(owner, _animals[index].position, sfx::id::BOAR_GRUNT, grunt_volume, call_reach_px);
        }
    }

    if(--_howl_timer <= 0)
    {
        _howl_timer = howl_frames + random.get_int(howl_frames * 4 / 5);
        int index = nearest(animal_kind::WOLF);

        if(index >= 0 && ! _pack_hunting && is_night(owner.minute_of_day()))
        {
            _call(owner, _animals[index].position, sfx::id::WOLF_HOWL, howl_volume, howl_reach_px);
        }
    }

    if(--_cluck_timer <= 0)
    {
        _cluck_timer = cluck_frames + random.get_int(cluck_frames * 6 / 5);
        int index = nearest(animal_kind::HEN);

        if(index >= 0)
        {
            _call(owner, _animals[index].position, sfx::id::HEN_CLUCK, cluck_volume, call_reach_px);
        }
    }

    if(--_bleat_timer <= 0)
    {
        _bleat_timer = bleat_frames + random.get_int(bleat_frames);
        int index = nearest(animal_kind::GOAT);

        if(index >= 0)
        {
            _call(owner, _animals[index].position, sfx::id::GOAT_BLEAT, bleat_volume, call_reach_px);
        }
    }

    if(--_caw_timer <= 0)
    {
        _caw_timer = caw_frames + random.get_int(caw_frames);

        if(_crows && ! _flying && _landing == 0 && ! _birds.empty())
        {
            _call(owner, _carrion, sfx::id::CROW_CAW, caw_volume / 2, call_reach_px);
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Farm animals
// ---------------------------------------------------------------------------------------------------------------------

int animal_manager::_known_farm(int cell_x, int cell_y) const
{
    for(int index = 0; index < max_known_farms; ++index)
    {
        if(_farms[index].cell_x == cell_x && _farms[index].cell_y == cell_y)
        {
            return index;
        }
    }

    return -1;
}

animal_manager::tally* animal_manager::_tally(int cell_x, int cell_y, bool create)
{
    for(tally& value : _tallies)
    {
        if(value.cell_x == cell_x && value.cell_y == cell_y)
        {
            return &value;
        }
    }

    if(! create)
    {
        return nullptr;
    }

    // The oldest farm's record makes way (that farm is restocked).
    tally& value = _tallies[_next_tally];
    _next_tally = (_next_tally + 1) % max_tallies;
    value = tally();
    value.cell_x = int8_t(cell_x);
    value.cell_y = int8_t(cell_y);
    return &value;
}

int animal_manager::farm_lost(int cell_x, int cell_y, bool goats) const
{
    for(const tally& value : _tallies)
    {
        if(value.cell_x == cell_x && value.cell_y == cell_y)
        {
            return goats ? value.goats_lost : value.hens_lost;
        }
    }

    return 0;
}

void animal_manager::_farm_loss(const animal& value)
{
    if(value.farm < 0)
    {
        return;
    }

    const known_farm& farm = _farms[value.farm];
    tally* record = _tally(farm.cell_x, farm.cell_y, true);

    if(value.kind == animal_kind::GOAT)
    {
        record->goats_lost = uint8_t(bn::min(record->goats_lost + 1, 255));
    }
    else
    {
        record->hens_lost = uint8_t(bn::min(record->hens_lost + 1, 255));
    }
}

void animal_manager::_spawn_farm(game& owner)
{
    if(world::test_world() || owner.hero().dead())
    {
        return;
    }

    // The farms in the survivor's cell and those round it, from the seed (a few kept for the session).
    const bn::fixed_point& feet = owner.hero().position();
    int tile_x = tile_of(feet.x());
    int tile_y = feet_tile_y(feet);
    int centre_x = tile_x >> macro::cell_shift;
    int centre_y = tile_y >> macro::cell_shift;

    // Stage 7: finding a yard costs (up to a quarter of a frame for nine cells at once), so a tick looks up at most
    // one new farm cell, the survivor's own first, and cells found to have none are remembered.
    bool looked_up = false;

    for(int step = 0; step < 9; ++step)
    {
        int around = step == 0 ? 4 : step <= 4 ? step - 1 : step;    // 4 is the survivor's own cell
        int cell_x = centre_x + (around % 3) - 1;
        int cell_y = centre_y + (around / 3) - 1;

        {
            if(cell_x < 0 || cell_y < 0 || cell_x >= macro::cells || cell_y >= macro::cells ||
               macro::zone_at(cell_x, cell_y) != macro::zone::FARM)
            {
                continue;
            }

            int farm = _known_farm(cell_x, cell_y);

            if(farm < 0)
            {
                int yard_x;
                int yard_y;
                uint16_t key = uint16_t((cell_y << 8) | cell_x);
                bool barren = false;

                for(uint16_t cell : _no_yard)
                {
                    barren |= cell == key;
                }

                if(barren || looked_up)
                {
                    continue;
                }

                looked_up = true;

                if(! world::farm_yard(cell_x, cell_y, yard_x, yard_y))
                {
                    _no_yard[_next_no_yard] = key;
                    _next_no_yard = (_next_no_yard + 1) % max_no_yard;
                    continue;
                }

                uint32_t roll = hash32(world::seed() ^ hash_xy(0x4641524du, cell_x, cell_y));
                known_farm& value = _farms[_next_farm];
                farm = _next_farm;
                _next_farm = (_next_farm + 1) % max_known_farms;

                // Its animals go with the old entry.
                for(int index = 0; index < _animals.size(); ++index)
                {
                    if(_animals[index].farm == farm)
                    {
                        _animals.erase(_animals.begin() + index);
                        --index;
                    }
                }

                value.cell_x = int8_t(cell_x);
                value.cell_y = int8_t(cell_y);
                value.yard_x = int16_t(yard_x);
                value.yard_y = int16_t(yard_y);
                bool stocked = int(roll % 100) < farm_animal_percent;
                value.hens = stocked ? uint8_t(least_hens + ((roll >> 8) % (most_hens - least_hens + 1))) : 0;
                value.goats = stocked ? uint8_t(least_goats + ((roll >> 12) % (most_goats - least_goats + 1))) : 0;

                // That lookup is this tick's work; its animals come out from the next.
                continue;
            }

            const known_farm& value = _farms[farm];

            if(bn::abs(value.yard_x - tile_x) > farm_spawn_tiles || bn::abs(value.yard_y - tile_y) > farm_spawn_tiles)
            {
                continue;
            }

            // The farm is topped up to what it has left (hens only by day): noted here, they come out one a frame
            // (update), as each new sprite costs (stage 7).
            if(_pending_farm < 0)
            {
                _pending_farm = farm;
            }
        }
    }
}

bool animal_manager::_bring_out_next(game& owner)
{
    // A farm's animals, one a frame until it has what it has left (or there's no room).
    int hens_out = 0;
    int goats_out = 0;

    for(const animal& other : _animals)
    {
        hens_out += other.farm == _pending_farm && other.kind == animal_kind::HEN;
        goats_out += other.farm == _pending_farm && other.kind == animal_kind::GOAT;
    }

    if(! _bring_out(owner, _pending_farm, hens_out, goats_out, 1))
    {
        _pending_farm = -1;
        return false;
    }

    return true;
}

bool animal_manager::_bring_out(game& owner, int farm, int hens_out, int goats_out, int most)
{
    // What it has left, less what is out already (a dead one lying there counts as out: it is in the losses too).
    const known_farm& value = _farms[farm];
    bool day = ! is_night(owner.minute_of_day());
    int hens = day ? bn::max(int(value.hens) - farm_lost(value.cell_x, value.cell_y, false) - hens_out, 0) : 0;
    int goats = bn::max(int(value.goats) - farm_lost(value.cell_x, value.cell_y, true) - goats_out, 0);
    hens = bn::min(hens, most);
    goats = bn::min(goats, most - hens);

    if(hens + goats == 0 || _animals.full())
    {
        return false;
    }

    bn::fixed_point door((value.yard_x * world::tile_size) + 8, (value.yard_y * world::tile_size) + 12);
    int placed_hens = 0;
    int placed_goats = 0;

    // Hens first, then goats, while there is room; each on open ground round the barn door.
    for(int index = 0; index < hens + goats && ! _animals.full(); ++index)
    {
        bool goat = index >= hens;
        int spread = (goat ? goat_spread_tiles : hen_spread_tiles) * world::tile_size;

        for(int attempt = 0; attempt < 6; ++attempt)
        {
            bn::fixed_point at = door + bn::fixed_point(_random.get_int((spread * 2) + 1) - spread,
                                                        _random.get_int(spread + 1) + 8);
            int x = tile_of(at.x());
            int y = feet_tile_y(at);

            // (Indoors the ground is floor, which open_ground refuses: no need for the costly under_roof.)
            if(! world::loaded(x, y) || world::solid(x, y) || ! box_free(at) || ! open_ground(world::terrain_at(x, y)))
            {
                continue;
            }

            int id = spawn(owner, goat ? animal_kind::GOAT : animal_kind::HEN, at);

            if(id >= 0)
            {
                animal& placed = _animals.back();
                placed.home = door;
                placed.farm = int8_t(farm);
                placed.slot = uint8_t(goat ? goats_out + placed_goats : hens_out + placed_hens);
                placed_goats += goat;
                placed_hens += ! goat;
            }

            break;
        }
    }

    #if KX_BENCH == 15 || KX_BENCH == 14
        BN_LOG("KXFARM farm ", value.cell_x, ",", value.cell_y, " brings out ", placed_hens, " hens, ", placed_goats,
               " goats (stock ", value.hens, "/", value.goats, ", lost ", farm_lost(value.cell_x, value.cell_y, false),
               "/", farm_lost(value.cell_x, value.cell_y, true), ")");
    #endif

    return placed_hens + placed_goats > 0;
}

void animal_manager::bench_farm(game& owner, int cell_x, int cell_y, int yard_x, int yard_y, int hens, int goats)
{
    int farm = _known_farm(cell_x, cell_y);

    if(farm < 0)
    {
        farm = _next_farm;
        _next_farm = (_next_farm + 1) % max_known_farms;
    }

    known_farm& value = _farms[farm];
    value.cell_x = int8_t(cell_x);
    value.cell_y = int8_t(cell_y);
    value.yard_x = int16_t(yard_x);
    value.yard_y = int16_t(yard_y);
    value.hens = uint8_t(hens);
    value.goats = uint8_t(goats);
    _bring_out(owner, farm, 0, 0, max_animals);
}

int animal_manager::hen_at(const bn::fixed_point& point, int reach) const
{
    for(const animal& value : _animals)
    {
        if(value.kind == animal_kind::HEN && value.current != state::DEAD &&
           approx_length(value.position - point) <= reach)
        {
            return value.id;
        }
    }

    return -1;
}

int animal_manager::goat_to_milk(game& owner, const bn::fixed_point& point, int reach) const
{
    for(const animal& value : _animals)
    {
        if(value.kind != animal_kind::GOAT || value.current == state::DEAD || value.current == state::FLEE ||
           approx_length(value.position - point) > reach)
        {
            continue;
        }

        if(value.farm >= 0)
        {
            const known_farm& farm = _farms[value.farm];

            for(const tally& record : _tallies)
            {
                if(record.cell_x == farm.cell_x && record.cell_y == farm.cell_y &&
                   record.milked[value.slot & 1] == owner.day())
                {
                    return -1;
                }
            }
        }

        return value.id;
    }

    return -1;
}

bool animal_manager::catch_hen(game& owner, int id)
{
    int index = _index_of(id);

    if(index < 0 || _animals[index].kind != animal_kind::HEN || _animals[index].current == state::DEAD)
    {
        return false;
    }

    _farm_loss(_animals[index]);
    _animals.erase(_animals.begin() + index);
    item_stack meat = make_stack(item_id::RAW_MEAT);
    meat.extra = uint16_t(owner.day());

    if(! owner.hero().items().add(meat))
    {
        static_cast<void>(owner.drop_item(meat));
    }

    sfx::play(sfx::id::FLUTTER, milli(600));
    owner.toast("CAUGHT A HEN: RAW MEAT", hud_tiles::bank_accent, 90);

    #if KX_BENCH == 15
        BN_LOG("KXFARM caught a hen");
    #endif

    return true;
}

bool animal_manager::milk_goat(game& owner, int id)
{
    int index = _index_of(id);
    inventory& items = owner.hero().items();
    int bottle = items.find(item_id::EMPTY_BOTTLE);

    if(index < 0 || bottle < 0 || goat_to_milk(owner, _animals[index].position, 0) != id)
    {
        return false;
    }

    animal& value = _animals[index];

    if(value.farm >= 0)
    {
        const known_farm& farm = _farms[value.farm];
        _tally(farm.cell_x, farm.cell_y, true)->milked[value.slot & 1] = int16_t(owner.day());
    }

    items.consume_one(bottle);
    item_stack milk = make_stack(item_id::MILK);
    milk.extra = uint16_t(owner.day());

    if(! items.add(milk))
    {
        static_cast<void>(owner.drop_item(milk));
    }

    // It stands for it, then wanders off.
    value.current = state::LOOK;
    value.velocity = bn::fixed_point();
    value.timer = 90;
    sfx::play(sfx::id::PICKUP, milli(700));
    owner.toast("A BOTTLE OF MILK", hud_tiles::bank_accent, 90);

    #if KX_BENCH == 15
        BN_LOG("KXFARM milked goat ", value.slot, " on day ", owner.day());
    #endif

    return true;
}

int animal_manager::eggs_near(int tile_x, int tile_y) const
{
    for(const known_farm& value : _farms)
    {
        if(value.cell_x < 0 || bn::abs(value.yard_x - tile_x) > egg_tiles || bn::abs(value.yard_y - tile_y) > egg_tiles)
        {
            continue;
        }

        int hens = int(value.hens) - farm_lost(value.cell_x, value.cell_y, false);
        return hens <= 0 ? 0 : hens >= 3 ? 2 : 1;
    }

    return 0;
}

bool animal_manager::_ring_spot(game& owner, int near, int far, bool deer, int& tile_x, int& tile_y) const
{
    const bn::fixed_point& feet = owner.hero().position();
    int centre_x = tile_of(feet.x());
    int centre_y = feet_tile_y(feet);
    bn::random& random = _random;

    for(int attempt = 0; attempt < 4; ++attempt)
    {
        bn::fixed_point direction = heading(random.get_int(256));
        int distance = near + random.get_int(far - near + 1);
        int x = centre_x + (direction.x() * distance).round_integer();
        int y = centre_y + (direction.y() * distance).round_integer();

        // Inside buildings the ground is floor, which neither list allows.
        if(! world::loaded(x, y) || world::solid(x, y))
        {
            continue;
        }

        terrain ground = world::terrain_at(x, y);

        if(deer ? open_ground(ground) : bird_ground(ground))
        {
            tile_x = x;
            tile_y = y;
            return true;
        }
    }

    return false;
}

void animal_manager::_spawn_tries(game& owner)
{
    // Benches play the test world: animals there only come from their scripts.
    if(world::test_world() || owner.hero().dead())
    {
        return;
    }

    bn::random& random = _random;
    bool day = ! is_night(owner.minute_of_day());
    int deer = 0;
    int rabbits = 0;
    int boars = 0;
    int wolves = 0;
    int free = max_animals - _animals.size();

    for(const animal& value : _animals)
    {
        deer += value.kind == animal_kind::DEER;
        boars += value.kind == animal_kind::BOAR;
        wolves += value.kind == animal_kind::WOLF;
        rabbits += value.kind == animal_kind::RABBIT && value.current != state::DEAD;
    }

    int tile_x;
    int tile_y;

    if(! day && wolves == 0 && free >= least_wolves &&
       _ring_spot(owner, wolf_spawn_near, wolf_spawn_far, true, tile_x, tile_y) && _wolf_cell(owner, tile_x, tile_y))
    {
        int count = bn::min(least_wolves + random.get_int(max_wolves - least_wolves + 1), free);
        static_cast<void>(spawn_pack(owner, bn::fixed_point((tile_x * world::tile_size) + 8,
                                                            (tile_y * world::tile_size) + 12), count));
    }
    else if(deer < max_deer && ! _animals.full() &&
            _ring_spot(owner, deer_spawn_near, deer_spawn_far, true, tile_x, tile_y) &&
       _deer_cell(owner, tile_x, tile_y))
    {
        static_cast<void>(spawn(owner, animal_kind::DEER, bn::fixed_point((tile_x * world::tile_size) + 8,
                                                                          (tile_y * world::tile_size) + 12)));
    }
    else if(boars < max_boar && ! _animals.full() &&
            _ring_spot(owner, boar_spawn_near, boar_spawn_far, true, tile_x, tile_y) && _boar_cell(owner, tile_x, tile_y))
    {
        static_cast<void>(spawn(owner, animal_kind::BOAR, bn::fixed_point((tile_x * world::tile_size) + 8,
                                                                          (tile_y * world::tile_size) + 12)));
    }
    else if(day && rabbits == 0 && ! _animals.full() && random.get_int(100) < rabbit_percent &&
            _ring_spot(owner, rabbit_spawn_near, rabbit_spawn_far, true, tile_x, tile_y))
    {
        macro::zone zone = macro::zone_at(tile_x >> macro::cell_shift, tile_y >> macro::cell_shift);

        if(zone != macro::zone::TOWN && zone != macro::zone::MILITARY &&
           world::terrain_at(tile_x, tile_y) != terrain::MUD)
        {
            static_cast<void>(spawn(owner, animal_kind::RABBIT, bn::fixed_point((tile_x * world::tile_size) + 8,
                                                                                (tile_y * world::tile_size) + 12)));
        }
    }
    else if(bn::fixed_point corpse; day && _birds.empty() && _bird_cooldown <= 0 &&
            owner.zombies().count() < max_zombies_for_birds && random.get_int(100) < crow_percent &&
            _carrion_spot(owner, corpse))
    {
        static_cast<void>(spawn_crows(owner, corpse));
    }
    else if(day && _birds.empty() && _bird_cooldown <= 0 && owner.zombies().count() < max_zombies_for_birds &&
            random.get_int(100) < bird_percent &&
            _ring_spot(owner, bird_spawn_near, bird_spawn_far, false, tile_x, tile_y))
    {
        spawn_flock(owner, bn::fixed_point((tile_x * world::tile_size) + 8, (tile_y * world::tile_size) + 8));
    }
}

int animal_manager::spawn(game& owner, animal_kind kind, const bn::fixed_point& feet)
{
    static_cast<void>(owner);

    if(_animals.full())
    {
        return -1;
    }

    int first = kind == animal_kind::DEER ? animal_frames::deer_side_stand :
                kind == animal_kind::BOAR ? animal_frames::boar_side_stand :
                kind == animal_kind::WOLF ? animal_frames::wolf_side_stand :
                kind == animal_kind::HEN ? animal_frames::hen_side_stand :
                kind == animal_kind::GOAT ? animal_frames::goat_side_stand : animal_frames::rabbit_sit;
    bn::sprite_ptr sprite = sprite_item_of(kind).create_sprite(feet.x(), feet.y() - sprite_feet_offset, first);
    sprite.set_camera(_camera);
    sprite.set_bg_priority(sprite_bg_priority);
    light::shade(sprite);
    _animals.push_back(animal(bn::move(sprite)));
    animal& value = _animals.back();
    value.position = feet;
    value.kind = kind;
    value.health = kind == animal_kind::DEER ? deer_health : kind == animal_kind::BOAR ? boar_health :
                   kind == animal_kind::WOLF ? wolf_health : kind == animal_kind::HEN ? hen_health :
                   kind == animal_kind::GOAT ? goat_health : rabbit_health;
    value.current = kind == animal_kind::RABBIT || kind == animal_kind::WOLF ? state::LOOK : state::GRAZE;
    value.orbit = (int(_next_id) * 85 + _random.get_int(32)) << 2;
    value.clockwise = _random.get_int(2);

    #if KX_BENCH == 15
        BN_LOG("KXBOAR a ", kind_name(kind), " spawned at ", feet.x().integer(), ",", feet.y().integer());
    #endif
    value.timer = 60 + _random.get_int(120);
    value.left = _random.get_int(2);
    value.id = _next_id;
    _next_id = uint8_t(_next_id == 255 ? 1 : _next_id + 1);
    return value.id;
}

int animal_manager::spawn_pack(game& owner, const bn::fixed_point& centre, int count)
{
    int placed = 0;

    for(int index = 0; index < count && ! _animals.full(); ++index)
    {
        bn::fixed_point at = centre + bn::fixed_point(_random.get_int(13) - 6, _random.get_int(9) - 4);
        placed += spawn(owner, animal_kind::WOLF, at) >= 0;
    }

    #if KX_BENCH == 15
        BN_LOG("KXWOLF a pack of ", placed, " at ", centre.x().integer() >> 4, ",", centre.y().integer() >> 4);
    #endif

    if(placed > 0)
    {
        _call(owner, centre, sfx::id::WOLF_HOWL, howl_volume, howl_reach_px);
    }

    return placed;
}

bool animal_manager::_carrion_spot(game& owner, bn::fixed_point& corpse) const
{
    // The nearest zombie corpse in the ring round the survivor.
    const container_store& store = owner.containers();
    const bn::fixed_point& feet = owner.hero().position();
    int tile_x = tile_of(feet.x());
    int tile_y = feet_tile_y(feet);
    int best = crow_corpse_far + 1;

    for(int index = 0, limit = store.count(); index < limit; ++index)
    {
        const container_record& record = store.record(index);

        if(record.kind != container_kind::CORPSE)
        {
            continue;
        }

        int distance = bn::max(bn::abs(record.tile_x - tile_x), bn::abs(record.tile_y - tile_y));

        if(distance >= crow_corpse_near && distance < best)
        {
            best = distance;
            corpse = bn::fixed_point((record.tile_x * world::tile_size) + 8, (record.tile_y * world::tile_size) + 10);
        }
    }

    return best <= crow_corpse_far;
}

int animal_manager::spawn_crows(game& owner, const bn::fixed_point& corpse)
{
    static_cast<void>(owner);
    _birds.clear();
    _flying = false;
    _crows = true;
    _carrion = corpse;
    _landing = crow_landing_frames;
    int count = least_crows + _random.get_int(most_crows - least_crows + 1);

    // Each glides down to its spot round the corpse from above the screen.
    for(int index = 0; index < count && ! _birds.full(); ++index)
    {
        bn::fixed_point target = corpse + bn::fixed_point(_random.get_int((crow_spread_px * 2) + 1) - crow_spread_px,
                                                          _random.get_int(crow_spread_px + 1) - (crow_spread_px / 2));
        bn::fixed_point start = target + bn::fixed_point(_random.get_int(81) - 40, -100 - _random.get_int(30));
        bn::sprite_ptr sprite = bn::sprite_items::birds.create_sprite(start.x(), start.y() - 4,
                                                                      animal_frames::crow_wings_up);
        sprite.set_camera(_camera);
        sprite.set_bg_priority(sprite_bg_priority);
        light::shade(sprite);
        _birds.push_back(bird(bn::move(sprite)));
        bird& value = _birds.back();
        value.position = start;
        value.velocity = (target - start) / crow_landing_frames;
        value.left = value.velocity.x() < 0;
        value.timer = crow_idle_frames + _random.get_int(crow_idle_frames);
    }

    #if KX_BENCH == 15
        BN_LOG("KXCROW ", _birds.size(), " crows come down to the corpse at ", corpse.x().integer() >> 4, ",",
               corpse.y().integer() >> 4);
    #endif

    return _birds.size();
}

void animal_manager::spawn_flock(game& owner, const bn::fixed_point& centre)
{
    static_cast<void>(owner);
    _birds.clear();
    _flying = false;
    _crows = false;
    _landing = 0;
    int count = least_birds + _random.get_int(max_birds - least_birds + 1);

    for(int index = 0; index < count; ++index)
    {
        bn::fixed_point at = centre + bn::fixed_point(_random.get_int(25) - 12, _random.get_int(17) - 8);
        bn::sprite_ptr sprite = bn::sprite_items::birds.create_sprite(at.x(), at.y() - 4,
                                                                      animal_frames::bird_perched);
        sprite.set_camera(_camera);
        sprite.set_bg_priority(sprite_bg_priority);
        light::shade(sprite);
        _birds.push_back(bird(bn::move(sprite)));
        _birds.back().position = at;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Behaviour
// ---------------------------------------------------------------------------------------------------------------------

void animal_manager::update(game& owner)
{
    ++_frame;
    _bird_cooldown = bn::max(_bird_cooldown - 1, 0);

    if(_frame % spawn_interval == 0)
    {
        _spawn_farm(owner);
        _spawn_tries(owner);
    }
    else if(_pending_farm >= 0)
    {
        static_cast<void>(_bring_out_next(owner));
    }

    const bn::fixed_point& feet = owner.hero().position();
    bool night = is_night(owner.minute_of_day());
    _update_pack(owner);

    for(int index = 0; index < _animals.size(); ++index)
    {
        animal& value = _animals[index];
        bn::fixed_point offset = value.position - feet;
        int distance = approx_length(offset).integer();
        int limit = (value.current == state::DEAD ? carcass_despawn_tiles : despawn_tiles) * world::tile_size;

        // Gone far away (or, for rabbits, the night has come and nobody sees it go).
        bool hidden = bn::abs(offset.x()) > (bn::display::width() / 2) + 16 ||
                      bn::abs(offset.y()) > (bn::display::height() / 2) + 16;

        bool wolf_gone = value.kind == animal_kind::WOLF && value.current != state::DEAD && hidden &&
                         (! night || value.current == state::FLEE);

        bool day_animal = value.kind == animal_kind::RABBIT || value.kind == animal_kind::HEN;
        bool body_gone = value.kind == animal_kind::HEN && value.current == state::DEAD && --value.timer <= 0;

        if(distance > limit || wolf_gone || body_gone ||
           (night && hidden && day_animal && value.current != state::DEAD))
        {
            _animals.erase(_animals.begin() + index);
            --index;
            continue;
        }

        if(value.current != state::DEAD)
        {
            if(value.kind == animal_kind::WOLF)
            {
                _think_wolf(owner, value);
            }
            else
            {
                _think(owner, value);
            }

            _move(value);
        }
    }

    _update_flock(owner);
    _update_calls(owner);
}

void animal_manager::_think(game& owner, animal& value)
{
    const player& hero = owner.hero();
    bool deer = value.kind == animal_kind::DEER;
    bool boar = value.kind == animal_kind::BOAR;
    bool hen = value.kind == animal_kind::HEN;
    bool goat = value.kind == animal_kind::GOAT;
    bool grazer = value.kind != animal_kind::RABBIT;

    if(boar)
    {
        value.calm = bn::max(value.calm - 1, 0);

        if(value.current == state::WINDUP)
        {
            if(--value.timer <= 0)
            {
                // Off it goes, at where the survivor stands now; the line is kept, so stepping aside works.
                bn::fixed_point line = hero.position() - value.position;

                if(line == bn::fixed_point())
                {
                    line = bn::fixed_point(value.left ? -1 : 1, 0);
                }

                value.velocity = normalized(line) * milli(boar_charge_milli);
                value.current = state::CHARGE;
                value.timer = boar_charge_frames;
                value.left = value.velocity.x() < 0;
                owner.fx().spawn(fx_kind::DUST, value.position - bn::fixed_point(0, 2));

                #if KX_BENCH == 15
                    BN_LOG("KXBOAR charges from ", approx_length(hero.position() - value.position).integer(),
                           " px, heading ", value.velocity.x(), ",", value.velocity.y());
                #endif
            }

            return;
        }

        if(value.current == state::CHARGE)
        {
            _update_charge(owner, value);
            return;
        }

        // Too close: it charges (the survivor in a car is left alone).
        if(! hero.dead() && hero.ride() != ride_kind::CAR && value.calm <= 0 && ((_frame + value.id) & 3) == 0 &&
           approx_length(value.position - hero.position()) < boar_charge_tiles * world::tile_size)
        {
            _wind_up(value, hero.position(), boar_windup_frames);
            _call(owner, value.position, sfx::id::BOAR_GRUNT, grunt_volume, call_reach_px);
            return;
        }
    }

    // Senses every 8 frames: the survivor too close for this animal's nerves.
    if(! hero.dead() && ((_frame + value.id) & 7) == 0 && value.current != state::FLEE)
    {
        int notice = deer ? (hero.crouched() ? deer_notice_crouched : deer_notice) :
                     boar ? (hero.crouched() ? boar_notice_crouched : boar_notice) :
                     hen ? (hero.crouched() ? hen_notice_crouched : hen_notice) :
                     goat ? (hero.speed() > milli(goat_calm_milli) ? goat_notice : 0) :
                            (hero.crouched() ? rabbit_notice_crouched : rabbit_notice);
        notice *= world::tile_size;

        if(deer)
        {
            if(world::terrain_at(tile_of(value.position.x()), feet_tile_y(value.position)) == terrain::FOREST)
            {
                notice = (notice * deer_forest_percent) / 100;
            }

            if(value.current == state::GRAZE)
            {
                notice = (notice * deer_grazing_percent) / 100;
            }
        }

        if(approx_length(value.position - hero.position()) < notice)
        {
            _flee(value, hero.position());

            if(boar || hen || goat)
            {
                _call(owner, value.position, boar ? sfx::id::BOAR_GRUNT : hen ? sfx::id::HEN_CLUCK : sfx::id::GOAT_BLEAT,
                      boar ? grunt_volume : hen ? cluck_volume : bleat_volume, call_reach_px);
            }

            #if KX_BENCH == 13 || KX_BENCH == 15
                BN_LOG("KXWILD ", kind_name(value.kind), " noticed the survivor at ",
                       approx_length(value.position - hero.position()).integer(), " px (notice ", notice, ")");
            #endif
        }
    }

    if(--value.timer > 0)
    {
        return;
    }

    // Grazing, looking up, ambling (deer); sitting and hopping (rabbits).
    bn::random& random = _random;

    switch(value.current)
    {

    case state::GRAZE:
        value.current = state::LOOK;
        value.timer = 60 + random.get_int(90);
        value.velocity = bn::fixed_point();
        break;

    case state::LOOK:
    case state::FLEE:
    {
        value.current = state::WALK;
        bn::fixed speed = deer ? milli(deer_walk_milli) : boar ? milli(boar_walk_milli) :
                          hen ? milli(hen_walk_milli) : goat ? milli(goat_walk_milli) : milli(rabbit_hop_milli);
        value.velocity = heading(random.get_int(256)) * speed;

        // Farm animals stray only so far from the barn door.
        if((hen || goat) && approx_length(value.home - value.position) >
                            (hen ? hen_leash_tiles : goat_leash_tiles) * world::tile_size)
        {
            value.velocity = normalized(value.home - value.position) * speed;
        }
        value.timer = grazer ? 60 + random.get_int(120) : 20 + random.get_int(16);
        break;
    }

    default:
        value.current = grazer ? state::GRAZE : state::LOOK;
        value.velocity = bn::fixed_point();
        value.timer = grazer ? 120 + random.get_int(180) : 60 + random.get_int(120);
        break;
    }
}

void animal_manager::_flee(animal& value, const bn::fixed_point& from)
{
    bn::fixed_point away = value.position - from;

    if(away == bn::fixed_point())
    {
        away = bn::fixed_point(1, 0);
    }

    bn::fixed speed = value.kind == animal_kind::DEER ? milli(deer_flee_milli) :
                      value.kind == animal_kind::BOAR ? milli(boar_flee_milli) :
                      value.kind == animal_kind::WOLF ? milli(wolf_flee_milli) :
                      value.kind == animal_kind::HEN ? milli(hen_flee_milli) :
                      value.kind == animal_kind::GOAT ? milli(goat_flee_milli) : milli(rabbit_flee_milli);
    value.velocity = normalized(away) * speed;
    value.current = state::FLEE;
    value.timer = value.kind == animal_kind::HEN ? hen_flee_frames :
                  value.kind == animal_kind::GOAT ? goat_flee_frames : flee_frames;
}

// ---------------------------------------------------------------------------------------------------------------------
// Wolves
// ---------------------------------------------------------------------------------------------------------------------

void animal_manager::_retreat(animal& value, const bn::fixed_point& from)
{
    bn::fixed_point away = value.position - from;

    if(away == bn::fixed_point())
    {
        away = bn::fixed_point(1, 0);
    }

    value.current = state::RETREAT;
    value.velocity = normalized(away) * milli(wolf_back_milli);
    value.timer = wolf_back_frames;
}

void animal_manager::_scare_pack(const bn::fixed_point& from)
{
    _pack_hunting = false;
    _pack_fear = wolf_fear_frames;

    for(animal& value : _animals)
    {
        if(value.kind == animal_kind::WOLF && value.current != state::DEAD)
        {
            _flee(value, from);
            value.timer = wolf_fear_frames;
        }
    }

    #if KX_BENCH == 15
        BN_LOG("KXWOLF the pack runs");
    #endif
}

void animal_manager::_update_pack(game& owner)
{
    const player& hero = owner.hero();
    int wolves = 0;
    int nearest = -1;
    bn::fixed nearest_distance = 32767;
    bn::fixed closest = 32767;
    bool busy = false;

    for(int index = 0; index < _animals.size(); ++index)
    {
        const animal& value = _animals[index];

        if(value.kind != animal_kind::WOLF || value.current == state::DEAD)
        {
            continue;
        }

        ++wolves;
        bn::fixed distance = approx_length(hero.position() - value.position);
        closest = bn::min(closest, distance);
        busy |= value.current == state::DART || value.current == state::RETREAT;

        if(value.current == state::CIRCLE && distance < nearest_distance)
        {
            nearest = index;
            nearest_distance = distance;
        }
    }

    if(wolves == 0)
    {
        _pack_hunting = false;
        _pack_fear = 0;
        _kept_off = false;
        return;
    }

    _pack_fear = bn::max(_pack_fear - 1, 0);

    // A fire near the survivor keeps the pack further out (a lit campfire, or a burning patch).
    if((_frame & 15) == 0)
    {
        int tile_x = tile_of(hero.position().x());
        int tile_y = feet_tile_y(hero.position());
        _kept_off = base::fire_near(tile_x, tile_y, wolf_fire_tiles, owner.world_minute() / 60) ||
                    owner.throws().fire_near(tile_x, tile_y, wolf_fire_tiles);
    }

    if(! _pack_hunting)
    {
        return;
    }

    // The hunt is off when the survivor dies, gets far away from every wolf, or the day comes.
    if(hero.dead() || closest > wolf_lose_tiles * world::tile_size || ! is_night(owner.minute_of_day()))
    {
        _pack_hunting = false;

        #if KX_BENCH == 15
            BN_LOG("KXWOLF the hunt is off");
        #endif

        return;
    }

    // One at a time, the nearest circling wolf darts in; none while a fire keeps them off or one is out already.
    if(_kept_off || busy || hero.ride() == ride_kind::CAR || --_dart_cooldown > 0 || nearest < 0)
    {
        return;
    }

    animal& darter = _animals[nearest];
    darter.current = state::DART;
    darter.timer = wolf_dart_frames;
    _call(owner, darter.position, sfx::id::WOLF_SNARL, snarl_volume, call_reach_px);

    #if KX_BENCH == 15
        BN_LOG("KXWOLF a wolf darts in from ", nearest_distance.integer(), " px");
    #endif
}

#if KX_BENCH == 15
    void animal_manager::trace_wolves(int frame) const
    {
        for(const animal& value : _animals)
        {
            if(value.kind == animal_kind::WOLF && value.current != state::DEAD)
            {
                BN_LOG("KXWTRACE f=", frame, " id=", int(value.id), " st=", int(value.current), " x=",
                       (value.position.x() * 16).integer(), " y=", (value.position.y() * 16).integer(), " vx=",
                       (value.velocity.x() * 1000).integer(), " vy=", (value.velocity.y() * 1000).integer(),
                       " fr=", value.frame, " left=", int(value.left), " detour=", value.detour, " mx=",
                       (value.moved.x() * 1000).integer(), " my=", (value.moved.y() * 1000).integer());
            }
        }
    }
#endif

void animal_manager::_think_wolf(game& owner, animal& value)
{
    player& hero = owner.hero();
    bn::fixed_point to_hero = hero.position() - value.position;
    bn::fixed distance = approx_length(to_hero);
    bn::random& random = _random;

    if(value.current == state::FLEE)
    {
        if(--value.timer <= 0)
        {
            value.current = state::WALK;
            value.velocity = heading(random.get_int(256)) * milli(wolf_walk_milli);
            value.timer = 60 + random.get_int(120);
        }

        return;
    }

    // Never within a few tiles of a fire.
    if(((_frame + value.id) & 15) == 0 &&
       (base::fire_near(tile_of(value.position.x()), feet_tile_y(value.position), wolf_fire_shy_tiles,
                        owner.world_minute() / 60) ||
        owner.throws().fire_near(tile_of(value.position.x()), feet_tile_y(value.position), wolf_fire_shy_tiles)))
    {
        _retreat(value, hero.position());
        return;
    }

    if(! _pack_hunting)
    {
        if(value.current == state::CIRCLE || value.current == state::DART || value.current == state::RETREAT)
        {
            value.current = state::LOOK;
            value.velocity = bn::fixed_point();
            value.timer = 30;
        }

        // One noticing the survivor sets the whole pack on them.
        if(! hero.dead() && hero.ride() != ride_kind::CAR && _pack_fear <= 0 && ((_frame + value.id) & 7) == 0 &&
           distance < (hero.crouched() ? wolf_notice_crouched : wolf_notice) * world::tile_size)
        {
            _pack_hunting = true;
            _dart_cooldown = wolf_first_dart;

            for(animal& other : _animals)
            {
                if(other.kind == animal_kind::WOLF && other.current != state::DEAD)
                {
                    other.current = state::CIRCLE;
                }
            }

            #if KX_BENCH == 15
                BN_LOG("KXWOLF the pack notices the survivor at ", distance.integer(), " px");
            #endif

            return;
        }

        // Roaming: amble and stop.
        if(--value.timer > 0)
        {
            return;
        }

        if(value.current == state::WALK)
        {
            value.current = state::LOOK;
            value.velocity = bn::fixed_point();
            value.timer = 60 + random.get_int(120);
        }
        else
        {
            value.current = state::WALK;
            value.velocity = heading(random.get_int(256)) * milli(wolf_walk_milli);
            value.timer = 60 + random.get_int(120);
        }

        return;
    }

    if(value.current == state::DART)
    {
        if(distance < wolf_bite_px && hero.ride() != ride_kind::CAR)
        {
            hero.bitten_by_wolf(owner, normalized(to_hero) * milli(wolf_bite_push_milli), wolf_cover_percent);
            _retreat(value, hero.position());
            _dart_cooldown = wolf_dart_gap + random.get_int(wolf_dart_gap_random);

            #if KX_BENCH == 15
                BN_LOG("KXWOLF a bite");
            #endif

            return;
        }

        if(--value.timer <= 0)
        {
            _retreat(value, hero.position());
            _dart_cooldown = wolf_dart_gap + random.get_int(wolf_dart_gap_random);
            return;
        }

        if(value.detour > 0)
        {
            --value.detour;
            return;
        }

        value.velocity = normalized(to_hero) * milli(wolf_dart_milli);
        return;
    }

    if(value.current == state::RETREAT)
    {
        if(--value.timer <= 0)
        {
            value.current = state::CIRCLE;
        }

        return;
    }

    // Circling: a spot on a ring round the survivor that keeps moving round; further out by a fire.
    value.current = state::CIRCLE;
    value.orbit += value.clockwise ? 1 : -1;

    if(value.detour > 0)
    {
        --value.detour;
        return;
    }
    int radius = (_kept_off ? wolf_fire_ring_tiles : wolf_circle_tiles) * world::tile_size;
    bn::fixed_point spot = hero.position() + (ring_heading(value.orbit) * radius);
    bn::fixed_point to_spot = spot - value.position;
    bn::fixed gap = approx_length(to_spot);

    // Towards the point, faster the further behind (so it settles a few px behind it, at the ring's own pace,
    // and never stands still), the speed easing round to that (playtest 3: it used to catch the point, stop, fall
    // behind and start again several times a second).
    bn::fixed_point wanted = to_spot / wolf_pursuit_frames;
    bn::fixed cap = milli(gap > 24 ? wolf_run_milli : wolf_circle_milli);

    if(approx_length(wanted) > cap)
    {
        wanted = normalized(to_spot) * cap;
    }

    value.velocity += (wanted - value.velocity) / wolf_turn_frames;
}

void animal_manager::_wind_up(animal& value, const bn::fixed_point& target, int frames)
{
    value.current = state::WINDUP;
    value.velocity = bn::fixed_point();
    value.timer = frames;
    value.left = target.x() < value.position.x();

    #if KX_BENCH == 15
        BN_LOG("KXBOAR winds up at ", approx_length(target - value.position).integer(), " px for ", frames, " frames");
    #endif
}

void animal_manager::_update_charge(game& owner, animal& value)
{
    player& hero = owner.hero();
    bn::fixed_point next = value.position + value.velocity;
    bool gored = ! hero.dead() && hero.ride() != ride_kind::CAR &&
                 approx_length(hero.position() - value.position) < boar_gore_px;

    if(gored)
    {
        hero.gored(owner, normalized(value.velocity) * milli(boar_knock_milli), boar_knock_frames,
                   boar_trouser_percent);
    }

    // The gore, the end of the run, or something in the way: it turns off and trots away, calmer.
    if(gored || --value.timer <= 0 || ! box_free(next) ||
       world::terrain_at(tile_of(next.x()), feet_tile_y(next)) == terrain::DEEP_WATER)
    {
        #if KX_BENCH == 15
            BN_LOG("KXBOAR charge ends: ", gored ? "gored the survivor" : value.timer <= 0 ? "ran out" : "blocked",
                   " at ", value.position.x().integer(), ",", value.position.y().integer());
        #endif

        _flee(value, hero.position());
        value.timer = boar_turn_frames;
        value.calm = boar_calm_frames;
    }
}

void animal_manager::_move(animal& value)
{
    if(value.velocity == bn::fixed_point())
    {
        value.moved = bn::fixed_point();
        return;
    }

    bn::fixed_point start = value.position;
    bn::fixed_point next = value.position + value.velocity;
    auto open = [](const bn::fixed_point& feet)
    {
        return box_free(feet) && world::terrain_at(tile_of(feet.x()), feet_tile_y(feet)) != terrain::DEEP_WATER;
    };

    if(open(next))
    {
        value.position = next;
    }
    else if(value.velocity.x() != 0 && value.velocity.y() != 0 &&
            open(value.position + bn::fixed_point(value.velocity.x(), 0)))
    {
        // Sliding along what's in the way (its picture follows the slide, not the push: playtest 3).
        value.position += bn::fixed_point(value.velocity.x(), 0);
    }
    else if(value.velocity.x() != 0 && value.velocity.y() != 0 &&
            open(value.position + bn::fixed_point(0, value.velocity.y())))
    {
        value.position += bn::fixed_point(0, value.velocity.y());
    }
    else
    {
        // Blocked: turn a quarter, keeping the speed; a wolf keeps to it for a moment, always to the side it
        // circles (playtest 3: alternating sides at each block made it zig-zag against trees); the others
        // alternate.
        value.detour = 16;
        bn::fixed_point turned(-value.velocity.y(), value.velocity.x());
        bool other_side = value.kind == animal_kind::WOLF ? ! value.clockwise : (value.anim & 1) != 0;

        if(other_side)
        {
            turned = -turned;
        }

        ++value.anim;
        value.velocity = turned;
    }

    // It faces the way it really goes, smoothed over two frames (grinding along a tree's edge it is blocked one
    // frame and free the next); a wolf only once it goes across clearly (its speed eases, so it would flicker as
    // the speed passes zero).
    value.moved += ((value.position - start) - value.moved) / 2;
    bn::fixed across = value.kind == animal_kind::WOLF ? milli(wolf_face_milli) : bn::fixed(0);

    if(bn::abs(value.moved.x()) > across)
    {
        value.left = value.moved.x() < 0;
    }
}

void animal_manager::hear(game& owner, const bn::fixed_point& position, int radius_tiles)
{
    if(radius_tiles < scare_noise)
    {
        return;
    }

    int radius = radius_tiles * world::tile_size;

    for(animal& value : _animals)
    {
        bool busy = value.current == state::WINDUP || value.current == state::CHARGE ||
                    value.current == state::CIRCLE || value.current == state::DART || value.current == state::RETREAT;

        if(value.current != state::DEAD && ! busy && approx_length(value.position - position) < radius)
        {
            _flee(value, position);
        }
    }

    if(! _birds.empty() && ! _flying && approx_length(_birds[0].position - position) < radius)
    {
        _scatter(owner, position);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Birds
// ---------------------------------------------------------------------------------------------------------------------

void animal_manager::_scatter(game& owner, const bn::fixed_point& from)
{
    if(_flying || _birds.empty())
    {
        return;
    }

    _flying = true;
    _fly_timer = bird_fly_frames;
    bn::fixed_point centre = _birds[0].position;

    for(bird& value : _birds)
    {
        bn::fixed_point away = value.position - from;

        if(away == bn::fixed_point())
        {
            away = bn::fixed_point(0, -1);
        }

        // Away from the fright and up into the sky (up the screen).
        bn::fixed_point direction = normalized(normalized(away) + bn::fixed_point(0, -1));
        value.velocity = direction * milli(bird_fly_milli);
    }

    if(_crows)
    {
        _call(owner, centre, sfx::id::CROW_CAW, caw_volume, call_reach_px);
    }
    else
    {
        sfx::play(sfx::id::FLUTTER, milli(700), owner.screen_x(centre));
    }
    owner.noise(centre, _crows ? crow_noise : bird_noise);
    _landing = 0;

    #if KX_BENCH == 13 || KX_BENCH == 15
        BN_LOG("KXWILD the ", _crows ? "crows" : "flock", " of ", _birds.size(), " take off at ",
               centre.x().integer() >> 4, ",", centre.y().integer() >> 4, ", a noise of ",
               _crows ? crow_noise : bird_noise, " tiles");
    #endif
}

void animal_manager::_update_crows(game& owner)
{
    const player& hero = owner.hero();
    bn::random& random = _random;

    // Gliding down.
    if(_landing > 0)
    {
        --_landing;

        for(bird& value : _birds)
        {
            value.position += value.velocity;

            if(_landing == 0)
            {
                value.velocity = bn::fixed_point();
            }
        }
    }
    else
    {
        // On the ground: now and then each pecks, stops, or hops a little, keeping by the corpse.
        for(bird& value : _birds)
        {
            if(value.hop > 0)
            {
                value.position += value.velocity;

                if(--value.hop == 0)
                {
                    value.velocity = bn::fixed_point();
                }

                continue;
            }

            if(--value.timer > 0)
            {
                continue;
            }

            value.timer = crow_idle_frames + random.get_int(crow_idle_frames);

            if(random.get_int(3) == 0)
            {
                bn::fixed_point spot = _carrion + bn::fixed_point(random.get_int((crow_spread_px * 2) + 1) -
                                                                  crow_spread_px, random.get_int(crow_spread_px + 1) -
                                                                  (crow_spread_px / 2));
                bn::fixed_point way = spot - value.position;

                if(way != bn::fixed_point())
                {
                    value.velocity = normalized(way) * milli(crow_hop_milli);
                    value.left = value.velocity.x() < 0;
                    value.hop = crow_hop_frames;
                    value.pecking = false;
                }
            }
            else
            {
                value.pecking = ! value.pecking;
                value.left = random.get_int(4) == 0 ? ! value.left : value.left;
            }
        }
    }

    // The survivor too close: off they go, cawing.
    int notice = (hero.crouched() ? crow_notice_crouched : crow_notice) * world::tile_size;

    if(! hero.dead() && approx_length(hero.position() - _carrion) < notice)
    {
        _scatter(owner, hero.position());
        return;
    }

    if(approx_length(hero.position() - _carrion) > despawn_tiles * world::tile_size)
    {
        _birds.clear();
    }
}

void animal_manager::_update_flock(game& owner)
{
    if(_birds.empty())
    {
        return;
    }

    if(_flying)
    {
        for(bird& value : _birds)
        {
            value.position += value.velocity;
        }

        if(--_fly_timer <= 0)
        {
            _birds.clear();
            _flying = false;
            _bird_cooldown = _crows ? crow_cooldown : bird_cooldown;
        }

        return;
    }

    if(_crows)
    {
        _update_crows(owner);
        return;
    }

    // The survivor or a zombie too close: off they go.
    const player& hero = owner.hero();
    const bn::fixed_point& centre = _birds[0].position;
    int notice = (hero.crouched() ? bird_notice_crouched : bird_notice) * world::tile_size;

    if((! hero.dead() && approx_length(hero.position() - centre) < notice) ||
       owner.zombies().standing_within(centre, bird_zombie_px))
    {
        _scatter(owner, approx_length(hero.position() - centre) < notice ? hero.position() : centre);
        return;
    }

    // Far behind: gone.
    if(approx_length(hero.position() - centre) > despawn_tiles * world::tile_size)
    {
        _birds.clear();
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Hunting
// ---------------------------------------------------------------------------------------------------------------------

void animal_manager::_hit(game& owner, animal& value, int damage, const bn::fixed_point& from)
{
    value.health -= damage;
    value.flash = 4;

    #if KX_BENCH == 13 || KX_BENCH == 15
        BN_LOG("KXWILD hit a ", kind_name(value.kind), " for ", damage, ": health ", value.health);
    #endif
    owner.fx().spawn(fx_kind::BLOOD, value.position - bn::fixed_point(0, fx_body_offset));

    if(value.health > 0)
    {
        // A wounded boar turns on whoever is near; from further off it runs like the rest.
        const player& hero = owner.hero();

        if(value.kind == animal_kind::BOAR && value.current != state::CHARGE && ! hero.dead() &&
           hero.ride() != ride_kind::CAR &&
           approx_length(hero.position() - value.position) < boar_wounded_charge_tiles * world::tile_size)
        {
            _wind_up(value, hero.position(), boar_windup_frames / 2);
            _call(owner, value.position, sfx::id::BOAR_GRUNT, grunt_volume, call_reach_px);
            value.calm = 0;
            return;
        }

        // A wounded wolf backs off, and the pack takes up the hunt if it hadn't.
        if(value.kind == animal_kind::WOLF)
        {
            _retreat(value, hero.position());

            if(! _pack_hunting && _pack_fear <= 0 && ! hero.dead())
            {
                _pack_hunting = true;
                _dart_cooldown = wolf_first_dart;
            }

            return;
        }

        if(value.current != state::CHARGE)
        {
            _flee(value, from);
        }

        return;
    }

    if(value.kind == animal_kind::RABBIT)
    {
        // A rabbit's meat lies where it fell.
        item_stack meat = make_stack(item_id::RAW_MEAT);
        meat.extra = uint16_t(owner.day());
        static_cast<void>(owner.drop_item_at(meat, tile_of(value.position.x()), feet_tile_y(value.position)));
        owner.toast("A RABBIT: RAW MEAT", hud_tiles::bank_accent, 90);
        value.health = -1000;       // removed by the caller's sweep below
        return;
    }

    // A hen: its meat where it fell, the body for a while; the farm has one fewer.
    if(value.kind == animal_kind::HEN)
    {
        item_stack meat = make_stack(item_id::RAW_MEAT);
        meat.extra = uint16_t(owner.day());
        static_cast<void>(owner.drop_item_at(meat, tile_of(value.position.x()), feet_tile_y(value.position)));
        owner.toast("A HEN: RAW MEAT", hud_tiles::bank_accent, 90);
        _farm_loss(value);
        value.current = state::DEAD;
        value.velocity = bn::fixed_point();
        value.frame = -1;
        value.timer = hen_body_frames;
        return;
    }

    // A deer, boar, wolf or goat falls: the carcass stays for butchering (a deer's cell is hunted for a week).
    value.current = state::DEAD;
    value.velocity = bn::fixed_point();
    value.frame = -1;

    if(value.kind == animal_kind::GOAT)
    {
        _farm_loss(value);
        owner.toast("A GOAT DOWN", hud_tiles::bank_accent, 90);
        return;
    }

    if(value.kind == animal_kind::BOAR)
    {
        owner.toast("A BOAR DOWN", hud_tiles::bank_accent, 90);
        return;
    }

    if(value.kind == animal_kind::WOLF)
    {
        owner.toast("A WOLF DOWN", hud_tiles::bank_accent, 90);
        _scare_pack(owner.hero().position());
        return;
    }

    int cell_x = tile_of(value.position.x()) >> macro::cell_shift;
    int cell_y = feet_tile_y(value.position) >> macro::cell_shift;
    kill& entry = _kills[_next_kill];
    entry.cell_x = int8_t(cell_x);
    entry.cell_y = int8_t(cell_y);
    entry.day = int16_t(owner.day());
    _next_kill = (_next_kill + 1) % max_kills;
    owner.toast("A DEER DOWN", hud_tiles::bank_accent, 90);
}

bool animal_manager::shoot(game& owner, const bn::fixed_point& origin, const bn::fixed_point& direction, int range,
                           gun kind)
{
    int best = -1;
    bn::fixed best_along = range;

    for(int index = 0; index < _animals.size(); ++index)
    {
        const animal& value = _animals[index];

        if(value.current == state::DEAD)
        {
            continue;
        }

        bn::fixed_point body = value.position - bn::fixed_point(0, body_height);
        bn::fixed_point offset = body - origin;
        bn::fixed along = dot(offset, direction);

        if(along <= 0 || along >= best_along)
        {
            continue;
        }

        bn::fixed_point closest = origin + (direction * along);

        int radius = value.kind == animal_kind::DEER ? deer_hit_radius :
                     value.kind == animal_kind::BOAR ? boar_hit_radius :
                     value.kind == animal_kind::WOLF ? wolf_hit_radius :
                     value.kind == animal_kind::HEN ? hen_hit_radius :
                     value.kind == animal_kind::GOAT ? goat_hit_radius : rabbit_hit_radius;

        #if KX_BENCH == 13 || KX_BENCH == 15
            BN_LOG("KXWILD shot passes ", approx_length(body - closest), " px from the ", kind_name(value.kind),
                   " (", radius, " to hit) at ", along, " px");
        #endif

        if(approx_length(body - closest) <= radius)
        {
            best = index;
            best_along = along;
        }
    }

    if(best < 0)
    {
        return false;
    }

    int damage = kind == gun::PISTOL || kind == gun::REVOLVER ? pistol_damage : long_gun_damage;
    _hit(owner, _animals[best], damage, origin);

    if(_animals[best].health <= -1000)
    {
        _animals.erase(_animals.begin() + best);
    }

    return true;
}

bool animal_manager::strike(game& owner, const bn::fixed_point& origin, const bn::fixed_point& direction, int reach,
                            int arc_degrees, weapon used)
{
    int best = -1;
    bn::fixed best_distance = reach + 6;

    for(int index = 0; index < _animals.size(); ++index)
    {
        const animal& value = _animals[index];

        if(value.current == state::DEAD)
        {
            continue;
        }

        bn::fixed_point offset = value.position - origin;
        bn::fixed distance = approx_length(offset);

        if(distance < best_distance && in_arc(direction, offset, arc_degrees))
        {
            best = index;
            best_distance = distance;
        }
    }

    if(best < 0)
    {
        return false;
    }

    int damage = used == weapon::SPEAR || used == weapon::AXE ? heavy_melee_damage : melee_damage;
    _hit(owner, _animals[best], damage, origin);

    if(_animals[best].health <= -1000)
    {
        _animals.erase(_animals.begin() + best);
    }

    return true;
}

bool animal_manager::aim_target(const bn::fixed_point& origin, const bn::fixed_point& direction, int range,
                                int arc_degrees, bn::fixed_point& feet) const
{
    bn::fixed best = range;
    bool found = false;

    for(const animal& value : _animals)
    {
        if(value.current == state::DEAD)
        {
            continue;
        }

        bn::fixed_point offset = (value.position - bn::fixed_point(0, body_height)) - origin;
        bn::fixed distance = approx_length(offset);

        if(distance < best && in_arc(direction, offset, arc_degrees))
        {
            best = distance;
            feet = value.position;
            found = true;
        }
    }

    return found;
}

int animal_manager::_index_of(int id) const
{
    for(int index = 0; index < _animals.size(); ++index)
    {
        if(_animals[index].id == id)
        {
            return index;
        }
    }

    return -1;
}

int animal_manager::carcass_at(const bn::fixed_point& point, int reach) const
{
    for(const animal& value : _animals)
    {
        if(value.current == state::DEAD && value.kind != animal_kind::HEN &&
           approx_length(value.position - point) <= reach)
        {
            return value.id;
        }
    }

    return -1;
}

bool animal_manager::carcass_exists(int id) const
{
    int index = _index_of(id);
    return index >= 0 && _animals[index].current == state::DEAD && _animals[index].kind != animal_kind::HEN;
}

animal_kind animal_manager::kind_of_id(int id) const
{
    int index = _index_of(id);
    return index >= 0 ? _animals[index].kind : animal_kind::DEER;
}

bn::fixed_point animal_manager::position_of(int id) const
{
    int index = _index_of(id);
    return index >= 0 ? _animals[index].position : bn::fixed_point();
}

void animal_manager::remove(int id)
{
    int index = _index_of(id);

    if(index >= 0)
    {
        _animals.erase(_animals.begin() + index);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------------------------------------------------

void animal_manager::update_visuals(int camera_left, int camera_top)
{
    static_cast<void>(camera_left);

    // Night: every sprite is shaded again when it begins or ends, and every frame while a light is on (new ones are
    // shaded when they appear).
    bool reshade = light::sprites_dark() != _shaded_dark || (light::sprites_dark() && light::lights_on());
    _shaded_dark = light::sprites_dark();

    // Sprites are only touched when something about them changed (Butano redoes work for every setter).
    for(animal& value : _animals)
    {
        int frame;
        bool mirror = value.left;
        bool moving = approx_length(value.moved) >= milli(20);     // how it really went (playtest 3)
        ++value.anim;

        bool boar = value.kind == animal_kind::BOAR;
        bool wolf = value.kind == animal_kind::WOLF;
        bool goat = value.kind == animal_kind::GOAT;
        bn::fixed vertical = value.moved.y();
        bool upright = ! moving || bn::abs(value.moved.x()) * 10 >= bn::abs(vertical) * 7;

        if(value.kind == animal_kind::RABBIT)
        {
            frame = moving ? animal_frames::rabbit_hop : animal_frames::rabbit_sit;
        }
        else if(value.kind == animal_kind::HEN)
        {
            // Pecking, flapping off, or walking (its side, front or back).
            bool stepping = moving && ((value.anim / 8) & 1);

            if(value.current == state::DEAD)
            {
                frame = animal_frames::hen_dead;
            }
            else if(value.current == state::FLEE)
            {
                frame = (value.anim / 4) & 1 ? animal_frames::hen_side_flap : animal_frames::hen_side_stand;
            }
            else if(value.current == state::GRAZE)
            {
                frame = (value.anim / 24) & 1 ? animal_frames::hen_side_peck : animal_frames::hen_side_stand;
            }
            else if(upright)
            {
                frame = animal_frames::hen_side_stand;
            }
            else
            {
                mirror = false;
                frame = vertical < 0 ? (stepping ? animal_frames::hen_up_step : animal_frames::hen_up_stand) :
                                       (stepping ? animal_frames::hen_down_step : animal_frames::hen_down_stand);
            }
        }
        else if(value.current == state::DEAD)
        {
            frame = boar ? animal_frames::boar_dead : wolf ? animal_frames::wolf_dead :
                    goat ? animal_frames::goat_dead : animal_frames::deer_carcass;
        }
        else
        {
            // The same order for both: side stand, side step (the boar's run), down stand, down step, up stand, up
            // step.
            int base = boar ? animal_frames::boar_side_stand : wolf ? animal_frames::wolf_side_stand :
                       goat ? animal_frames::goat_side_stand : animal_frames::deer_side_stand;
            bool running = value.current == state::FLEE || value.current == state::CHARGE ||
                           value.current == state::DART || value.current == state::RETREAT ||
                           (value.current == state::CIRCLE && approx_length(value.moved) > milli(1200));
            int step = running ? 4 : 10;
            bool stepping = moving && ((value.anim / step) & 1);
            bn::fixed vx = bn::abs(value.moved.x());
            bn::fixed vy = value.moved.y();

            // Side or front/back by the way it moves, with a margin either way (playtest 3: as a wolf's course swung
            // round, its picture flickered between the views): from the side it turns to face up or down past
            // about 63 degrees, and back to the side below about 48. Barely moving, it keeps its view (the
            // direction of a creep is noise), standing in it when still.
            bool was_side = value.frame < 0 || value.frame == base || value.frame == base + 1;
            bool was_up = value.frame == base + 4 || value.frame == base + 5;
            bool creeping = approx_length(value.moved) < milli(view_still_milli);
            bool side = creeping ? was_side :
                        was_side ? vx * 10 >= bn::abs(vy) * 5 : vx * 10 >= bn::abs(vy) * 9;
            bool up = creeping ? was_up : vy < 0;

            if(side)
            {
                frame = base + (stepping ? 1 : 0);
            }
            else
            {
                mirror = false;
                frame = base + (up ? 4 : 2) + (stepping ? 1 : 0);
            }
        }

        if(frame != value.frame)
        {
            value.frame = frame;
            value.sprite.set_tiles(sprite_item_of(value.kind).tiles_item(), frame);
        }

        if(mirror != value.mirrored)
        {
            value.mirrored = mirror;
            value.sprite.set_horizontal_flip(mirror);
        }

        int x = value.position.x().floor_integer();
        int y = value.position.y().floor_integer();

        // A boar winding up paws and shakes: the warning before the charge.
        if(value.current == state::WINDUP)
        {
            x += ((value.anim / 3) & 1) ? 1 : -1;
        }

        if(x != value.sprite_x || y != value.sprite_y)
        {
            value.sprite_x = x;
            value.sprite_y = y;
            value.sprite.set_position(x, y - sprite_feet_offset);
        }

        int z = depth_z(y, camera_top);

        if(z != value.sprite_z)
        {
            value.sprite_z = z;
            value.sprite.set_z_order(z);
        }

        if(value.flash > 0)
        {
            --value.flash;
            value.sprite.set_visible((value.flash & 1) == 0);
        }

        if(reshade)
        {
            light::shade(value.sprite);
        }
    }

    int bird_frame = _flying ? ((_fly_timer / 5) & 1 ? animal_frames::bird_wings_up : animal_frames::bird_wings_down) :
                               animal_frames::bird_perched;

    bool wings_up = ((_fly_timer + _landing) / 5) & 1;

    for(bird& value : _birds)
    {
        int frame = bird_frame;
        bool mirror = false;

        if(_crows)
        {
            frame = _flying || _landing > 0 ? (wings_up ? animal_frames::crow_wings_up : animal_frames::crow_wings_down) :
                    value.pecking ? animal_frames::crow_peck : animal_frames::crow_perched;
            mirror = _flying ? value.velocity.x() < 0 : value.left;
        }

        if(frame != value.frame)
        {
            value.frame = frame;
            value.sprite.set_tiles(bn::sprite_items::birds.tiles_item(), frame);
        }

        if(mirror != value.mirrored)
        {
            value.mirrored = mirror;
            value.sprite.set_horizontal_flip(mirror);
        }

        int x = value.position.x().floor_integer();
        int y = value.position.y().floor_integer();

        if(x != value.sprite_x || y != value.sprite_y)
        {
            value.sprite_x = x;
            value.sprite_y = y;
            value.sprite.set_position(x, y - 4);
        }

        int z = _flying || _landing > 0 ? flying_z : depth_z(y, camera_top);

        if(z != value.sprite_z)
        {
            value.sprite_z = z;
            value.sprite.set_z_order(z);
        }

        if(reshade)
        {
            light::shade(value.sprite);
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Saving: only the hunted cells.
// ---------------------------------------------------------------------------------------------------------------------

void animal_manager::save(save::writer& out) const
{
    out.put(uint8_t(_next_kill));

    for(const kill& value : _kills)
    {
        out.put(value.cell_x);
        out.put(value.cell_y);
        out.put(value.day);
    }

    // Animals plan stage 4: the farms' losses and milking days.
    out.put(uint8_t(farm_marker));
    out.put(uint8_t(_next_tally));

    for(const tally& value : _tallies)
    {
        out.put(value.cell_x);
        out.put(value.cell_y);
        out.put(value.hens_lost);
        out.put(value.goats_lost);
        out.put(value.milked[0]);
        out.put(value.milked[1]);
    }
}

void animal_manager::load(save::reader& in)
{
    uint8_t next = 0;
    in.get(next);
    _next_kill = bn::min(int(next), max_kills - 1);

    for(kill& value : _kills)
    {
        in.get(value.cell_x);
        in.get(value.cell_y);
        in.get(value.day);
    }

    for(tally& value : _tallies)
    {
        value = tally();
    }

    _next_tally = 0;

    if(in.remaining() >= 2 + (max_tallies * 8) && in.get<uint8_t>() == farm_marker)
    {
        _next_tally = bn::min(int(in.get<uint8_t>()), max_tallies - 1);

        for(tally& value : _tallies)
        {
            in.get(value.cell_x);
            in.get(value.cell_y);
            in.get(value.hens_lost);
            in.get(value.goats_lost);
            in.get(value.milked[0]);
            in.get(value.milked[1]);
        }
    }

    for(known_farm& value : _farms)
    {
        value = known_farm();
    }

    _next_farm = 0;

    for(uint16_t& cell : _no_yard)
    {
        cell = 0xFFFF;
    }

    _next_no_yard = 0;
    clear();
}

}
