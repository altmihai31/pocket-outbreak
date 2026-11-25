#include "kx_build_mode.h"

#include "bn_sprite_items_fx.h"

#include "kx_farm.h"
#include "kx_game.h"
#include "kx_power.h"
#include "kx_traps.h"
#include "kx_hud_tiles.h"
#include "kx_input.h"
#include "kx_sfx.h"
#include "kx_ui.h"
#include "kx_world.h"

namespace kx
{

using namespace base_tuning;

namespace
{
    // Milestone 14: a tarp does for a rain collector's garbage bag (used after the bags).
    [[nodiscard]] int material_count(const inventory& items, item_id id)
    {
        return items.count(id) + (id == item_id::GARBAGE_BAG ? items.count(item_id::TARP) : 0);
    }

    void remove_material(inventory& items, item_id id, int amount)
    {
        int first = bn::min(items.count(id), amount);
        items.remove(id, first);

        if(amount > first && id == item_id::GARBAGE_BAG)
        {
            items.remove(item_id::TARP, amount - first);
        }
    }

    // Sheet metal reinforces a barricade without the Carpentry level (one sheet each time).
    [[nodiscard]] bool can_reinforce(const player& hero)
    {
        return hero.skills().level(skill::CARPENTRY) >= reinforce_level ||
               hero.items().count(item_id::SHEET_METAL) > 0;
    }
}

namespace
{
    constexpr int corner_yellow = 17;           // fx frames (kx_fx: after the edge arrows and the flames)
    constexpr int corner_red = 18;
    constexpr int corner_bg_priority = 1;
    constexpr int fuel_frames = 60;             // adding fuel to a campfire

    [[nodiscard]] bool wallish(world_gfx gfx)
    {
        bool side;
        return gfx == world_gfx::WALL_FACE || gfx == world_gfx::WALL_TOP || gfx == world_gfx::WINDOW_FACE ||
               gfx == world_gfx::WINDOW_TOP || gfx == world_gfx::BARRICADE_FACE || gfx == world_gfx::BARRICADE_TOP ||
               gfx == world_gfx::BROKEN_FACE || gfx == world_gfx::BROKEN_TOP ||
               world::door_of(gfx, side) != world::door_state::NONE;
    }

    // A doorway: an open tile between two wall tiles, side by side (front wall) or one above the other (side wall).
    [[nodiscard]] bool doorway(int x, int y, covered& covers)
    {
        // A door in it (any state, milestone 12.7) is boarded over; the boards take its place.
        bool side;

        if(world::door_of(world::wall_gfx(x, y), side) != world::door_state::NONE)
        {
            covers = side ? covered::HUNG_DOOR_TOP : covered::HUNG_DOOR_FACE;
            return true;
        }

        if(world::wall_gfx(x, y) != world_gfx::EMPTY || world::solid(x, y))
        {
            return false;
        }

        if(wallish(world::wall_gfx(x - 1, y)) && wallish(world::wall_gfx(x + 1, y)))
        {
            covers = covered::DOOR_FACE;
            return true;
        }

        if(wallish(world::wall_gfx(x, y - 1)) && wallish(world::wall_gfx(x, y + 1)))
        {
            covers = covered::DOOR_TOP;
            return true;
        }

        return false;
    }

    [[nodiscard]] int feet_tile_x(const player& hero)
    {
        return hero.position().x().integer() >> world::tile_shift;
    }

    [[nodiscard]] int feet_tile_y(const player& hero)
    {
        return (hero.position().y().integer() - 1) >> world::tile_shift;
    }

    // A tool carried or in hand.
    [[nodiscard]] int carried(const inventory& items, item_id tool)
    {
        return items.count(tool) + (items.equipped(equip_slot::HAND).id == tool);
    }

    // Ground a plot can be dug in: open earth, not roads, floors, sand or water.
    [[nodiscard]] bool diggable(terrain ground)
    {
        return ground == terrain::GRASS || ground == terrain::SOIL || ground == terrain::MUD ||
               ground == terrain::CROPS;
    }

