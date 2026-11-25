#include "kx_vehicles.h"
#include "kx_light.h"

#include "bn_algorithm.h"
#include "bn_math.h"

#include "bn_sprite_items_bike.h"
#include "bn_sprite_items_car.h"

#include "kx_game.h"
#include "kx_hash.h"
#include "kx_hud_tiles.h"
#include "kx_input.h"
#include "kx_save.h"
#include "kx_sfx.h"
#include "kx_ui.h"
#include "kx_vector.h"
#include "kx_vehicle_frames.h"
#include "kx_world.h"

namespace kx
{

using namespace vehicle_tuning;

static_assert(max_active <= world::max_obstacles);

namespace
{
    constexpr uint16_t no_origin = 0xFFFF;
    constexpr int blocks_per_side = world::width_tiles >> block_shift;
    constexpr int block_px = world::tile_size << block_shift;

    // record::bits
    constexpr uint8_t bit_used = 1;
    constexpr uint8_t bit_bike = 2;
    constexpr uint8_t bit_wreck = 4;
    constexpr int variant_shift = 3;
    constexpr uint8_t variant_mask = 3 << variant_shift;
    constexpr uint8_t bit_alarm = 0x20;         // an alarm that has not gone off yet
    constexpr uint8_t bit_hotwired = 0x40;
    constexpr uint8_t bit_lights = 0x80;

    // record::parts
    constexpr uint8_t part_battery = 1;
    constexpr uint8_t part_plug = 2;
    constexpr uint8_t part_radiator = 4;

    // Vehicles come into play within this distance of the player (the 3x3 blocks looked at reach a block and a
    // half either way) and leave it a little further out.
    constexpr int activate_px = 2 * block_px;
    constexpr int release_px = (5 * block_px) / 2;

    // Off-map container positions (like a past survivor's gear): trunk and glove box by the vehicle's block, or by
    // its tracked slot for spawned ones.
    constexpr int container_tile_x = -10;
    constexpr int spawned_tile_x = -14;

    constexpr int sprite_bg_priority = 2;
    constexpr int view_margin = 24;

    [[nodiscard]] bool is_bike(uint8_t bits)
    {
        return bits & bit_bike;
    }

    // Some of the cars the seed put out have moss grown on them, whoever drives them later: rolled from where they
    // were found, so nothing is stored (spawned ones stay clean).
    [[nodiscard]] bool mossy(uint16_t origin)
    {
        return origin != no_origin &&
               int(hash_xy(world::seed() ^ 0x4d4f5353u, origin, 0) % 100) < vehicle_tuning::moss_percent;
    }

    // Which of the wreck looks (burnt with the bonnet gone, picked clean, its bonnet in the road), rolled from where
    // the seed put it; spawned wrecks are burnt.
    [[nodiscard]] int wreck_look(uint16_t origin)
    {
        return origin == no_origin ? 0 :
               int(hash_xy(world::seed() ^ 0x57524b4cu, origin, 2) % vehicle_frames::car_wreck_looks);
    }

    [[nodiscard]] bool is_wreck(uint8_t bits)
    {
        return bits & bit_wreck;
    }

    // Half the collision box of a car at a heading (the box around the turned car).
    void extents(bn::fixed heading, int& half_x, int& half_y)
    {
        int angle = (heading * (2048 / headings)).integer() & 2047;
        bn::fixed sine = bn::abs(bn::lut_sin(angle));
        bn::fixed cosine = bn::abs(bn::lut_cos(angle));
        half_x = ((cosine * half_length) + (sine * half_width)).round_integer();
        half_y = ((sine * half_length) + (cosine * half_width)).round_integer();
    }

    void extents(int heading, int& half_x, int& half_y)
    {
        extents(bn::fixed(heading), half_x, half_y);
    }

    [[nodiscard]] bn::fixed_point forward_of(bn::fixed heading)
    {
        int angle = (heading * (2048 / headings)).integer() & 2047;
        return bn::fixed_point(bn::lut_cos(angle), bn::lut_sin(angle));
    }

    [[nodiscard]] bool road_like(int tile_x, int tile_y)
    {
        terrain ground = world::terrain_at(tile_x, tile_y);
        return ground == terrain::ROAD || ground == terrain::PAVEMENT;
    }

    // A carried fuel can with fuel in it, or with room.
    [[nodiscard]] item_stack* fuel_can(game& owner, bool with_fuel)
    {
        inventory& items = owner.hero().items();

        for(int slot = 0; slot < inventory::slots; ++slot)
        {
            item_stack& can = items.slot(slot);

            if(can.id == item_id::FUEL_CAN && (with_fuel ? can.amount > 0 : can.amount < info(can.id).durability))
            {
                return &can;
            }
        }

        return nullptr;
    }

    void give(game& owner, item_stack stack)
    {
        if(! owner.hero().items().add(stack))
        {
            static_cast<void>(owner.drop_item(stack));
        }
    }

    void deny(game& owner, const char* problem)
    {
        owner.toast(problem, hud_tiles::bank_danger, 60);
        sfx::play(sfx::id::DENIED, milli(600));
    }