    void segment(hud::build_line& line, const char* text, int colour)
    {
        if(line.count < hud::build_line::max_segments)
        {
            ui::append(line.text[line.count], 0, text);
            line.colour[line.count] = uint8_t(colour);
            ++line.count;
        }
    }
}

void build_mode::open(game& owner, bool farming)
{
    const player& hero = owner.hero();
    _active = true;
    _hold = 0;

    if(farming)
    {
        _piece = int(piece_kind::PLOT);
    }
    _cursor_x = feet_tile_x(hero) + hero.facing_x();
    _cursor_y = feet_tile_y(hero) + hero.facing_y();

    for(int corner = 0; corner < 4; ++corner)
    {
        bn::sprite_ptr sprite = bn::sprite_items::fx.create_sprite(0, 0, corner_yellow);
        sprite.set_horizontal_flip(corner & 1);
        sprite.set_vertical_flip(corner & 2);
        sprite.set_bg_priority(corner_bg_priority);
        sprite.set_camera(owner.camera());
        _corners[corner] = bn::move(sprite);
    }

    sfx::play(sfx::id::SWAP, milli(500));
}

void build_mode::close(game& owner)
{
    if(! _active)
    {
        return;
    }

    _active = false;
    _hold = 0;

    for(bn::optional<bn::sprite_ptr>& corner : _corners)
    {
        corner.reset();
    }

    owner.hud_ref().set_build(nullptr);
}

build_mode::target build_mode::_check(game& owner) const
{
    target result;
    const player& hero = owner.hero();
    piece_kind kind = piece_kind(_piece);
    const piece_info& piece = info(kind);
    int x = _cursor_x;
    int y = _cursor_y;
    world_gfx wall = world::wall_gfx(x, y);
    piece_kind existing;
    bool has_piece = base::find(x, y, existing);

    if(! world::loaded(x, y))
    {
        result.problem = "TOO FAR";
        return result;
    }

    if(hero.skills().level(skill::CARPENTRY) < piece.carpentry)
    {
        result.problem = piece.carpentry == 1 ? "NEEDS CARPENTRY 1" : piece.carpentry == 2 ? "NEEDS CARPENTRY 2" :
                                                                                         "NEEDS CARPENTRY 3";
        return result;
    }

    if(piece.needs != magazine::NONE && ! hero.skills().knows(piece.needs))
    {
        result.problem = piece.needs == magazine::TRAPPING ? "READ TRAPPING MAGAZINE" : "READ GENERATOR MAGAZINE";
        return result;
    }

    switch(kind)
    {

    case piece_kind::BARRICADE:
    {
        if(has_piece && existing == piece_kind::BARRICADE)
        {
            int full;
            int health = base::barricade_health(x, y, full);
            bool reinforce = full < reinforced_health && can_reinforce(hero);

            if(health >= full && ! reinforce)
            {
                result.problem = "ALREADY BARRICADED";
            }

            result.repair = true;
        }
        else if(wall == world_gfx::WINDOW_FACE || wall == world_gfx::WINDOW_TOP)
        {
            result.covers = wall == world_gfx::WINDOW_FACE ? covered::WINDOW_FACE : covered::WINDOW_TOP;
        }
        else if(wall == world_gfx::BROKEN_FACE || wall == world_gfx::BROKEN_TOP)
        {
            result.covers = wall == world_gfx::BROKEN_FACE ? covered::BROKEN_FACE : covered::BROKEN_TOP;
        }
        else if(! doorway(x, y, result.covers))
        {
            result.problem = "NOT A WINDOW OR DOOR";
        }
        else if(x == feet_tile_x(hero) && y == feet_tile_y(hero))
        {
            result.problem = "STAND ASIDE";
        }
        break;
    }

    case piece_kind::WOODEN_DOOR:
    {
        // In a doorway with no door, or a broken one (milestone 12.7).
        bool side_wall;
        world::door_state door = world::door_of(wall, side_wall);
        covered ignored;

        if(has_piece)
        {
            result.problem = "ALREADY BUILT ON";
        }
        else if(door == world::door_state::BROKEN)
        {
            result.covers = side_wall ? covered::DOOR_TOP : covered::DOOR_FACE;
        }
        else if(door != world::door_state::NONE || ! doorway(x, y, ignored))
        {
            result.problem = door != world::door_state::NONE ? "THERE IS A DOOR" : "NOT A DOORWAY";
        }
        else
        {
            result.covers = ignored == covered::DOOR_TOP ? covered::DOOR_TOP : covered::DOOR_FACE;
        }

        if(! result.problem && x == feet_tile_x(hero) && y == feet_tile_y(hero))
        {
            result.problem = "STAND ASIDE";
        }
        break;
    }

    case piece_kind::LOG_WALL:
    {
        // Outdoors, on open ground.
        covered ignored;

        if(wall != world_gfx::EMPTY || world::solid(x, y) || owner.containers().at(x, y) >= 0 ||
           doorway(x, y, ignored))
        {
            result.problem = "NO ROOM THERE";
        }
        else if(world::terrain_at(x, y) == terrain::FLOOR)
        {
            result.problem = "OUTDOORS ONLY";
        }
        else if(world::terrain_at(x, y) == terrain::SHALLOW_WATER)
        {
            result.problem = "NOT IN WATER";
        }
        else if(x == feet_tile_x(hero) && y == feet_tile_y(hero))
        {
            result.problem = "STAND ASIDE";
        }
        break;
    }

    case piece_kind::BED:
    {
        if(! _carrying_bed)
        {
            result.pick_bed = true;

            if(wall != world_gfx::BED)
            {
                result.problem = "NO BED THERE";
            }

            break;
        }

        [[fallthrough]];
    }

    case piece_kind::PLOT:
    {
        if(wall != world_gfx::EMPTY || world::solid(x, y) || owner.containers().at(x, y) >= 0)
        {
            result.problem = farm::at(x, y) >= 0 ? "ALREADY A PLOT" : "NO ROOM THERE";
        }
        else if(! diggable(world::terrain_at(x, y)))
        {
            result.problem = "DIG IN GRASS OR SOIL";
        }
        else if(farm::count() >= farm_tuning::max_plots)
        {
            result.problem = "32 PLOTS ALREADY";
        }
        break;
    }

    default:
    {
        if(kind == piece_kind::CAMPFIRE && has_piece && existing == piece_kind::CAMPFIRE)
        {
            // Fuel for a fire that is there already (planks or sticks; matches to relight it).
            result.repair = true;

            if(owner.hero().items().count(item_id::PLANKS) + owner.hero().items().count(item_id::STICKS) <= 0)
            {
                result.problem = "NEEDS PLANKS OR STICKS";
            }
            else if(! base::fire_lit(x, y, owner.world_minute() / 60) && ! owner.hero().items().fire_starter())
            {
                result.problem = "NEEDS MATCHES OR A LIGHTER";
            }

            break;
        }

        covered ignored;

        if(wall != world_gfx::EMPTY || world::solid(x, y) || owner.containers().at(x, y) >= 0 ||
           doorway(x, y, ignored))
        {
            result.problem = "NO ROOM THERE";
        }
        else if(kind == piece_kind::GENERATOR && power::count() >= power_tuning::max_generators)
        {
            result.problem = "4 GENERATORS ALREADY";
        }
        else if((kind == piece_kind::TRAP || kind == piece_kind::SNARE) && traps::count() >= trap_tuning::max_traps)
        {
            result.problem = "16 TRAPS ALREADY";
        }
        else if((kind == piece_kind::TRAP || kind == piece_kind::SNARE) && world::terrain_at(x, y) != terrain::FOREST &&
                world::terrain_at(x, y) != terrain::GRASS && world::terrain_at(x, y) != terrain::MUD)
        {
            result.problem = "SET TRAPS IN THE WILD";
        }
        else if(x == feet_tile_x(hero) && y == feet_tile_y(hero))
        {
            result.problem = "STAND ASIDE";
        }
        else if(world::terrain_at(x, y) == terrain::SHALLOW_WATER)
        {
            result.problem = "NOT IN WATER";
        }
        else if(kind == piece_kind::CAMPFIRE && world::terrain_at(x, y) == terrain::FLOOR)
        {
            result.problem = "NOT INDOORS";
        }
        break;
    }
    }

    // A new piece needs room in a safehouse (beds are not pieces).
    int used;

    bool own_list = kind == piece_kind::PLOT || kind == piece_kind::GENERATOR || kind == piece_kind::TRAP ||
                    kind == piece_kind::SNARE;

    if(! result.problem && ! result.repair && kind != piece_kind::BED && ! own_list && ! base::room_at(x, y, used))
    {
        result.problem = used ? "SAFEHOUSE FULL" : "ALREADY 3 SAFEHOUSES";
    }

    return result;
}

const char* build_mode::place(game& owner, piece_kind kind, int tile_x, int tile_y)
{
    _piece = int(kind);
    _cursor_x = tile_x;
    _cursor_y = tile_y;
    target value = _check(owner);

    if(! value.problem && ! _materials(owner, false))
    {
        value.problem = "MISSING MATERIALS";
    }

    if(! value.problem)
    {
        _finish(owner, value);
    }

    return value.problem;
}

bool build_mode::_materials(game& owner, bool use)
{
    inventory& items = owner.hero().items();
    const piece_info& piece = info(piece_kind(_piece));

    if(piece.tool != item_id::NONE && carried(items, piece.tool) <= 0)
    {
        return false;
    }

    if(piece.matches && ! items.fire_starter())
    {
        return false;
    }

    if(piece.matches && use)
    {
        items.use_fire_starter();
    }

    for(int index = 0; index < piece.material_count; ++index)
    {
        if(material_count(items, piece.materials[index].item) < piece.materials[index].amount)
        {
            return false;
        }
    }

    if(use)
    {
        for(int index = 0; index < piece.material_count; ++index)
        {
            remove_material(items, piece.materials[index].item, piece.materials[index].amount);
        }
    }

    return true;
}

void build_mode::_finish(game& owner, const target& value)
{
    player& hero = owner.hero();
    piece_kind kind = piece_kind(_piece);
    int x = _cursor_x;
    int y = _cursor_y;
    int hour = owner.world_minute() / 60;
    bool reinforced = can_reinforce(hero);
    const char* message = nullptr;

    // Reinforcing with sheet metal (milestone 14) uses a sheet (not when the barricade already is).
    if(kind == piece_kind::BARRICADE && reinforced && hero.skills().level(skill::CARPENTRY) < reinforce_level)
    {
        int full = 0;

        if(value.repair)
        {
            static_cast<void>(base::barricade_health(x, y, full));
        }

        if(full < reinforced_health)
        {
            hero.items().remove(item_id::SHEET_METAL, 1);
        }
    }

    switch(kind)
    {

    case piece_kind::BARRICADE:
        static_cast<void>(_materials(owner, true));

        if(value.repair)
        {
            // Playtest 8: only a plain barricade turned reinforced says so; a repair is a mend.
            int full = 0;
            static_cast<void>(base::barricade_health(x, y, full));
            base::repair_barricade(x, y, reinforced);
            message = reinforced && full < reinforced_health ? "BARRICADE REINFORCED" : "BARRICADE MENDED";
        }
        else
        {
            static_cast<void>(base::add(kind, x, y, value.covers, barricade_health, owner.world_minute()));
            base::repair_barricade(x, y, reinforced);
            static_cast<void>(world::set_wall_gfx(x, y, top_wall(value.covers) ? world_gfx::BARRICADE_TOP :
                                                                               world_gfx::BARRICADE_FACE));
            message = reinforced ? "REINFORCED BARRICADE BUILT" : "BARRICADE BUILT";
        }
        break;

    case piece_kind::CRATE:
        static_cast<void>(_materials(owner, true));
        static_cast<void>(base::add(kind, x, y, covered::DOOR_FACE, 0, owner.world_minute()));
        static_cast<void>(world::set_wall_gfx(x, y, world_gfx::CRATE));
        static_cast<void>(owner.containers().add(container_kind::CRATE, loot_table::CRATE, x, y));
        message = "CRATE BUILT";
        break;

    case piece_kind::BED:
        if(value.pick_bed)
        {
            static_cast<void>(world::set_wall_gfx(x, y, world_gfx::EMPTY));
            _carrying_bed = true;
            message = "CARRYING A BED";
        }
        else
        {
            static_cast<void>(world::set_wall_gfx(x, y, world_gfx::BED));
            _carrying_bed = false;
            message = "BED MOVED";
        }
        break;

    case piece_kind::RAIN_COLLECTOR:
        static_cast<void>(_materials(owner, true));
        static_cast<void>(base::add(kind, x, y, covered::DOOR_FACE, 0, owner.world_minute()));
        static_cast<void>(world::set_wall_gfx(x, y, world_gfx::RAIN_COLLECTOR));
        message = "RAIN COLLECTOR BUILT";
        break;

    case piece_kind::PLOT:
        static_cast<void>(farm::add(x, y, owner.day()));
        static_cast<void>(world::set_wall_gfx(x, y, world_gfx::PLOT_SOIL));
        message = "PLOT DUG";
        break;

    case piece_kind::GENERATOR:
        static_cast<void>(_materials(owner, true));
        static_cast<void>(power::add(x, y, owner.world_minute()));
        static_cast<void>(world::set_wall_gfx(x, y, world_gfx::GENERATOR));
        message = "GENERATOR PLACED";
        break;

    case piece_kind::TRAP:
        static_cast<void>(_materials(owner, true));
        static_cast<void>(traps::add(x, y, owner.day()));
        static_cast<void>(world::set_wall_gfx(x, y, world_gfx::TRAP));
        message = "BOX TRAP SET";
        break;

    case piece_kind::SNARE:
        static_cast<void>(_materials(owner, true));
        static_cast<void>(traps::add(x, y, owner.day(), true));
        static_cast<void>(world::set_wall_gfx(x, y, world_gfx::SNARE));
        message = "SNARE SET";
        break;

    case piece_kind::CAN_LINE:
        static_cast<void>(_materials(owner, true));
        static_cast<void>(base::add(kind, x, y, covered::DOOR_FACE, 0, owner.world_minute()));
        static_cast<void>(world::set_wall_gfx(x, y, world_gfx::CAN_LINE));
        message = "CAN LINE STRUNG";
        break;

    case piece_kind::WOODEN_DOOR:
        // Hung closed; it opens and closes like any door, and is never locked against you.
        static_cast<void>(_materials(owner, true));
        static_cast<void>(base::add(kind, x, y, value.covers, wooden_door_health, owner.world_minute()));
        static_cast<void>(world::set_wall_gfx(x, y, world::door_gfx(world::door_state::CLOSED,
                                                                   value.covers == covered::DOOR_TOP)));
        message = "WOODEN DOOR HUNG";
        break;

    case piece_kind::LOG_WALL:
    {
        // Logs across, or running north when the wall carries on above or below (one there already across is
        // turned to match).
        static_cast<void>(_materials(owner, true));
        static_cast<void>(base::add(kind, x, y, covered::DOOR_FACE, log_wall_health, owner.world_minute()));
        bool running_north = false;

        for(int dy : { -1, 1 })
        {
            world_gfx next = world::wall_gfx(x, y + dy);

            if(next == world_gfx::LOG_WALL_H || next == world_gfx::LOG_WALL_V)
            {
                running_north = true;

                if(next == world_gfx::LOG_WALL_H && world::wall_gfx(x - 1, y + dy) != world_gfx::LOG_WALL_H &&
                   world::wall_gfx(x + 1, y + dy) != world_gfx::LOG_WALL_H)
                {
                    static_cast<void>(world::set_wall_gfx(x, y + dy, world_gfx::LOG_WALL_V));
                    owner.refresh_tile(x, y + dy);
                }
            }
        }

        static_cast<void>(world::set_wall_gfx(x, y, running_north ? world_gfx::LOG_WALL_V : world_gfx::LOG_WALL_H));
        message = "LOG WALL BUILT";
        break;
    }

    default:
        if(value.repair)
        {
            // One plank, or else a bundle of sticks.
            inventory& items = hero.items();
            items.remove(items.count(item_id::PLANKS) > 0 ? item_id::PLANKS : item_id::STICKS, 1);
            base::add_fuel(x, y, fuel_hours, hour);
            message = "FUEL ADDED";
        }
        else
        {
            static_cast<void>(_materials(owner, true));
            static_cast<void>(base::add(kind, x, y, covered::DOOR_FACE, (hour + campfire_hours) & 0xFF,
                                        owner.world_minute()));
            static_cast<void>(world::set_wall_gfx(x, y, world_gfx::CAMPFIRE));
            message = "CAMPFIRE LIT";
        }
        break;
    }

    owner.refresh_tile(x, y);
    hero.train(owner, kind == piece_kind::PLOT ? skill::FARMING : skill::CARPENTRY, info(kind).xp);
    owner.toast(message, hud_tiles::bank_accent, 90);
    sfx::play(info(kind).loud ? sfx::id::HAMMER : sfx::id::DROP, 1);
}

void build_mode::update(game& owner, const input_frame& input)
{
    if(! _active)
    {
        return;
    }

    player& hero = owner.hero();

    if(hero.dead() || hero.grabbed())
    {
        _carrying_bed = false;
        close(owner);
        return;
    }

    if(input.b_pressed || input.start_pressed)
    {
        if(_carrying_bed)
        {
            owner.toast("PUT THE BED DOWN FIRST", hud_tiles::bank_danger, 60);
            sfx::play(sfx::id::DENIED, milli(600));
        }
        else
        {
            close(owner);
            return;
        }
    }

    if(input.l_pressed || input.r_pressed)
    {
        _piece = (_piece + (input.r_pressed ? 1 : piece_kinds - 1)) % piece_kinds;
        _hold = 0;
        sfx::play(sfx::id::SWAP, milli(400));
    }

    int dx = int(input.right_pressed) - int(input.left_pressed);
    int dy = int(input.down_pressed) - int(input.up_pressed);

    if(dx || dy)
    {
        int player_x = feet_tile_x(hero);
        int player_y = feet_tile_y(hero);
        _cursor_x = bn::clamp(_cursor_x + dx, player_x - reach, player_x + reach);
        _cursor_y = bn::clamp(_cursor_y + dy, player_y - reach, player_y + reach);
        _hold = 0;
    }

    target value = _check(owner);
    piece_kind kind = piece_kind(_piece);
    bool ready = ! value.problem && (value.repair && kind == piece_kind::CAMPFIRE ? true :
                                     kind == piece_kind::BED || _materials(owner, false));
    int needed = value.repair && kind == piece_kind::CAMPFIRE ? fuel_frames :
                 hero.body().action_time(info(kind).build_frames);

    if(input.a_held && ready)
    {
        ++_hold;

        // Hammering is loud: a bang and a noise every second.
        if(info(kind).loud && ! value.repair && _hold % 60 == 1)
        {
            sfx::play(sfx::id::HAMMER, milli(800));
            owner.noise(hero.position(), hammer_noise);
        }

        if(_hold >= needed)
        {
            _hold = 0;
            _finish(owner, value);
            value = _check(owner);
        }
    }
    else
    {
        if(input.a_pressed && ! ready)
        {
            owner.toast(value.problem ? value.problem : "MISSING MATERIALS", hud_tiles::bank_danger, 60);
            sfx::play(sfx::id::DENIED, milli(600));
        }

        _hold = 0;
    }

    // Red when A would do nothing: a bad spot, or no tool or materials (playtest 8).
    _update_cursor(owner, ready);
    _update_view(owner, value);
    owner.hud_ref().set_build(&_view);
}

void build_mode::_update_cursor(game& owner, bool valid)
{
    static_cast<void>(owner);
    int left = _cursor_x * world::tile_size;
    int top = _cursor_y * world::tile_size;

    for(int corner = 0; corner < 4; ++corner)
    {
        bn::sprite_ptr& sprite = *_corners[corner];
        sprite.set_tiles(bn::sprite_items::fx.tiles_item(), valid ? corner_yellow : corner_red);
        sprite.set_position(left + ((corner & 1) ? 16 + 1 : -1), top + ((corner & 2) ? 16 + 1 : -1));
    }
}

void build_mode::_update_view(game& owner, const target& value)
{
    const inventory& items = owner.hero().items();
    piece_kind kind = piece_kind(_piece);
    const piece_info& piece = info(kind);
    char text[24];
    int length;

    _view = hud::build_view();
    _view.selected = _piece;
    int needed = value.repair && kind == piece_kind::CAMPFIRE ? fuel_frames :
                 owner.hero().body().action_time(piece.build_frames);
    _view.seconds = (needed - _hold + 59) / 60;
    _view.holding = _hold > 0;

    switch(kind)
    {

    case piece_kind::PLOT:
        _view.limit_name = "PLOTS";
        _view.limit = farm_tuning::max_plots;
        _view.used = farm::count();
        break;

    case piece_kind::GENERATOR:
        _view.limit_name = "GENERATORS";
        _view.limit = power_tuning::max_generators;
        _view.used = power::count();
        break;

    case piece_kind::TRAP:
    case piece_kind::SNARE:
        _view.limit_name = "TRAPS";
        _view.limit = trap_tuning::max_traps;
        _view.used = traps::count();
        break;

    default:
        _view.used = base::pieces_at(_cursor_x, _cursor_y);
        break;
    }

    // Title: the piece, or (on a spot it can go) what A will do there.
    const char* title = piece.name;

    if(value.problem && kind != piece_kind::BED)
    {
        title = piece.name;
    }
    else if(kind == piece_kind::BARRICADE)
    {
        title = value.repair ? "MEND BARRICADE" : top_wall(value.covers) ? "BARRICADE" :
                value.covers == covered::DOOR_FACE ? "BARRICADE DOOR" : "BARRICADE WINDOW";
    }
    else if(kind == piece_kind::BED)
    {
        title = _carrying_bed ? "PUT THE BED DOWN" : "PICK UP A BED";
    }
    else if(kind == piece_kind::WOODEN_DOOR)
    {
        title = "HANG A DOOR";
    }
    else if(kind == piece_kind::CAMPFIRE && value.repair)
    {
        title = "ADD FUEL";
    }
    else if(kind == piece_kind::PLOT)
    {
        title = "DIG A PLOT";
    }

    ui::append(_view.title, 0, title);

    // Materials: carried / needed, green when there is enough.
    hud::build_line& materials = _view.lines[0];

    if(kind == piece_kind::CAMPFIRE && value.repair)
    {
        length = ui::append(text, 0, "PLANKS ");
        ui::append_int(text, length, items.count(item_id::PLANKS));
        segment(materials, text, colour::text);
        length = ui::append(text, 0, "STICKS ");
        ui::append_int(text, length, items.count(item_id::STICKS));
        segment(materials, text, colour::text);
    }
    else if(! piece.material_count)
    {
        segment(materials, _carrying_bed ? "IN YOUR ARMS" : kind == piece_kind::PLOT ? "PLANT SEEDS, THEN WATER" :
                                                                       "NO MATERIALS", colour::quiet);
    }

    // A mend uses the same materials as a build (playtest 8), so it lists them too.
    for(int index = 0; index < piece.material_count && ! (value.repair && kind == piece_kind::CAMPFIRE); ++index)
    {
        const material& need = piece.materials[index];
        int have = material_count(items, need.item);
        length = ui::append(text, 0, need.item == item_id::GARBAGE_BAG ? "BAG" : need.item == item_id::PLANKS ? "PLANKS" :
                                      need.item == item_id::NAILS ? "NAILS" : need.item == item_id::STONES ? "STONES" :
                                      need.item == item_id::GENERATOR ? "GENERATOR" : need.item == item_id::HINGE ? "HINGE" :
                                      need.item == item_id::DOORKNOB ? "KNOB" : need.item == item_id::LOGS ? "LOGS" :
                                      need.item == item_id::ROPE ? "ROPE" : "STICKS");
        length = ui::append(text, length, " ");
        length = ui::append_int(text, length, have);
        length = ui::append(text, length, "/");
        ui::append_int(text, length, need.amount);
        segment(materials, text, have >= need.amount ? colour::ui_green : colour::ui_red);
    }

    // Tools and noise, or why it can't be built here.
    hud::build_line& status = _view.lines[1];

    if(value.problem)
    {
        segment(status, value.problem, colour::ui_red);
    }
    else
    {
        // Short of something: say what first (playtest 8), instead of a green "HAMMER OK".
        const material* short_of = nullptr;

        for(int index = 0; index < piece.material_count && ! short_of && kind != piece_kind::BED &&
                           ! (value.repair && kind == piece_kind::CAMPFIRE); ++index)
        {
            if(material_count(items, piece.materials[index].item) < piece.materials[index].amount)
            {
                short_of = &piece.materials[index];
            }
        }

        if(short_of)
        {
            item_id need = short_of->item;
            segment(status, need == item_id::PLANKS ? "NEED PLANKS" : need == item_id::NAILS ? "NEED NAILS" :
                            need == item_id::LOGS ? "NEED LOGS" : need == item_id::STONES ? "NEED STONES" :
                            need == item_id::HINGE ? "NEED A HINGE" : need == item_id::DOORKNOB ? "NEED A KNOB" :
                            need == item_id::ROPE ? "NEED ROPE" : need == item_id::GENERATOR ? "NEED A GENERATOR" :
                            need == item_id::GARBAGE_BAG ? "NEED A BAG" : "NEED STICKS", colour::ui_red);
        }
        else if(piece.tool != item_id::NONE)
        {
            bool has = carried(items, piece.tool) > 0;
            bool shovel = piece.tool == item_id::SHOVEL;
            segment(status, has ? (shovel ? "SHOVEL OK" : "HAMMER OK") : (shovel ? "NO SHOVEL" : "NO HAMMER"),
                    has ? colour::ui_green : colour::ui_red);
        }

        if(piece.matches)
        {
            bool has = items.fire_starter();
            segment(status, has ? "FIRE OK" : "NO MATCHES", has ? colour::ui_green : colour::ui_red);
        }

        segment(status, piece.loud ? "NOISE: LOUD" : "NOISE: QUIET", piece.loud ? colour::ui_orange : colour::quiet);
    }

    // The HUD redraws when any of this changes.
    int key = (_piece * 7) + (_view.seconds * 131) + (_view.used * 9973) + int(_view.holding) +
              (_view.limit * 3);

    for(const char* c = _view.title; *c; ++c)
    {
        key = (key * 31) + *c;
    }

    for(const hud::build_line& line : _view.lines)
    {
        for(int index = 0; index < line.count; ++index)
        {
            key = (key * 7) + line.colour[index];

            for(const char* c = line.text[index]; *c; ++c)
            {
                key = (key * 31) + *c;
            }
        }
    }

    _view.key = key & 0x7FFFFFFF;
}

}