    constexpr const char* part_names[] = { "BATTERY", "SPARK PLUG", "RADIATOR", "TYRE", "FUEL" };
}

vehicle_manager::vehicle_manager(const bn::camera_ptr& camera) :
    _camera(camera)
{
    reset();
}

void vehicle_manager::reset()
{
    for(record& value : _tracked)
    {
        value = record();
    }

    for(active& value : _active)
    {
        value = active();
    }

    _driven = -1;
    _ridden = -1;
    _resume_slot = -1;
    _alarm_frames = 0;
    _job = job::NONE;
    _engine = false;
    rescan();

    if(world::test_world())
    {
        // The benches' map: a bicycle in the shed, a wreck on the road south and, on the pavement west of the
        // start, a car that needs a battery and fuel (mockup 10).
        auto place = [this](int slot, int tile_x, int tile_y, int heading, uint8_t bits, uint8_t parts, int tyres,
                            int fuel)
        {
            record& value = _tracked[slot];
            value.x = uint16_t((tile_x * world::tile_size) + (world::tile_size / 2));
            value.y = uint16_t((tile_y * world::tile_size) + (world::tile_size / 2));
            value.origin = no_origin;
            value.heading = uint8_t(heading);
            value.bits = uint8_t(bit_used | bits);
            value.parts = parts;
            value.tyres = uint8_t(tyres);
            value.fuel = uint16_t(fuel);
            value.condition = 100;
            value.charge = 100;
        };

        place(0, 23, 12, 0, bit_bike, 0, 0, 0);
        place(1, 29, 44, 10, bit_wreck, part_plug, 2, 30);
        place(2, 6, 19, 0, 0, part_plug | part_radiator, 3, 0);
    }
}

void vehicle_manager::rescan()
{
    for(int index = 0; index < max_active; ++index)
    {
        if(index != _driven && index != _ridden)
        {
            _active[index] = active();
        }
    }

    _scan_block_x = -1000;
    _scan_block_y = -1000;
    _scan_step = 0;
    _obstacles_dirty = true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Placement from the seed, and the vehicles in play around the player.
// ---------------------------------------------------------------------------------------------------------------------

bool vehicle_manager::_seeded(int block_x, int block_y, record& output, bool& ready) const
{
    ready = true;

    // The last block's id is the "no origin" mark; it is open sea anyway.
    if(world::test_world() || unsigned(block_x) >= unsigned(blocks_per_side) ||
       unsigned(block_y) >= unsigned(blocks_per_side) || (block_x == blocks_per_side - 1 && block_y == blocks_per_side - 1))
    {
        return false;
    }

    int first_x = (block_x << block_shift) + block_offset;
    int first_y = (block_y << block_shift) + block_offset;
    int last = (1 << block_shift) - 1;

    // A block can lie across chunks: all of it must be generated.
    if(! world::loaded(first_x, first_y) || ! world::loaded(first_x + last, first_y) ||
       ! world::loaded(first_x, first_y + last) || ! world::loaded(first_x + last, first_y + last))
    {
        ready = false;
        return false;
    }

    roll_stream rng(hash_xy(world::seed() ^ 0x56454831u, block_x, block_y));

    for(int attempt = 0; attempt < placement_tries; ++attempt)
    {
        // Every roll is made whatever the spot turns out to be, so the stream never depends on the order of tests.
        int tile_x = first_x + 2 + rng.next((1 << block_shift) - 4);
        int tile_y = first_y + 2 + rng.next((1 << block_shift) - 4);
        int chance = rng.next(100);
        int detail = rng.next(1 << 16);
        int state = rng.next(1 << 16);
        int amounts = rng.next(1 << 16);

        // Milestone 14: nothing parks on a set piece (the coach, the barricade, the checkpoint).
        if(world::wall_gfx(tile_x, tile_y) != world_gfx::EMPTY ||
           world::road_piece_near(tile_x, tile_y, vehicle_tuning::piece_clearance) != world::road_piece::NONE)
        {
            continue;
        }

        world_gfx ground = world_gfx(world_gfx_base[int(world::ground_gfx(tile_x, tile_y))]);
        bool road = ground == world_gfx::ROAD || ground == world_gfx::ROAD_LINE_H || ground == world_gfx::ROAD_LINE_V;
        bool concrete = ground == world_gfx::CONCRETE;
        int left;
        int top;
        int width;
        int height;
        bool roofed = world::building_over(tile_x, tile_y, left, top, width, height);

        output = record();
        output.x = uint16_t((tile_x * world::tile_size) + (world::tile_size / 2));
        output.y = uint16_t((tile_y * world::tile_size) + (world::tile_size / 2));
        output.origin = uint16_t((block_y * blocks_per_side) + block_x);
        output.condition = 100;

        if(roofed)
        {
            // Garages, barns and petrol stations: a bicycle.
            if(concrete && chance < bicycle_percent)
            {
                output.bits = uint8_t(bit_used | bit_bike);
                return true;
            }

            continue;
        }

        if(! road && ! concrete && ground != world_gfx::PAVEMENT)
        {
            continue;
        }

        if(chance >= (road ? road_percent : pavement_percent))
        {
            continue;
        }

        // Along the road it stands on; wrecks on the road may be left at any angle.
        bool along_x = road_like(tile_x - 2, tile_y) && road_like(tile_x + 2, tile_y);
        bool along_y = road_like(tile_x, tile_y - 2) && road_like(tile_x, tile_y + 2);
        int heading = along_x && ! along_y ? 0 : along_y && ! along_x ? 8 : (detail & 1) ? 0 : 8;
        heading += (detail & 2) ? 16 : 0;
        bool wreck = ((detail >> 2) % 100) < wreck_percent;

        if(wreck && road && ((detail >> 9) % 100) < crashed_percent)
        {
            heading = (state & 15) * 2;
        }

        int half_x;
        int half_y;
        extents(heading, half_x, half_y);

        if(world::box_blocked(output.x - half_x, output.y - half_y, output.x + half_x, output.y + half_y))
        {
            continue;
        }

        output.heading = uint8_t(heading);
        output.bits = uint8_t(bit_used | ((amounts & 3) << variant_shift));

        if(wreck)
        {
            // Burnt out, but maybe with something left to take.
            output.bits = uint8_t(output.bits | bit_wreck);
            output.parts = uint8_t((((state >> 4) % 100) < 20 ? part_battery : 0) |
                                   (((state >> 8) % 100) < 30 ? part_plug : 0));
            output.tyres = uint8_t(((amounts >> 2) % 6) / 2);                   // 0-2
            output.fuel = uint16_t(((amounts >> 5) % 10) < 6 ? 0 : 10 + ((amounts >> 9) % 50));
            output.condition = 0;
            output.charge = uint8_t(20 + ((amounts >> 4) % 60));
        }
        else
        {
            // Left where it stopped: something is always missing.
            output.parts = uint8_t((((state >> 4) % 100) < 50 ? part_battery : 0) |
                                   (((state >> 8) % 100) < 60 ? part_plug : 0) |
                                   (((state >> 11) % 10) < 4 ? part_radiator : 0));
            output.tyres = uint8_t(2 + ((amounts >> 2) % 3));                   // 2-4
            output.fuel = uint16_t(((amounts >> 5) % 10) < 4 ? 0 : 5 + ((amounts >> 9) % 120));
            output.condition = uint8_t(50 + ((amounts >> 3) % 51));
            output.charge = uint8_t(30 + ((amounts >> 4) % 71));
        }

        if((state % 100) < alarm_percent && (output.parts & part_battery))
        {
            output.bits = uint8_t(output.bits | bit_alarm);
        }

        return true;
    }

    return false;
}

int vehicle_manager::_find_active(uint16_t origin, int slot) const
{
    for(int index = 0; index < max_active; ++index)
    {
        const active& value = _active[index];

        if(value.live && ((slot >= 0 && value.slot == slot) || (origin != no_origin && value.data.origin == origin)))
        {
            return index;
        }
    }

    return -1;
}

int vehicle_manager::_activate(const record& data, int slot)
{
    for(int index = 0; index < max_active; ++index)
    {
        active& value = _active[index];

        if(! value.live)
        {
            value = active();
            value.data = data;
            value.slot = int8_t(slot);
            value.live = true;
            _obstacles_dirty = true;
            return index;
        }
    }

    return -1;
}

void vehicle_manager::_scan(game& owner)
{
    const bn::fixed_point& feet = owner.hero().position();
    int player_x = feet.x().integer();
    int player_y = feet.y().integer();

    // Vehicles left far behind go (tracked ones are already written back).
    if(_frame % 16 == 0)
    {
        for(int index = 0; index < max_active; ++index)
        {
            active& value = _active[index];

            if(value.live && index != _driven && index != _ridden &&
               (bn::abs(value.data.x - player_x) > release_px || bn::abs(value.data.y - player_y) > release_px))
            {
                value = active();
                _obstacles_dirty = true;
            }
        }
    }

    // Tracked ones close by come into play.
    if(_frame % 16 == 8)
    {
        for(int slot = 0; slot < max_tracked; ++slot)
        {
            const record& value = _tracked[slot];

            if((value.bits & bit_used) && bn::abs(value.x - player_x) <= activate_px &&
               bn::abs(value.y - player_y) <= activate_px && _find_active(value.origin, slot) < 0)
            {
                _activate(value, slot);
            }
        }
    }

    // The seed's vehicles: the 3x3 blocks around the player, one block a frame, again whenever the player changes
    // block. A block whose chunk is not generated yet is tried again next frame.
    int block_x = bn::max(player_x - (block_offset * world::tile_size), 0) / block_px;
    int block_y = bn::max(player_y - (block_offset * world::tile_size), 0) / block_px;

    if(block_x != _scan_block_x || block_y != _scan_block_y)
    {
        _scan_block_x = block_x;
        _scan_block_y = block_y;
        _scan_step = 0;
        _scan_again = false;
    }

    if(_scan_step >= 9)
    {
        // Blocks whose chunks were still being generated get another look.
        if(_scan_again && _frame % 32 == 0)
        {
            _scan_step = 0;
            _scan_again = false;
        }

        return;
    }

    int look_x = block_x + (_scan_step % 3) - 1;
    int look_y = block_y + (_scan_step / 3) - 1;
    record found;
    bool ready;

    if(_seeded(look_x, look_y, found, ready))
    {
        bool known = _find_active(found.origin, -1) >= 0;

        for(int slot = 0; slot < max_tracked && ! known; ++slot)
        {
            known = (_tracked[slot].bits & bit_used) && _tracked[slot].origin == found.origin;
        }

        if(! known)
        {
            _activate(found, -1);
        }
    }

    _scan_again |= ! ready;
    ++_scan_step;
}

bool vehicle_manager::_track(game& owner, active& value, bool quiet)
{
    if(value.slot >= 0)
    {
        return true;
    }

    int free_slot = -1;
    int free_count = 0;

    for(int slot = 0; slot < max_tracked; ++slot)
    {
        if(! (_tracked[slot].bits & bit_used))
        {
            ++free_count;

            if(free_slot < 0)
            {
                free_slot = slot;
            }
        }
    }

    // The last few slots are kept for vehicles that can move.
    bool wreck = is_wreck(value.data.bits);

    if(free_slot < 0 || (wreck && free_count <= reserved_slots))
    {
        if(! quiet)
        {
            deny(owner, wreck ? "CAN'T TRACK MORE WRECKS" : "CAN'T TRACK MORE VEHICLES");
        }

        return false;
    }

    value.slot = int8_t(free_slot);
    _commit(value);
    return true;
}

void vehicle_manager::_commit(active& value)
{
    if(value.slot >= 0)
    {
        _tracked[value.slot] = value.data;
    }
}

void vehicle_manager::_rebuild_obstacles(game& owner)
{
    world::obstacle list[max_active];
    int count = 0;

    for(int index = 0; index < max_active; ++index)
    {
        const active& value = _active[index];

        // Bicycles are stepped over; the car being driven collides on its own.
        if(! value.live || is_bike(value.data.bits) || index == _driven)
        {
            continue;
        }

        int half_x;
        int half_y;
        extents(int(value.data.heading), half_x, half_y);
        list[count++] = world::obstacle{ uint16_t(bn::max(value.data.x - half_x, 0)),
                                         uint16_t(bn::max(value.data.y - half_y, 0)),
                                         uint16_t(value.data.x + half_x), uint16_t(value.data.y + half_y) };
    }

    world::set_obstacles(list, count);
    _obstacles_dirty = false;
    static_cast<void>(owner);
}

// ---------------------------------------------------------------------------------------------------------------------
// Queries.
// ---------------------------------------------------------------------------------------------------------------------

int vehicle_manager::reachable(const bn::fixed_point& feet, const bn::fixed_point& probe) const
{
    int feet_x = feet.x().integer();
    int feet_y = feet.y().integer() - 3;
    int probe_x = probe.x().integer();
    int probe_y = probe.y().integer() - 3;
    int best = -1;
    int best_distance = 0;

    for(int index = 0; index < max_active; ++index)
    {
        const active& value = _active[index];

        if(! value.live || index == _driven || index == _ridden)
        {
            continue;
        }

        int half_x = bike_reach_px;
        int half_y = bike_reach_px;

        if(! is_bike(value.data.bits))
        {
            extents(int(value.data.heading), half_x, half_y);
            half_x += car_reach_px;
            half_y += car_reach_px;
        }

        bool at_feet = bn::abs(feet_x - value.data.x) <= half_x && bn::abs(feet_y - value.data.y) <= half_y;
        bool at_probe = bn::abs(probe_x - value.data.x) <= half_x && bn::abs(probe_y - value.data.y) <= half_y;

        if(at_feet || at_probe)
        {
            int distance = bn::abs(feet_x - value.data.x) + bn::abs(feet_y - value.data.y);

            if(best < 0 || distance < best_distance)
            {
                best = index;
                best_distance = distance;
            }
        }
    }

    return best;
}

vehicle_kind vehicle_manager::kind(int index) const
{
    uint8_t bits = _active[index].data.bits;
    return is_bike(bits) ? vehicle_kind::BICYCLE : is_wreck(bits) ? vehicle_kind::WRECK : vehicle_kind::CAR;
}

bn::fixed_point vehicle_manager::position(int index) const
{
    return bn::fixed_point(_active[index].data.x, _active[index].data.y);
}

const char* vehicle_manager::name(int index) const
{
    constexpr const char* sedans[] = { "BLUE SEDAN", "RED SEDAN", "CREAM SEDAN", "GREEN SEDAN" };
    static_assert(sizeof(sedans) / sizeof(sedans[0]) == vehicle_frames::car_colours);
    uint8_t bits = _active[index].data.bits;
    return is_bike(bits) ? "BICYCLE" : is_wreck(bits) ? "WRECK" : sedans[(bits & variant_mask) >> variant_shift];
}

int vehicle_manager::condition(int index) const
{
    return _active[index].data.condition;
}

int vehicle_manager::tracked_count() const
{
    int result = 0;

    for(const record& value : _tracked)
    {
        result += (value.bits & bit_used) != 0;
    }

    return result;
}

int vehicle_manager::mossy_count() const
{
    int result = 0;

    for(const active& value : _active)
    {
        result += value.live && ! is_bike(value.data.bits) && mossy(value.data.origin);
    }

    return result;
}

int vehicle_manager::active_count() const
{
    int result = 0;

    for(const active& value : _active)
    {
        result += value.live;
    }

    return result;
}

int vehicle_manager::tracked_checksum() const
{
    int result = 0;

    for(int slot = 0; slot < max_tracked; ++slot)
    {
        const record& value = _tracked[slot];

        if(value.bits & bit_used)
        {
            result += (slot + 1) * (value.x + (value.y * 3) + (value.fuel * 7) + (value.bits * 11) + (value.parts * 13) +
                                    (value.tyres * 17) + (value.heading * 19) + value.origin);
            result &= 0x3FFFFFFF;
        }
    }

    return result;
}

int vehicle_manager::speed_milli() const
{
    return _driven >= 0 ? (approx_length(_velocity) * 1000).integer() : 0;
}

int vehicle_manager::first(vehicle_kind which) const
{
    for(int index = 0; index < max_active; ++index)
    {
        if(_active[index].live && kind(index) == which)
        {
            return index;
        }
    }

    return -1;
}

void vehicle_manager::numbers(int index, int& fuel_dl, int& heading, int& tyres, int& charge) const
{
    const record& data = _active[index].data;
    fuel_dl = data.fuel;
    heading = data.heading;
    tyres = data.tyres;
    charge = data.charge;
}

bool vehicle_manager::lit() const
{
    for(const active& value : _active)
    {
        if(value.live && (value.data.bits & bit_lights))
        {
            return true;
        }
    }

    return false;
}

bool vehicle_manager::headlights(bn::fixed_point& front, int& angle) const
{
    // How far ahead of a car's centre its lamps are.
    constexpr int lamps_ahead = 12;
    int chosen = -1;

    for(int index = 0; index < max_active; ++index)
    {
        const active& value = _active[index];

        if(value.live && (value.data.bits & bit_lights) && ! is_bike(value.data.bits) &&
                (chosen < 0 || index == _driven))
        {
            chosen = index;
        }
    }

    if(chosen < 0)
    {
        return false;
    }

    bn::fixed_point centre;
    int heading;

    if(chosen == _driven)
    {
        centre = _position;
        heading = _heading.round_integer();
    }
    else
    {
        centre = bn::fixed_point(_active[chosen].data.x, _active[chosen].data.y);
        heading = _active[chosen].data.heading;
    }

    // Headings run clockwise from east in 32 steps; the hardware turns sprites counterclockwise.
    angle = ((headings - (heading & (headings - 1))) & (headings - 1)) * 360 / headings;
    front = bn::fixed_point(centre.x() + (bn::degrees_lut_cos(angle) * lamps_ahead),
                            centre.y() - (bn::degrees_lut_sin(angle) * lamps_ahead));
    return true;
}

int vehicle_manager::hit_driven(int damage)
{
    if(_driven < 0)
    {
        return 100;
    }

    record& data = _active[_driven].data;
    data.condition = uint8_t(bn::max(int(data.condition) - damage, 1));
    return data.condition;
}

bool vehicle_manager::driven_lit() const
{
    return _driven >= 0 && (_active[_driven].data.bits & bit_lights);
}

void vehicle_manager::set_lights(int index, bool on)
{
    if(index >= 0 && index < max_active && _active[index].live)
    {
        record& data = _active[index].data;
        data.bits = uint8_t(on ? data.bits | bit_lights : data.bits & ~bit_lights);
    }
}

bool vehicle_manager::status(char* line1, char* line2) const
{
    if(_driven < 0)
    {
        return false;
    }

    const record& data = _active[_driven].data;
    int length = ui::append(line1, 0, "FUEL ");
    length = ui::append_int(line1, length, data.fuel / 10);
    ui::append(line1, length, " L");

    if(! _engine)
    {
        ui::append(line2, 0, _stall_timer > 0 ? "STALLED" : "ENGINE OFF");
    }
    else
    {
        length = ui::append(line2, 0, "CAR ");
        length = ui::append_int(line2, length, data.condition);
        ui::append(line2, length, (data.bits & bit_lights) ? "% LIGHTS" : "%");
    }

    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// The checklist and work on a car.
// ---------------------------------------------------------------------------------------------------------------------

bool vehicle_manager::part_ok(int index, car_part part) const
{
    const record& data = _active[index].data;

    switch(part)
    {

    case car_part::BATTERY:
        return (data.parts & part_battery) && data.charge > 0;

    case car_part::SPARK_PLUG:
        return data.parts & part_plug;

    case car_part::RADIATOR:
        return data.parts & part_radiator;

    case car_part::TYRES:
        return data.tyres >= needed_tyres;

    default:
        return data.fuel > 0;
    }
}

bool vehicle_manager::part_full(int index, car_part part) const
{
    const record& data = _active[index].data;

    switch(part)
    {

    case car_part::BATTERY:
        return data.parts & part_battery;

    case car_part::SPARK_PLUG:
        return data.parts & part_plug;

    case car_part::RADIATOR:
        return data.parts & part_radiator;

    case car_part::TYRES:
        return data.tyres >= 4;

    default:
        return data.fuel + 10 > tank_dl;
    }
}

void vehicle_manager::part_note(int index, car_part part, char* output) const
{
    const record& data = _active[index].data;
    int length;
    output[0] = 0;

    switch(part)
    {

    case car_part::BATTERY:
        ui::append(output, 0, ! (data.parts & part_battery) ? "MISSING" : data.charge == 0 ? "FLAT" : "");
        break;

    case car_part::SPARK_PLUG:
        ui::append(output, 0, (data.parts & part_plug) ? "" : "MISSING");
        break;

    case car_part::RADIATOR:
        ui::append(output, 0, (data.parts & part_radiator) ? "FULL" : "EMPTY");
        break;

    case car_part::TYRES:
        length = ui::append_int(output, 0, data.tyres);
        ui::append(output, length, "/4");
        break;

    default:
        length = ui::append_int(output, 0, (data.fuel + 9) / 10);
        ui::append(output, length, " L");
        break;
    }
}

int vehicle_manager::part_work_minutes(game& owner, car_part part) const
{
    // Milestone 14: tyres go on and off in half the time with a car jack.
    return part == car_part::TYRES && owner.carries(item_id::CAR_JACK) > 0 ?
               (part_minutes * item_tuning::jack_percent) / 100 : part_minutes;
}

int vehicle_manager::minutes(game& owner, int base) const
{
    int level = owner.hero().skills().level(skill::MECHANICS);
    return bn::max((base * (100 - (level * faster_percent_per_level))) / 100, 2);
}

bool vehicle_manager::can_fit(game& owner, int index, car_part part, const char*& problem) const
{
    const record& data = _active[index].data;
    problem = nullptr;

    if(is_wreck(data.bits))
    {
        problem = "A WRECK CAN'T BE FIXED";
        return false;
    }

    bool wrench = owner.carries(item_id::WRENCH) > 0;

    switch(part)
    {

    case car_part::BATTERY:
        problem = (data.parts & part_battery) ? "ALREADY FITTED" :
                  owner.carries(item_id::CAR_BATTERY) <= 0 ? "NEEDS A CAR BATTERY" : ! wrench ? "NEEDS A WRENCH" : nullptr;
        break;

    case car_part::SPARK_PLUG:
        problem = (data.parts & part_plug) ? "ALREADY FITTED" :
                  owner.carries(item_id::SPARK_PLUG) <= 0 ? "NEEDS A SPARK PLUG" : ! wrench ? "NEEDS A WRENCH" : nullptr;
        break;

    case car_part::RADIATOR:
        problem = (data.parts & part_radiator) ? "ALREADY FULL" :
                  owner.carries(item_id::WATER_BOTTLE) <= 0 ? "NEEDS A WATER BOTTLE" : nullptr;
        break;

    case car_part::TYRES:
        problem = data.tyres >= 4 ? "ALL FOUR ARE FITTED" :
                  owner.carries(item_id::TYRE) <= 0 ? "NEEDS A TYRE" : ! wrench ? "NEEDS A WRENCH" : nullptr;
        break;

    default:
        problem = data.fuel + 10 > tank_dl ? "THE TANK IS FULL" :
                  ! fuel_can(owner, true) ? "NEEDS A FUEL CAN WITH FUEL" : nullptr;
        break;
    }

    return ! problem;
}

bool vehicle_manager::can_remove(game& owner, int index, car_part part, const char*& problem) const
{
    const record& data = _active[index].data;
    bool wrench = owner.carries(item_id::WRENCH) > 0;
    problem = nullptr;

    switch(part)
    {

    case car_part::BATTERY:
        problem = ! (data.parts & part_battery) ? "NOTHING TO TAKE OUT" : ! wrench ? "NEEDS A WRENCH" : nullptr;
        break;

    case car_part::SPARK_PLUG:
        problem = ! (data.parts & part_plug) ? "NOTHING TO TAKE OUT" : ! wrench ? "NEEDS A WRENCH" : nullptr;
        break;

    case car_part::TYRES:
        problem = data.tyres == 0 ? "NO GOOD TYRES LEFT" : ! wrench ? "NEEDS A WRENCH" : nullptr;
        break;

    case car_part::FUEL:
        problem = "USE SIPHON FUEL";
        break;

    default:
        problem = "NOTHING TO TAKE OUT";
        break;
    }

    return ! problem;
}

uint16_t vehicle_manager::_key_id(const active& car) const
{
    return car.data.origin == no_origin ? 0 : uint16_t(car.data.origin + 1);
}

int vehicle_manager::_key_slot(game& owner, const active& car) const
{
    const inventory& items = owner.hero().items();
    int own = _key_id(car);

    for(int slot = 0; slot < inventory::slots; ++slot)
    {
        const item_stack& key = items.slot(slot);

        if(key.id != item_id::CAR_KEY)
        {
            continue;
        }

        // A key with no mark, or a car that is not the seed's, fits; otherwise the key starts the cars parked near
        // where it was found.
        if(key.extra == 0 || own == 0)
        {
            return slot;
        }

        int key_block = key.extra - 1;
        int car_block = own - 1;

        if(bn::abs((key_block % blocks_per_side) - (car_block % blocks_per_side)) <= key_blocks &&
           bn::abs((key_block / blocks_per_side) - (car_block / blocks_per_side)) <= key_blocks)
        {
            return slot;
        }
    }

    return -1;
}

bool vehicle_manager::has_key(game& owner, int index) const
{
    return _key_slot(owner, _active[index]) >= 0;
}

bool vehicle_manager::hotwired(int index) const
{
    return _active[index].data.bits & bit_hotwired;
}

bool vehicle_manager::_ready(const active& car) const
{
    const record& data = car.data;
    return ! is_wreck(data.bits) && ! is_bike(data.bits) && data.condition > 0 && (data.parts & part_battery) &&
           data.charge > 0 && (data.parts & part_plug) && (data.parts & part_radiator) && data.tyres >= needed_tyres &&
           data.fuel > 0;
}

bool vehicle_manager::can_do(game& owner, int index, car_action action, const char*& problem) const
{
    const active& car = _active[index];
    const record& data = car.data;
    const character& skills = owner.hero().skills();
    problem = nullptr;

    switch(action)
    {

    case car_action::SIPHON:
        problem = data.fuel < 10 ? "THE TANK IS EMPTY" : owner.carries(item_id::HOSE) <= 0 ? "NEEDS A HOSE" :
                  ! fuel_can(owner, false) ? "NEEDS A FUEL CAN WITH ROOM" : nullptr;
        break;

    case car_action::HOTWIRE:
        problem = is_wreck(data.bits) ? "A WRECK CAN'T BE FIXED" : (data.bits & bit_hotwired) ? "ALREADY HOTWIRED" :
                  _key_slot(owner, car) >= 0 ? "YOU HAVE THE KEY" :
                  skills.level(skill::MECHANICS) < hotwire_level && skills.job() != occupation::MECHANIC &&
                  skills.job() != occupation::BURGLAR ? "NEEDS MECHANICS 2" : nullptr;
        break;

    case car_action::REPAIR:
        problem = is_wreck(data.bits) ? "A WRECK CAN'T BE FIXED" : is_bike(data.bits) || data.condition >= 100 ?
                  "NOTHING TO REPAIR" :
                  owner.carries(item_id::RADIATOR) <= 0 && owner.carries(item_id::BRAKE_PADS) <= 0 ?
                  "NEEDS A RADIATOR OR PADS" : owner.carries(item_id::WRENCH) <= 0 ? "NEEDS A WRENCH" : nullptr;
        break;

    case car_action::DRIVE:
        problem = is_wreck(data.bits) ? "A WRECK CAN'T BE FIXED" : data.condition == 0 ? "TOO DAMAGED TO DRIVE" :
                  ! (data.parts & part_battery) ? "NEEDS A BATTERY" : data.charge == 0 ? "THE BATTERY IS FLAT" :
                  ! (data.parts & part_plug) ? "NEEDS A SPARK PLUG" :
                  ! (data.parts & part_radiator) ? "THE RADIATOR IS EMPTY" :
                  data.tyres < needed_tyres ? "NEEDS 3 GOOD TYRES" : data.fuel == 0 ? "NO FUEL" :
                  ! (data.bits & bit_hotwired) && _key_slot(owner, car) < 0 ? "NEEDS THE KEY OR A HOTWIRE" : nullptr;
        break;

    default:
        break;
    }

    return ! problem;
}

void vehicle_manager::fit(game& owner, int index, car_part part)
{
    active& car = _active[index];
    const char* problem;

    if(! can_fit(owner, index, part, problem))
    {
        deny(owner, problem);
        return;
    }

    if(! _track(owner, car) || ! owner.begin_car_work(minutes(owner, part == car_part::FUEL ? fuel_minutes :
                                                                     part_work_minutes(owner, part))))
    {
        return;
    }

    inventory& items = owner.hero().items();
    _job = job::FIT;
    _job_part = part;
    _job_vehicle = index;
    _job_amount = 0;

    switch(part)
    {

    case car_part::BATTERY:
    {
        // A battery remembers its charge (kept one higher, so a found one, with nothing kept, reads as new).
        int slot = items.find(item_id::CAR_BATTERY);
        int kept = items.slot(slot).extra;
        _job_amount = kept ? kept - 1 : loot_charge;
        items.consume_one(slot);
        break;
    }

    case car_part::SPARK_PLUG:
        items.remove(item_id::SPARK_PLUG, 1);
        break;

    case car_part::RADIATOR:
        items.remove(item_id::WATER_BOTTLE, 1);
        break;

    case car_part::TYRES:
        items.remove(item_id::TYRE, 1);
        break;

    default:
    {
        if(item_stack* can = fuel_can(owner, true))
        {
            int litres = bn::min(int(can->amount), (tank_dl - car.data.fuel) / 10);
            can->amount = uint8_t(can->amount - litres);
            _job_amount = litres;
        }

        break;
    }
    }
}

void vehicle_manager::remove(game& owner, int index, car_part part)
{
    active& car = _active[index];
    const char* problem;

    if(! can_remove(owner, index, part, problem))
    {
        deny(owner, problem);
        return;
    }

    if(! _track(owner, car) || ! owner.begin_car_work(minutes(owner, part_work_minutes(owner, part))))
    {
        return;
    }

    _job = job::REMOVE;
    _job_part = part;
    _job_vehicle = index;
}

void vehicle_manager::act(game& owner, int index, car_action action)
{
    active& car = _active[index];
    const char* problem;

    if(! can_do(owner, index, action, problem))
    {
        deny(owner, problem);
        return;
    }

    if(action == car_action::DRIVE)
    {
        enter(owner, index);
        return;
    }

    // Milestone 14: a radiator (or else brake pads) used up, condition back when the work is done.
    if(action == car_action::REPAIR)
    {
        if(! _track(owner, car) || ! owner.begin_car_work(minutes(owner, part_minutes)))
        {
            return;
        }

        inventory& items = owner.hero().items();
        bool radiator = items.count(item_id::RADIATOR) > 0;
        items.remove(radiator ? item_id::RADIATOR : item_id::BRAKE_PADS, 1);
        _job = job::REPAIR;
        _job_vehicle = index;
        _job_amount = radiator ? item_tuning::radiator_repair : item_tuning::brake_pads_repair;
        return;
    }

    if(action != car_action::SIPHON && action != car_action::HOTWIRE)
    {
        return;
    }

    bool siphon = action == car_action::SIPHON;
    int base = siphon ? siphon_minutes : hotwire_minutes;

    if(! _track(owner, car) || ! owner.begin_car_work(minutes(owner, base)))
    {
        return;
    }

    _job = siphon ? job::SIPHON : job::HOTWIRE;
    _job_vehicle = index;
}

int vehicle_manager::finish_work(game& owner)
{
    if(_job == job::NONE || _job_vehicle < 0 || ! _active[_job_vehicle].live)
    {
        _job = job::NONE;
        return -1;
    }

    active& car = _active[_job_vehicle];
    record& data = car.data;
    char text[32];
    int length;
    int xp = part_xp;

    switch(_job)
    {

    case job::FIT:
        switch(_job_part)
        {

        case car_part::BATTERY:
            data.parts = uint8_t(data.parts | part_battery);
            data.charge = uint8_t(_job_amount);
            break;

        case car_part::SPARK_PLUG:
            data.parts = uint8_t(data.parts | part_plug);
            break;

        case car_part::RADIATOR:
            data.parts = uint8_t(data.parts | part_radiator);
            xp = part_xp / 3;
            break;

        case car_part::TYRES:
            ++data.tyres;
            break;

        default:
            data.fuel = uint16_t(bn::min(data.fuel + (_job_amount * 10), tank_dl));
            xp = 0;
            break;
        }

        if(_job_part == car_part::FUEL)
        {
            length = ui::append_int(text, 0, _job_amount);
            ui::append(text, length, " L OF FUEL ADDED");
        }
        else
        {
            length = ui::append(text, 0, part_names[int(_job_part)]);
            ui::append(text, length, _job_part == car_part::RADIATOR ? " FILLED" : " FITTED");
        }

        sfx::play(_job_part == car_part::FUEL || _job_part == car_part::RADIATOR ? sfx::id::GULP : sfx::id::HAMMER,
                  milli(700));
        break;

    case job::REMOVE:
    {
        item_stack part = make_stack(_job_part == car_part::BATTERY ? item_id::CAR_BATTERY :
                                     _job_part == car_part::SPARK_PLUG ? item_id::SPARK_PLUG : item_id::TYRE);

        if(_job_part == car_part::BATTERY)
        {
            data.parts = uint8_t(data.parts & ~part_battery);
            data.bits = uint8_t(data.bits & ~(bit_lights | bit_alarm));
            part.extra = uint16_t(data.charge + 1);

            if(_alarm_slot == car.slot)
            {
                _stop_alarm();
            }
        }
        else if(_job_part == car_part::SPARK_PLUG)
        {
            data.parts = uint8_t(data.parts & ~part_plug);
        }
        else
        {
            --data.tyres;
        }

        give(owner, part);
        length = ui::append(text, 0, part_names[int(_job_part)]);
        ui::append(text, length, " TAKEN OUT");
        sfx::play(sfx::id::PICKUP, milli(800));
        break;
    }

    case job::REPAIR:
        data.condition = uint8_t(bn::min(int(data.condition) + _job_amount, 100));
        length = ui::append(text, 0, "REPAIRED: ");
        length = ui::append_int(text, length, data.condition);
        ui::append(text, length, "%");
        sfx::play(sfx::id::HAMMER, milli(700));
        break;

    case job::SIPHON:
    {
        int litres = 0;

        while(item_stack* can = fuel_can(owner, false))
        {
            int moved = bn::min(info(can->id).durability - can->amount, data.fuel / 10);

            if(moved <= 0)
            {
                break;
            }

            can->amount = uint8_t(can->amount + moved);
            data.fuel = uint16_t(data.fuel - (moved * 10));
            litres += moved;
        }

        length = ui::append_int(text, 0, litres);
        ui::append(text, length, " L SIPHONED");
        xp = part_xp / 3;
        sfx::play(sfx::id::GULP, milli(700));
        break;
    }

    default:
    {
        // Hotwiring: noisy, and it can fail (spec).
        const character& skills = owner.hero().skills();
        bool trade = skills.job() == occupation::MECHANIC || skills.job() == occupation::BURGLAR;
        int chance = hotwire_percent + (skills.level(skill::MECHANICS) * hotwire_percent_per_level) +
                     (trade ? hotwire_job_percent : 0);
        bool worked = owner.random().get_int(100) < chance;
        owner.noise(position(_job_vehicle), hotwire_noise);

        if(worked)
        {
            data.bits = uint8_t(data.bits | bit_hotwired);
        }

        ui::append(text, 0, worked ? "HOTWIRED" : "THE HOTWIRING FAILED");
        xp = worked ? hotwire_xp : hotwire_xp / 3;
        sfx::play(worked ? sfx::id::SWAP : sfx::id::DENIED, milli(800));
        break;
    }
    }

    _commit(car);
    owner.toast(text, hud_tiles::bank_accent, 120);

    if(xp > 0)
    {
        owner.hero().train(owner, skill::MECHANICS, xp);
    }

    int vehicle = _job_vehicle;
    _job = job::NONE;
    _job_vehicle = -1;
    return vehicle;
}

int vehicle_manager::container_of(game& owner, int index, bool trunk)
{
    active& car = _active[index];

    if(is_bike(car.data.bits))
    {
        return -1;
    }

    int tile_x;
    int tile_y;

    if(car.data.origin != no_origin)
    {
        tile_x = container_tile_x - (trunk ? 0 : 1) - ((car.data.origin >> 15) * 2);
        tile_y = car.data.origin & 0x7FFF;
    }
    else
    {
        if(! _track(owner, car))
        {
            return -1;
        }

        tile_x = spawned_tile_x - (trunk ? 0 : 1);
        tile_y = car.slot;
    }

    container_store& store = owner.containers();
    int found = store.at(tile_x, tile_y);

    if(found < 0)
    {
        found = store.add(container_kind::VEHICLE, trunk ? loot_table::CAR_TRUNK : loot_table::GLOVE_BOX, tile_x,
                          tile_y);
    }

    return found;
}

void vehicle_manager::_stop_alarm()
{
    _alarm_frames = 0;
    _alarm_slot = -1;
}

void vehicle_manager::opened(game& owner, int index)
{
    active& car = _active[index];

    // An alarm goes off once; it needs remembering, so not on a wreck that can't be tracked any more.
    if(! (car.data.bits & bit_alarm) || ! (car.data.parts & part_battery) || ! _track(owner, car, true))
    {
        return;
    }

    car.data.bits = uint8_t(car.data.bits & ~bit_alarm);
    _commit(car);
    _alarm_frames = alarm_frames;
    _alarm_position = position(index);
    _alarm_slot = car.slot;
    owner.toast("THE CAR ALARM GOES OFF", hud_tiles::bank_danger, 150);
}

// ---------------------------------------------------------------------------------------------------------------------
// Riding and driving.
// ---------------------------------------------------------------------------------------------------------------------

bool vehicle_manager::mount(game& owner, int index)
{
    active& bike = _active[index];

    if(! is_bike(bike.data.bits) || ! _track(owner, bike))
    {
        return false;
    }

    _ridden = index;
    owner.hero().set_ride(ride_kind::BICYCLE);
    sfx::play(sfx::id::CHAIN, milli(600));
    return true;
}

void vehicle_manager::enter(game& owner, int index)
{
    active& car = _active[index];

    if(is_bike(car.data.bits) || ! _track(owner, car))
    {
        return;
    }

    _driven = index;
    _position = bn::fixed_point(car.data.x, car.data.y);
    _velocity = bn::fixed_point();
    _heading = bn::fixed(int(car.data.heading));
    _engine = false;
    _stall_timer = 0;
    _recent_hits = 0;
    _obstacles_dirty = true;
    owner.hero().set_ride(ride_kind::CAR);
    owner.hero().carried(_position, _velocity, 0);

    if(_ready(car))
    {
        _start_engine(owner, car);
    }
}

void vehicle_manager::_start_engine(game& owner, active& car)
{
    record& data = car.data;

    if(! _ready(car))
    {
        deny(owner, data.fuel == 0 ? "OUT OF FUEL" : data.condition == 0 ? "THE CAR IS WRECKED" : "IT WON'T START");
        return;
    }

    if(data.charge < start_charge)
    {
        data.charge = 0;
        _commit(car);
        deny(owner, "THE BATTERY IS FLAT");
        return;
    }

    data.charge = uint8_t(data.charge - start_charge);
    _engine = true;
    sfx::play(sfx::id::STARTER, milli(800));
    owner.noise(_position, engine_noise);
}

bool vehicle_manager::leave(game& owner, bool place_player)
{
    player& hero = owner.hero();

    if(_ridden >= 0)
    {
        active& bike = _active[_ridden];
        bike.data.x = uint16_t(bn::clamp(hero.position().x().integer(), 0, 65535));
        bike.data.y = uint16_t(bn::clamp(hero.position().y().integer() - 4, 0, 65535));
        _commit(bike);
        _ridden = -1;

        if(hero.ride() == ride_kind::BICYCLE)
        {
            hero.set_ride(ride_kind::NONE);
        }

        return true;
    }

    if(_driven < 0)
    {
        return true;
    }

    active& car = _active[_driven];
    int half_x;
    int half_y;
    extents(_heading, half_x, half_y);
    int centre_x = _position.x().integer();
    int centre_y = _position.y().integer();

    if(place_player)
    {
        // Out of the driver's door if there is room, else the other side, then behind or in front.
        bn::fixed_point forward = forward_of(_heading);
        bn::fixed_point side(-forward.y(), forward.x());
        bn::fixed_point spot;
        bool found = false;

        for(int reach = 0; reach < 3 && ! found; ++reach)
        {
            int side_px = half_width + 9 + (reach * 6);
            int end_px = half_length + 9 + (reach * 6);
            const bn::fixed_point offsets[] = { side * -side_px, side * side_px, forward * -end_px, forward * end_px };

            for(const bn::fixed_point& offset : offsets)
            {
                bn::fixed_point feet = _position + offset + bn::fixed_point(0, 3);
                int x = feet.x().integer();
                int y = feet.y().integer();
                bool in_car = x + 4 >= centre_x - half_x && x - 4 <= centre_x + half_x && y >= centre_y - half_y &&
                              y - 6 <= centre_y + half_y;

                if(! in_car && ! world::box_blocked(x - 4, y - 6, x + 4, y))
                {
                    spot = feet;
                    found = true;
                    break;
                }
            }
        }

        if(! found)
        {
            deny(owner, "NO ROOM TO GET OUT");
            return false;
        }

        hero.teleport(spot);
    }

    car.data.x = uint16_t(bn::clamp(centre_x, 0, 65535));
    car.data.y = uint16_t(bn::clamp(centre_y, 0, 65535));
    car.data.heading = uint8_t(_heading.round_integer() & (headings - 1));
    _commit(car);
    _driven = -1;
    _engine = false;
    _velocity = bn::fixed_point();
    hero.set_ride(ride_kind::NONE);

    // The parked car is in the way again; zombies it stopped on are moved clear.
    _rebuild_obstacles(owner);
    extents(int(car.data.heading), half_x, half_y);
    owner.zombies().push_out(car.data.x - half_x, car.data.y - half_y, car.data.x + half_x, car.data.y + half_y);
    return true;
}

void vehicle_manager::halt()
{
    _velocity = bn::fixed_point();
    _engine = false;
}

bool vehicle_manager::_car_blocked(const bn::fixed_point& centre, bn::fixed heading) const
{
    int half_x;
    int half_y;
    extents(heading, half_x, half_y);
    int x = centre.x().integer();
    int y = centre.y().integer();

    #if KX_BENCH == 7 || KX_BENCH == 8
        // Travel and save benches go through everything (as the player does there).
        static_cast<void>(x + y);
        return false;
    #else
        return world::box_blocked(x - half_x, y - half_y, x + half_x, y + half_y);
    #endif
}

void vehicle_manager::_crash(game& owner, active& car, bn::fixed impact)
{
    if(impact < milli(crash_milli) || _crash_cooldown > 0)
    {
        return;
    }

    _crash_cooldown = 20;
    record& data = car.data;
    int damage = ((impact - milli(crash_milli)) * crash_damage_per_px).round_integer() + 1;
    data.condition = uint8_t(bn::max(data.condition - damage, 0));
    sfx::play(sfx::id::CRASH, bn::min(impact / 4, bn::fixed(1)));
    owner.shake(bn::min(impact.integer() + 1, 3), 12);
    owner.noise(_position, crash_noise);

    // Spec: crashes can cause fractures or deep wounds.
    if(impact >= milli(injury_milli) && ! owner.god_mode() && owner.random().get_int(100) < injury_percent)
    {
        bool fracture = owner.random().get_int(100) < fracture_percent;
        constexpr body_part parts[] = { body_part::HEAD, body_part::TORSO, body_part::LEFT_ARM, body_part::RIGHT_ARM };
        body_part part = fracture ? (owner.random().get_int(2) ? body_part::LEFT_LEG : body_part::RIGHT_LEG) :
                                    parts[owner.random().get_int(4)];
        owner.hero().body().add_wound(part, fracture ? wound_type::FRACTURE : wound_type::DEEP_WOUND, false,
                                      owner.random());
        owner.toast(fracture ? "CRASH: FRACTURE" : "CRASH: DEEP WOUND", hud_tiles::bank_danger, 120);
    }

    if(data.condition == 0)
    {
        // Wrecked for good.
        data.bits = uint8_t((data.bits | bit_wreck) & ~(bit_lights | bit_hotwired));
        _engine = false;
        _velocity = bn::fixed_point();
        owner.toast("THE CAR IS WRECKED", hud_tiles::bank_danger, 150);
    }
}

void vehicle_manager::_drive(game& owner, const input_frame& input)
{
    active& car = _active[_driven];
    record& data = car.data;

    if(_crash_cooldown > 0)
    {
        --_crash_cooldown;
    }

    if(_stall_timer > 0)
    {
        --_stall_timer;
    }

    if(_frame % stall_decay_frames == 0 && _recent_hits > 0)
    {
        --_recent_hits;
    }

    // A starts a stopped engine; R1 switches the headlights.
    if(! _engine && input.a_pressed && _stall_timer <= 0)
    {
        _start_engine(owner, car);
    }

    if(input.r_pressed && (data.parts & part_battery) && data.charge > 0)
    {
        data.bits = uint8_t(data.bits ^ bit_lights);
        sfx::play(sfx::id::SWAP, milli(500));
    }

    bn::fixed_point forward = forward_of(_heading);
    bn::fixed along = dot(_velocity, forward);
    bn::fixed_point sideways = _velocity - (forward * along);

    // Top speed and grip from the ground under the car, and from its condition.
    terrain ground = world::terrain_at(_position.x().integer() >> world::tile_shift,
                                       _position.y().integer() >> world::tile_shift);
    bool paved = ground == terrain::ROAD || ground == terrain::PAVEMENT || ground == terrain::FLOOR;
    int percent = paved ? 100 : ground == terrain::SHALLOW_WATER ? water_percent :
                  ground == terrain::FOREST || ground == terrain::MUD || ground == terrain::CROPS ? mud_percent :
                                                                                                    offroad_percent;

    if(data.condition < worn_condition)
    {
        percent = (percent * worn_percent) / 100;
    }

    bn::fixed top = (milli(top_milli) * percent) / 100;

    if(_engine && input.a_held && along < top)
    {
        along = bn::min(along + milli(accel_milli), top);
    }
    else if(input.b_held)
    {
        // Brakes, then reverse.
        if(along > milli(50))
        {
            along = bn::max(along - milli(brake_milli), bn::fixed(0));
        }
        else if(_engine)
        {
            along = bn::max(along - milli(accel_milli), -bn::min(milli(reverse_milli), top));
        }
    }
    else
    {
        along = (along * coast_256) / 256;

        if(bn::abs(along) < milli(20))
        {
            along = 0;
        }
    }

    if(along > top)
    {
        along = bn::max(along - milli(slow_milli), top);
    }

    sideways = (sideways * (paved ? grip_road_256 : grip_rough_256)) / 256;

    // Steering: none at a standstill, full from a moderate speed, reversed when backing up.
    if(input.dx && along != 0)
    {
        bn::fixed effect = bn::min(bn::abs(along) / milli(turn_full_milli), bn::fixed(1));
        bn::fixed turned = _heading + ((bn::fixed(turn_256) / 256) * effect * input.dx * (along < 0 ? -1 : 1));

        if(turned < 0)
        {
            turned += headings;
        }
        else if(turned >= headings)
        {
            turned -= headings;
        }

        // The turned car must still fit where it is.
        if(! _car_blocked(_position, turned))
        {
            _heading = turned;
            forward = forward_of(_heading);
        }
    }

    _velocity = (forward * along) + sideways;

    // Movement, an axis at a time; what it hits bounces it back and damages it (spec).
    bn::fixed impact;
    bn::fixed_point next = _position + bn::fixed_point(_velocity.x(), 0);

    if(_velocity.x() != 0)
    {
        if(_car_blocked(next, _heading))
        {
            impact = bn::abs(_velocity.x());
            _velocity.set_x((-_velocity.x() * bounce_256) / 256);
        }
        else
        {
            _position = next;
        }
    }

    next = _position + bn::fixed_point(0, _velocity.y());

    if(_velocity.y() != 0)
    {
        if(_car_blocked(next, _heading))
        {
            impact = bn::max(impact, bn::abs(_velocity.y()));
            _velocity.set_y((-_velocity.y() * bounce_256) / 256);
        }
        else
        {
            _position = next;
        }
    }

    if(impact > 0)
    {
        _crash(owner, car, impact);
    }

    // Zombies in the way (spec: a thump with hitstop and shake; it damages the car and slows it).
    bn::fixed speed = approx_length(_velocity);

    if(++_ram_timer >= ram_frames)
    {
        _ram_timer = 0;
        ++_ram_id;
    }

    if(speed >= milli(ram_milli))
    {
        attack_info ram;
        ram.origin = _position + bn::fixed_point(0, 4);
        ram.direction = normalized(_velocity);
        ram.reach = ram_reach_px;
        ram.arc_degrees = 200;
        ram.max_targets = 6;
        ram.damage = speed >= milli(ram_kill_milli) ? 100 : (speed * 30).integer();
        ram.knockback = speed * 2;
        ram.stagger_frames = 40;
        ram.swing_id = _ram_id;
        ram.weapon_used = weapon::BAT;
        attack_result result = owner.zombies().attack(owner, ram);

        if(result.hits > 0)
        {
            for(int hit = 0; hit < result.hits; ++hit)
            {
                _velocity = (_velocity * ram_slow_256) / 256;
            }

            data.condition = uint8_t(bn::max(data.condition - (result.hits * ram_damage), 1));
            _recent_hits += result.hits;
            owner.request_hitstop(3);
            owner.shake(2, 6);
            sfx::play(sfx::id::HIT_BLUNT, 1);

            // Ploughing into a crowd can stall it.
            if(_recent_hits >= stall_hits && _engine)
            {
                _engine = false;
                _stall_timer = stall_frames;
                _recent_hits = 0;
                _velocity = _velocity / 3;
                owner.toast("THE ENGINE STALLED", hud_tiles::bank_danger, 90);
                sfx::play(sfx::id::DENIED, milli(700));
            }
        }
    }

    // Engine: heard, burning fuel, charging the battery.
    if(_engine)
    {
        if(_frame % engine_noise_frames == 0)
        {
            owner.noise(_position, engine_noise);
        }

        if(_frame % engine_sound_frames == 0)
        {
            sfx::play(sfx::id::ENGINE, bn::min(milli(250) + (speed / 12), milli(700)));
        }

        _fuel_burn += fuel_idle + (speed * fuel_per_px).integer();

        while(_fuel_burn >= fuel_unit && data.fuel > 0)
        {
            _fuel_burn -= fuel_unit;
            --data.fuel;
        }

        if(data.fuel == 0)
        {
            _engine = false;
            owner.toast("OUT OF FUEL", hud_tiles::bank_danger, 120);
            sfx::play(sfx::id::DENIED, milli(700));
        }

        if(_frame % charge_frames == 0 && data.charge < 100)
        {
            ++data.charge;
        }
    }

    data.x = uint16_t(bn::clamp(_position.x().integer(), 0, 65535));
    data.y = uint16_t(bn::clamp(_position.y().integer(), 0, 65535));
    data.heading = uint8_t(_heading.round_integer() & (headings - 1));
    _commit(car);
    owner.hero().carried(_position + bn::fixed_point(0, 4), _velocity, _engine ? engine_noise : 0);

    // L1 gets out once the car has all but stopped.
    if(input.l_pressed)
    {
        if(speed > milli(300))
        {
            deny(owner, "STOP THE CAR FIRST");
        }
        else
        {
            static_cast<void>(leave(owner, true));
        }
    }
}

void vehicle_manager::update(game& owner, const input_frame& input)
{
    ++_frame;
    player& hero = owner.hero();

    // The survivor got or fell off the bicycle: it stays where that happened.
    if(_ridden >= 0)
    {
        if(hero.ride() != ride_kind::BICYCLE)
        {
            static_cast<void>(leave(owner, false));
        }
        else
        {
            record& data = _active[_ridden].data;
            data.x = uint16_t(bn::clamp(hero.position().x().integer(), 0, 65535));
            data.y = uint16_t(bn::clamp(hero.position().y().integer() - 4, 0, 65535));
        }
    }

    if(_driven >= 0)
    {
        _drive(owner, input);
    }

    _scan(owner);

    // A loaded game: back in the car or on the bicycle the survivor was using.
    if(_resume_slot >= 0)
    {
        int index = _find_active(no_origin, _resume_slot);

        if(index >= 0)
        {
            _resume_slot = -1;

            if(is_bike(_active[index].data.bits))
            {
                static_cast<void>(mount(owner, index));
            }
            else
            {
                enter(owner, index);
            }
        }
    }

    // An alarm wails and draws every horde around (spec: noise radius 60).
    if(_alarm_frames > 0)
    {
        if(_alarm_frames % alarm_sound_frames == 0)
        {
            sfx::play(sfx::id::ALARM, milli(800), owner.screen_x(_alarm_position));
            owner.noise(_alarm_position, alarm_noise);
        }

        if(--_alarm_frames == 0)
        {
            _stop_alarm();
        }
    }

    // Headlights left on drain the battery while the engine is off (spec).
    if(_frame % lights_drain_frames == 0)
    {
        for(int index = 0; index < max_active; ++index)
        {
            active& value = _active[index];

            if(value.live && (value.data.bits & bit_lights) && ! (index == _driven && _engine))
            {
                if(value.data.charge > 0)
                {
                    --value.data.charge;
                }

                if(value.data.charge == 0)
                {
                    value.data.bits = uint8_t(value.data.bits & ~bit_lights);
                }

                _commit(value);
            }
        }
    }

    if(_obstacles_dirty)
    {
        _rebuild_obstacles(owner);
    }
}

void vehicle_manager::update_visuals(game& owner, int camera_left, int camera_top)
{
    const player& hero = owner.hero();

    for(int index = 0; index < max_active; ++index)
    {
        active& value = _active[index];

        if(! value.live)
        {
            continue;
        }

        bool bike = is_bike(value.data.bits);
        int x = value.data.x;
        int y = value.data.y;
        int frame;
        bool flip = false;

        if(bike)
        {
            frame = vehicle_frames::bike_side;

            if(index == _ridden)
            {
                x = hero.position().x().floor_integer();
                y = hero.position().y().floor_integer() - 5;
                frame = hero.facing_x() ? vehicle_frames::bike_side : vehicle_frames::bike_end_on;
                flip = hero.facing_x() < 0;
            }
        }
        else
        {
            if(index == _driven)
            {
                x = _position.x().floor_integer();
                y = _position.y().floor_integer();
            }

            // 32 headings onto 16 directions; the 9 drawn run from north round to south, the rest are mirrored.
            int heading = index == _driven ? _heading.round_integer() : value.data.heading;
            int direction = ((heading + 1) >> 1) & (vehicle_frames::car_directions - 1);
            int drawn = (direction - 12) & (vehicle_frames::car_directions - 1);

            if(drawn >= vehicle_frames::car_drawn)
            {
                drawn = ((8 - direction) - 12) & (vehicle_frames::car_directions - 1);
                flip = true;
            }

            int variant = is_wreck(value.data.bits) ? vehicle_frames::car_wreck + wreck_look(value.data.origin) :
                                                      (value.data.bits & variant_mask) >> variant_shift;

            if(mossy(value.data.origin))
            {
                variant += vehicle_frames::car_mossy;
            }

            frame = (variant * vehicle_frames::car_drawn) + drawn;
        }

        bool visible = x >= camera_left - view_margin && x <= camera_left + 240 + view_margin &&
                       y >= camera_top - view_margin && y <= camera_top + 160 + view_margin;

        if(! visible)
        {
            value.sprite.reset();
            value.frame = -1;
            continue;
        }

        if(! value.sprite)
        {
            bn::sprite_ptr sprite = bike ? bn::sprite_items::bike.create_sprite(x, y, frame) :
                                           bn::sprite_items::car.create_sprite(x, y, frame);
            sprite.set_camera(_camera);
            sprite.set_bg_priority(sprite_bg_priority);
            value.sprite = bn::move(sprite);
            value.frame = int16_t(frame);
        }
        else if(frame != value.frame)
        {
            value.frame = int16_t(frame);
            value.sprite->set_tiles(bike ? bn::sprite_items::bike.tiles_item() : bn::sprite_items::car.tiles_item(),
                                    frame);
        }

        // A bicycle draws under whoever rides it; a car sorts by its lower edge.
        int z_order = bike ? depth_z(y + (index == _ridden ? 5 : 2), camera_top) + (index == _ridden ? 1 : 0) :
                             depth_z(y + 6, camera_top);
        value.sprite->set_position(x, y);
        light::shade(*value.sprite);
        value.sprite->set_horizontal_flip(flip);
        value.sprite->set_z_order(z_order);
    }
}

int vehicle_manager::spawn(game& owner, vehicle_kind which)
{
    const bn::fixed_point& feet = owner.hero().position();
    record data;
    data.origin = no_origin;
    data.heading = 0;
    data.bits = bit_used;
    data.condition = 100;
    data.charge = 100;

    // Beside the player: the first free spot east, west, south or north.
    constexpr int offsets[][2] = { { 26, -4 }, { -26, -4 }, { 0, 20 }, { 0, -28 } };
    bool placed = false;

    for(const auto& offset : offsets)
    {
        int x = feet.x().integer() + offset[0];
        int y = feet.y().integer() + offset[1];

        if(which == vehicle_kind::BICYCLE ? ! world::box_blocked(x - 4, y - 4, x + 4, y + 4) :
                                           ! world::box_blocked(x - half_length, y - half_width, x + half_length,
                                                                y + half_width))
        {
            data.x = uint16_t(bn::clamp(x, 0, 65535));
            data.y = uint16_t(bn::clamp(y, 0, 65535));
            placed = true;
            break;
        }
    }

    if(! placed)
    {
        deny(owner, "NO ROOM HERE");
        return -1;
    }

    if(which == vehicle_kind::BICYCLE)
    {
        data.bits = uint8_t(data.bits | bit_bike);
    }
    else
    {
        data.bits = uint8_t(data.bits | bit_hotwired |
                            (owner.random().get_int(vehicle_frames::car_colours) << variant_shift));
        data.parts = uint8_t(part_battery | part_plug | part_radiator);
        data.tyres = 4;
        data.fuel = tank_dl / 2;
    }

    int index = _activate(data, -1);

    if(index < 0 || ! _track(owner, _active[index]))
    {
        if(index >= 0)
        {
            _active[index] = active();
        }

        deny(owner, "TOO MANY VEHICLES HERE");
        return -1;
    }

    return index;
}

void vehicle_manager::save(save::writer& out) const
{
    out.put(_tracked);
    int occupied = _driven >= 0 ? _active[_driven].slot : _ridden >= 0 ? _active[_ridden].slot : -1;
    out.put(occupied);
    int reserved = 0;
    out.put(reserved);
}

void vehicle_manager::load(save::reader& in)
{
    reset();
    in.get(_tracked);
    _resume_slot = in.get<int>();
    static_cast<void>(in.get<int>());
}

}
