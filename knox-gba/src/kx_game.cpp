#include "kx_game.h"

#include "bn_sprite_items_fx.h"

#include "bn_core.h"
#include "bn_display.h"
#include "bn_keypad.h"
#include "bn_log.h"
#include "bn_profiler.h"

#include "bn_bg_palettes.h"
#include "bn_color.h"
#include "bn_sprite_palettes.h"
#include "bn_sprite_tiles.h"
#include "bn_sprites.h"
#include "bn_timers.h"

#include "kx_bench.h"
#include "kx_flow_field.h"
#include "kx_hordes.h"
#include "kx_base.h"
#include "kx_farm.h"
#include "kx_power.h"
#include "kx_traps.h"
#include "kx_fixtures.h"
#include "kx_recipes.h"
#include "kx_sandbox.h"
#include "kx_save.h"
#include "kx_settings.h"
#include "kx_survivors.h"
#include "kx_timeline.h"
#include "kx_hud_tiles.h"
#include "kx_macro_map.h"
#include "kx_sfx.h"
#include "kx_vector.h"
#include "kx_world.h"
#include "kx_world_detail.h"
#include "kx_world_gen_tuning.h"

namespace kx
{

namespace
{
    // Test map: the start on the east-west road, west of the crossing (bench zombies are placed from it).
    constexpr int spawn_tile_x = 24;
    constexpr int spawn_tile_y = 21;
    constexpr int spawn_feet_y = 12;

    // Hand-placed test zombies (tiles): forest, house, shed, field, roads and the south-east.
    constexpr int test_zombies[][2] = {
        { 37, 4 }, { 45, 3 }, { 50, 9 }, { 41, 13 }, { 53, 15 },
        { 8, 8 }, { 15, 12 }, { 23, 11 },
        { 6, 30 }, { 10, 40 }, { 20, 30 },
        { 40, 21 }, { 29, 40 }, { 44, 33 }, { 48, 39 },
    };

    constexpr int horde_size = zombie_manager::max_zombies;

    #if KX_BENCH == 8
        // Milestone 12.7's saved state, set up before the save and checked after the load (tiles; -1 if none was
        // found): a door opened, one broken, a tree chopped, a gun safe opened.
        int bench_open_door[2] = { -1, -1 };
        int bench_broken_door[2] = { -1, -1 };
        int bench_stump[2] = { -1, -1 };
        int bench_safe[2] = { -1, -1 };

        // Base pieces built in the first pass, checked after the load (tiles; -1 if none was found).
        int bench_barricade_x = -1;
        int bench_barricade_y = -1;
        int bench_crate_x = -1;
        int bench_crate_y = -1;
        int bench_fire_x = -1;
        int bench_fire_y = -1;

        // The first tile within a square around a centre that passes a test.
        template<typename Test>
        bool bench_find(int center_x, int center_y, int radius, int& found_x, int& found_y, const Test& test)
        {
            for(int y = center_y - radius; y <= center_y + radius; ++y)
            {
                for(int x = center_x - radius; x <= center_x + radius; ++x)
                {
                    if(world::loaded(x, y) && test(x, y))
                    {
                        found_x = x;
                        found_y = y;
                        return true;
                    }
                }
            }

            return false;
        }
    #endif

    #if KX_BENCH == 4
        constexpr int restart_games = 3;
        constexpr int restart_death_frame = 30;
        int restart_count = 0;
    #endif
    constexpr int horde_min_distance_tiles = 8;
    constexpr int death_screen_delay = 75;
    constexpr int menu_sprite_dim_percent = 70;   // sprites behind a menu screen fade this far to black (as the world)

    // Only the game itself and the save bench touch SRAM; the other benches never save.
    constexpr bool saving = ! KX_BENCH || KX_BENCH == 8;

    [[nodiscard]] bn::fixed_point tile_feet(int tile_x, int tile_y)
    {
        return bn::fixed_point((tile_x * world::tile_size) + (world::tile_size / 2),
                               (tile_y * world::tile_size) + spawn_feet_y);
    }

    [[nodiscard]] bn::fixed_point start_feet()
    {
        int tile_x;
        int tile_y;
        world::start_tile(tile_x, tile_y);
        return tile_feet(tile_x, tile_y);
    }

    // Nearest open tile to the given one (spiral search), so spawns never land inside walls or trees.
    [[nodiscard]] bool open_tile_near(int& tile_x, int& tile_y)
    {
        for(int radius = 0; radius <= 3; ++radius)
        {
            for(int y = tile_y - radius; y <= tile_y + radius; ++y)
            {
                for(int x = tile_x - radius; x <= tile_x + radius; ++x)
                {
                    if(! world::solid(x, y) && world::terrain_at(x, y) != terrain::DEEP_WATER)
                    {
                        tile_x = x;
                        tile_y = y;
                        return true;
                    }
                }
            }
        }

        return false;
    }

    // Merges the button edges of a frame skipped by hitstop into the next one, so no press is lost.
    void merge_edges(input_frame& pending, const input_frame& frame)
    {
        pending.dx = frame.dx;
        pending.dy = frame.dy;
        pending.a_held = frame.a_held;
        pending.b_held = frame.b_held;
        pending.l_held = frame.l_held;
        pending.a_pressed |= frame.a_pressed;
        pending.b_pressed |= frame.b_pressed;
        pending.b_released |= frame.b_released;
        pending.l_pressed |= frame.l_pressed;
        pending.l_released |= frame.l_released;
        pending.select_pressed |= frame.select_pressed;
    pending.select_held = frame.select_held;
    pending.start_pressed |= frame.start_pressed;
    pending.r_pressed |= frame.r_pressed;
    pending.r_held = frame.r_held;
    pending.up_pressed |= frame.up_pressed;
    pending.down_pressed |= frame.down_pressed;
    pending.left_pressed |= frame.left_pressed;
    pending.right_pressed |= frame.right_pressed;
    }
}

game::game(bool load_save, const character_choice* choice) :
    _camera(bn::camera_ptr::create(0, 0)),
    _renderer(_camera),
    _fx(_camera),
    _zombies(_camera),
    _vehicles(_camera),
    _player(_camera, start_feet()),
    _camera_controller(_camera),
    _lights(_camera),
    _gun_fx(_camera),
    _throws(_camera),
    _animals(_camera)
{
    // Playtest 5: Save and Quit from the MENU tab left the sprites faded as they are behind a menu, and the next
    // game (Continue) started with them dark. Every game starts with the palettes at full colour.
    bn::sprite_palettes::set_fade_intensity(0);
    bn::sprite_palettes::set_grayscale_intensity(0);
    bn::bg_palettes::set_fade_intensity(0);
    bn::bg_palettes::set_grayscale_intensity(0);

    _containers.init();
    flow::reset();
    music::enter_world();
    _hud.set_perf_visible(settings::cpu_meter());       // playtest 2: the CPU METER setting

    if(! saving || ! load_save)
    {
        survivors::reset();
        base::reset();
        farm::reset();
        power::reset();
        traps::reset();
        _events.reset();
        world_map::reset();
    }

    bool loaded = saving && load_save && _load();
    _loaded = loaded;
    #if KX_BENCH == 10
        // Night bench: 22:55 on day 1 (the grid still has power; the campfire calls the hordes at 23:00).
        _world_minute = (22 * 60) + 55;
    #endif

    _weather = weather::at(_world_minute);

    // A new survivor: the occupation and traits chosen on the creation screen.
    if(choice && (! loaded || _new_survivor))
    {
        _player.skills().start(choice->job, choice->traits);
    }

    #if KX_BENCH == 3
        // Loot bench: start in the kitchen, under the cabinet at (6, 5).
        _player.teleport(tile_feet(6, 6));
    #endif

    world::prepare(_player.position().x().integer(), _player.position().y().integer());
    _camera_controller.snap_to(_player);
    _renderer.update(_camera_controller.left(), _camera_controller.top());

    #if KX_BENCH == 6
        // Survival bench: fast time, a laceration to bandage, the bed right below the player.
        survival_tuning().adjust(int(survival_param::FRAMES_PER_MINUTE), -1000);
        survival_tuning().adjust(int(survival_param::INFECTION_MIN_HOURS), -1000);
        survival_tuning().adjust(int(survival_param::INFECTION_MAX_HOURS), -1000);
        survival_tuning().adjust(int(survival_param::INFECTION_MIN_HOURS), 2);
        survival_tuning().adjust(int(survival_param::INFECTION_MAX_HOURS), 2);
        _player.teleport(tile_feet(7, 12));
        _player.body().add_wound(body_part::LEFT_ARM, wound_type::LACERATION, false, _random);
    #elif KX_BENCH == 5
        // Crawler bench: a crawler 40 px east crawls in while the player faces it and swings.
        BN_LOG("KXCRAWLER game start");

        // Regression: a direction 10/4096 px long used to be a division by zero inside normalized().
        bn::fixed_point tiny = normalized(bn::fixed_point(bn::fixed::from_data(10), bn::fixed::from_data(-3)));
        BN_LOG("KXCRAWLER tiny normalized=", tiny.x(), ",", tiny.y());
        _zombies.spawn(_player.position() + bn::fixed_point(40, 0), _random);
        _zombies.make_last_crawler();
    #elif KX_BENCH == 7 || KX_BENCH == 8
        // Travel and save benches: god mode; the hordes bring the zombies.
        _god_mode = true;
        respawn_zombies();
    #elif KX_BENCH == 4
        // Restart bench: no zombies; the run loop kills the player to test death and restart.
        BN_LOG("KXRESTART game=", restart_count);
    #elif KX_BENCH
        _god_mode = true;

        #if KX_BENCH == 3 || KX_BENCH == 9 || KX_BENCH == 11 || KX_BENCH == 12 || KX_BENCH == 13 || KX_BENCH == 14 || \
            KX_BENCH == 15 || KX_BENCH == 16
            // No zombies: the loot bench only searches; the vehicles and guns benches place their own.
        #elif KX_BENCH == 2
            _zombies.spawn(tile_feet(spawn_tile_x + 4, spawn_tile_y), _random);
            _zombies.spawn(tile_feet(spawn_tile_x + 7, spawn_tile_y - 1), _random);
            _zombies.spawn(tile_feet(spawn_tile_x + 9, spawn_tile_y + 1), _random);
        #else
            spawn_horde();
        #endif
    #else
        respawn_zombies();
    #endif

    // A new world or a new survivor is saved straight away, so CONTINUE finds them before the first autosave.
    if(saving && (! loaded || _new_survivor))
    {
        _save(true);
    }

    // How the survivor hears (traits: Keen Hearing, Hard of Hearing).
    sfx::set_hearing(_player.skills().hearing_percent(), _player.skills().stereo());

    if(_new_survivor)
    {
        toast("A NEW SURVIVOR ARRIVES", hud_tiles::bank_accent, 180);
    }
}

// The world block's worst case fits the save: horde counts and noise, every container, every decal, plus the small
// blocks, 1404 B measured on 2026-11-24 (safehouses 804, farm plots 320, traps 128, survivors 68, generators 64,
// events 20), allowed 1600 (it was 2048; the animals plan's farm tallies needed some of that).
static_assert(macro::cells * macro::cells + 256 + container_store::max_save_bytes + world::max_decal_save_bytes +
              1600 + vehicle_manager::save_bytes + world_map::save_bytes + animal_manager::save_bytes <=
              save::world_capacity);

void game::_save(bool at_once)
{
    if(! saving || world::test_world())
    {
        return;
    }

    save::writer character = save::character_writer();
    character.put(_world_minute);
    character.put(_start_minute);
    character.put(_kills);
    _player.save(character);

    // The zombies on screen go back into the counts of their cells in the saved copy (they materialise again
    // when the save is loaded).
    save::writer world = save::world_writer();
    uint8_t* counts = world.cursor();
    hordes::save(world);

    for(int index = 0; index < _zombies.count(); ++index)
    {
        const bn::fixed_point& position = _zombies.position(index);

        if(_zombies.survivor(index) >= 0)
        {
            survivors::seen_at(_zombies.survivor(index), position);
            continue;
        }

        int cell_x = (position.x().integer() >> world::tile_shift) >> macro::cell_shift;
        int cell_y = (position.y().integer() >> world::tile_shift) >> macro::cell_shift;

        if(unsigned(cell_x) < unsigned(macro::cells) && unsigned(cell_y) < unsigned(macro::cells))
        {
            uint8_t& count = counts[(cell_y * macro::cells) + cell_x];
            count = uint8_t(bn::min(count + 1, 255));
        }
    }

    _containers.save(world);
    world::save_decals(world);
    survivors::save(world);
    base::save(world);
    farm::save(world);
    power::save(world);
    traps::save(world);
    _events.save(world);
    _vehicles.save(world);
    world_map::save(world);
    _animals.save(world);

    save::summary info;
    info.valid = true;
    info.has_survivor = ! _dead;
    info.seed = world::seed();
    info.world_minute = _world_minute;
    info.focus_x = _player.position().x().integer();
    info.focus_y = _player.position().y().integer();
    info.sandbox = sandbox::current();
    save::commit(character, world, info, at_once);
}

bool game::_load()
{
    save::summary info;
    bool world_ok;

    if(! save::load(info, world_ok))
    {
        return false;
    }

    // The world's sandbox settings come back first (the clock, loot and zombies read them).
    sandbox::set(info.sandbox);

    // A world block cut off mid-write is lost; the character still continues on the seed's fresh world.
    if(world_ok)
    {
        save::reader world = save::world_reader();
        hordes::load(world);
        _containers.load(world);
        world::load_decals(world);
        survivors::load(world);
        base::load(world);
        farm::load(world);
        power::load(world);
        traps::load(world);
        _events.load(world);
        _vehicles.load(world);
        world_map::load(world);
        _animals.load(world);
    }
    else
    {
        world_map::reset();
        survivors::reset();
        base::reset();
        farm::reset();
        power::reset();
        traps::reset();
        _events.reset();
        _vehicles.reset();
    }

    if(info.has_survivor)
    {
        save::reader character = save::character_reader();
        character.get(_world_minute);
        character.get(_start_minute);
        character.get(_kills);
        _player.load(character);
    }
    else
    {
        // The last survivor died: a new one starts in a random village, the clock running on (spec: Permadeath).
        _world_minute = info.world_minute;
        _start_minute = _world_minute;
        _new_survivor = true;

        int tile_x;
        int tile_y;
        survivors::start_tile(tile_x, tile_y);
        _player.teleport(tile_feet(tile_x, tile_y));
    }

    return true;
}

game_exit game::run()
{
    #if KX_BENCH == 4
        struct restart_counter
        {
            ~restart_counter()
            {
                ++restart_count;
            }
        } counter;
    #endif

    input_frame pending;

    #if KX_BENCH
        bool bench_reported = false;
        int bench_frame = 0;
    #endif

    #if KX_DUMP
        int dump_frame = 0;
    #endif

    while(true)
    {
        if(saving && save::busy())
        {
            KX_BENCH_BEGIN(bench::SAVE);
            save::step();
            KX_BENCH_END();
        }

        if(_dead)
        {
            if(_update_death())
            {
                return game_exit::DIED;
            }
        }
        else if(_menu.active())
        {
            _menu.update(*this);
        }
        else
        {
            input_frame frame_input = _input.read();

            // Lists scroll while a direction is held (playtest 5); the map screen repeats its own cursor.
            if((_menu_open() && ! _map.active()) || _search.active() || _car.active())
            {
                _menu_repeat.apply(frame_input);
            }
            else
            {
                _menu_repeat.reset();
            }

            #if KX_BENCH
                if(_input.finished() && ! bench_reported)
                {
                    BN_LOG("KXBENCH kills=", _kills, " zombies=", _zombies.count(), " hits=", _zombies.total_hits(),
                           " lunge_grabs=", _zombies.total_grabs(), " knockdowns=", _zombies.total_knockdowns(),
                           " stomps=", _zombies.total_stomps());

                    const inventory& items = _player.items();

                    for(int slot = 0; slot < inventory::slots; ++slot)
                    {
                        const item_stack& stack = items.slot(slot);

                        if(! stack.empty())
                        {
                            BN_LOG("KXLOOT carried ", slot, ": ", info(stack.id).name, " x", stack.quantity());
                        }
                    }

                    int opened = 0;

                    for(int index = 0; index < _containers.count(); ++index)
                    {
                        opened += _containers.record(index).opened;
                    }

                    BN_LOG("KXLOOT weight=", items.weight(), " capacity=", items.capacity(), " containers=",
                           _containers.count(), " opened=", opened, " hand=", info(items.equipped(equip_slot::HAND).id).name);
                    // The dump comes before the report, whose last line tells the log reader the run is over.
                    #if KX_BENCH == 7
                        // Where the route ended, then around the first settlements (a base, towns, villages).
                        world::dump(0, true);

                        for(int index = 0; index < bn::min(macro::settlement_count(), 6); ++index)
                        {
                            teleport_to_settlement(index);
                            world::dump(index + 1, false);
                        }
                    #endif

                    bench::report();
                    bench_reported = true;
                }
            #elif BN_CFG_PROFILER_ENABLED
                if(bn::keypad::l_held() && bn::keypad::start_pressed())
                {
                    bn::profiler::show();
                }
            #endif

            if(_menu_open())
            {
                // The Start menu pauses the world; L/R switch tabs (spec: Menu tabs). MENU's SAVE AND QUIT saves and
                // quits to the title (playtest 4; it was Select held on any tab).
                bool switch_tab = (frame_input.l_pressed || frame_input.r_pressed) && ! _gear.choosing();

                if(switch_tab)
                {
                    // Gear, Health, Craft, Skills, Map, Menu.
                    constexpr int tab_count = 6;
                    int current = _gear.active() ? 0 : _health.active() ? 1 : _craft.active() ? 2 :
                                  _skills.active() ? 3 : _map.active() ? 4 : 5;
                    _open_tab((current + (frame_input.r_pressed ? 1 : tab_count - 1)) % tab_count);
                }
                else if(_gear.active())
                {
                    _gear.update(*this, frame_input);
                }
                else if(_health.active())
                {
                    _health.update(*this, frame_input);
                }
                else if(_craft.active())
                {
                    _craft.update(*this, frame_input);
                }
                else if(_skills.active())
                {
                    _skills.update(*this, frame_input);
                }
                else if(_map.active())
                {
                    _map.update(*this, frame_input);
                }
                else
                {
                    _menu_tab.update(*this, frame_input);
                }

                if(saving && _menu_tab.active() && _menu_tab.quit_requested())
                {
                    close_menus();
                    _save(true);
                    return game_exit::QUIT;
                }

                #if KX_BENCH == 3 && KX_DUMP
                    // Playtests 1 and 4: popups raised in the Gear tab show over it, then go and the tab redraws
                    // (dumps 9101, 9103 and 9102).
                    static int bench_toast_frames = -1;

                    if(_gear.active() && bench_toast_frames < 0)
                    {
                        // First, a tour of the tabs as they first draw (dumps 9200-9205: Gear, Health, Craft,
                        // Skills, Map, Menu), to check them against the mockups.
                        for(int tab = 0; tab < 6; ++tab)
                        {
                            _open_tab(tab);
                            input_frame none;

                            for(int step = 0; step < 2; ++step)
                            {
                                if(_gear.active()) _gear.update(*this, none);
                                else if(_health.active()) _health.update(*this, none);
                                else if(_craft.active()) _craft.update(*this, none);
                                else if(_skills.active()) _skills.update(*this, none);
                                else if(_map.active()) _map.update(*this, none);
                                else _menu_tab.update(*this, none);
                            }

                            _hud.pixels().dump(9200 + tab);
                        }

                        // The MENU tab's pages (playtest 3, mockup 20): the guide on GUNS' second page (9206), the
                        // settings (9207).
                        _menu_tab.show(menu_screen::page::GUIDE, 4, 1);
                        _menu_tab.update(*this, input_frame());
                        _hud.pixels().dump(9206);
                        _menu_tab.show(menu_screen::page::SETTINGS, 0);
                        _menu_tab.update(*this, input_frame());
                        _hud.pixels().dump(9207);
                        // Playtest 4: the WORLD page on SHUTOFF (9208), the entries with WORLD chosen (9209).
                        _menu_tab.show(menu_screen::page::WORLD, 3);
                        _menu_tab.update(*this, input_frame());
                        _hud.pixels().dump(9208);
                        _menu_tab.show(menu_screen::page::ENTRIES, 2);
                        _menu_tab.update(*this, input_frame());
                        _hud.pixels().dump(9209);
                        // SAVE AND QUIT chosen (9210), then the Gear tab on the bag's last slot, empty (9211).
                        _menu_tab.show(menu_screen::page::ENTRIES, 3);
                        _menu_tab.update(*this, input_frame());
                        _hud.pixels().dump(9210);
                        _open_tab(0);
                        _gear.show_bag_slot(inventory::slots - 1);
                        _gear.update(*this, input_frame());
                        _hud.pixels().dump(9211);

                        _open_tab(0);
                        _gear.update(*this, input_frame());

                        // Playtest 4: three at once, stacked above the hotbar's place (9101), the shortest gone
                        // (9103), all gone (9102).
                        bench_toast_frames = 0;
                        toast("LACERATION: LEFT ARM", hud_tiles::bank_danger, 30);
                        toast("BLADE LEVEL 2", hud_tiles::bank_accent, 20);
                        toast("TOOK VITAMINS", hud_tiles::bank_normal, 12);
                    }
                #endif

                _screen_toast();

                #if KX_BENCH == 3 && KX_DUMP
                    if(_gear.active() && bench_toast_frames >= 0)
                    {
                        ++bench_toast_frames;

                        if(bench_toast_frames == 1)
                        {
                            _hud.pixels().dump(9101);
                        }
                        else if(bench_toast_frames == 16)
                        {
                            _hud.pixels().dump(9103);
                        }
                        else if(bench_toast_frames == 40)
                        {
                            _hud.pixels().dump(9102);
                        }
                    }
                #endif
            }
            else if(_sleep_state)
            {
                _update_sleep(frame_input);
            }
            else if(! _search.active() && ! _build.active() && ! _car.active() && ! _safe.active() &&
                    frame_input.start_pressed)
            {
                // Select + Start: the tuning menu, in tester and bench builds only (playtest 5: players found it by
                // accident on the Miyoo Mini and could break the game in it).
                if((KX_DEBUG || KX_BENCH) && frame_input.select_held)
                {
                    _menu.open(*this);
                }
                else
                {
                    _open_tab(0);
                }
            }
            else
            {
                // While searching, the world keeps running but the player stands still.
                if(_search.active())
                {
                    _update_search(frame_input);
                    frame_input = input_frame();
                }
                else if(_build.active())
                {
                    // Build mode: the player stands still while the cursor moves; the world keeps running.
                    _build.update(*this, frame_input);
                    frame_input = input_frame();
                }
                else if(_car.active())
                {
                    // Looking a car over, like searching: the world keeps running.
                    _car.update(*this, frame_input);
                    frame_input = input_frame();
                }
                else if(_safe.active())
                {
                    // A gun safe's dial (milestone 12.7): the world keeps running too.
                    _safe.update(*this, frame_input);
                    frame_input = input_frame();
                }

                if(_search.active() || _car.active() || _safe.active())
                {
                    _screen_toast();
                }

                merge_edges(pending, frame_input);

                if(_hitstop > 0)
                {
                    --_hitstop;
                }
                else
                {
                    ++_frames;

                    #if KX_BENCH == 4
                        if(_frames == restart_death_frame && restart_count < restart_games)
                        {
                            _player.die();
                            player_bitten();
                        }
                    #endif

                    // Vehicles first: a car being driven carries the player.
                    BN_PROFILER_START("vehicles");
                    KX_BENCH_BEGIN(bench::VEHICLES);
                    _vehicles.update(*this, pending);
                    KX_BENCH_END();
                    BN_PROFILER_STOP();

                    BN_PROFILER_START("player");
                    KX_BENCH_BEGIN(bench::PLAYER);
                    _player.update(pending, *this);
                    _update_door_work(pending);
                    _update_drowning();
                    KX_BENCH_END();
                    BN_PROFILER_STOP();

                    BN_PROFILER_START("flow");
                    KX_BENCH_BEGIN(bench::FLOW);
                    flow::update(_player.position().x().integer() >> world::tile_shift,
                                 (_player.position().y().integer() - 1) >> world::tile_shift,
                                 tuning(zombie_param::FLOW_POPS).integer(), _zombies.chase_reach(_player.position()));
                    KX_BENCH_END();
                    BN_PROFILER_STOP();

                    BN_PROFILER_START("zombies");
                    KX_BENCH_BEGIN(bench::ZOMBIES);
                    _zombies.update(*this);
                    KX_BENCH_END();
                    KX_BENCH_BEGIN(bench::ANIMALS);
                    _animals.update(*this);
                    KX_BENCH_END();
                    BN_PROFILER_STOP();

                    BN_PROFILER_START("hordes");
                    KX_BENCH_BEGIN(bench::HORDES);
                    _update_population();
                    KX_BENCH_END();
                    BN_PROFILER_STOP();

                    BN_PROFILER_START("fx");
                    KX_BENCH_BEGIN(bench::FX);
                    _throws.update(*this);
                    _fx.update();
                    KX_BENCH_END();
                    BN_PROFILER_STOP();

                    pending = input_frame();

                    // The clock runs while playing and searching, not in menus.
                    if(++_minute_frames >= frames_per_minute())
                    {
                        _minute_frames = 0;
                        _tick_minute(false);
                    }
                }

                _update_cues();

                // A grab interrupts the search.
                if(_search.active() && (_player.grabbed() || _player.dead()))
                {
                    _search.close(*this);
                    _search_car = -1;
                }

                if(_car.active() && (_player.grabbed() || _player.dead()))
                {
                    _car.close(*this);
                }

                if(_safe.active() && (_player.grabbed() || _player.dead()))
                {
                    _safe.close(*this);
                }

                if(! _search.active() && ! _car.active() && ! _safe.active())
                {
                    _update_context();
                }

                _update_play();

                #if KX_BENCH
                    if(! bench_reported)
                    {
                        bench::frame_end();

                        #if KX_BENCH == 14
                            if(bench_frame == 1)
                            {
                                _bench_census();
                            }
                            else if(bench_frame >= 30 && (bench_frame - 30) % 40 == 0)
                            {
                                _bench_tour((bench_frame - 30) / 40);
                            }
                        #endif

                        #if KX_BENCH == 7
                            // Milestone 14: every lot of the world counted by building (a one-off, at the start).
                            if(bench_frame == 1)
                            {
                                int counts[int(building::COUNT)] = {};
                                int cabins = 0;

                                for(int cell_y = 0; cell_y < macro::cells; ++cell_y)
                                {
                                    for(int cell_x = 0; cell_x < macro::cells; ++cell_x)
                                    {
                                        for(int key = 0; key < 64; ++key)
                                        {
                                            world::detail::lot_view lot;

                                            if(((key >> 2) & 3) < world_gen::lots_deep &&
                                               ((key >> 4) & 3) < world_gen::lots_deep &&
                                               world::detail::lot_planned(cell_x, cell_y, key, lot))
                                            {
                                                ++counts[int(lot.kind)];
                                                cabins += macro::zone_at(cell_x, cell_y) == macro::zone::WILDERNESS;
                                            }
                                        }
                                    }
                                }

                                for(int index = 0; index < int(building::COUNT); ++index)
                                {
                                    BN_LOG("KXTOWN ", building_names[index], "=", counts[index]);
                                }

                                BN_LOG("KXTOWN in the wild: ", cabins);

                                // Stage 4: the set pieces on the roads (the churches are in the census above).
                                const char* piece_names[] = { "NONE", "BUS", "BARRICADE", "CHECKPOINT" };
                                int piece_counts[4] = {};

                                for(int cell_y = 0; cell_y < macro::cells; ++cell_y)
                                {
                                    for(int cell_x = 0; cell_x < macro::cells; ++cell_x)
                                    {
                                        world::detail::road_piece_view pieces[2];
                                        int count = world::detail::road_pieces(cell_x, cell_y, pieces);

                                        for(int index = 0; index < count; ++index)
                                        {
                                            ++piece_counts[int(pieces[index].kind)];
                                            BN_LOG("KXTOWN ", piece_names[int(pieces[index].kind)], " at ",
                                                   pieces[index].tile_x, ",", pieces[index].tile_y);
                                        }
                                    }
                                }

                                BN_LOG("KXTOWN pieces: bus=", piece_counts[1], " barricade=", piece_counts[2],
                                       " checkpoint=", piece_counts[3]);
                            }

                            // The wild (milestone 13): what the seed puts round the route.
                            if(bench_frame % 120 == 0)
                            {
                                int deer = 0;
                                int rabbits = 0;

                                for(int index = 0; index < _animals.count(); ++index)
                                {
                                    deer += _animals.kind_of(index) == animal_kind::DEER;
                                    rabbits += _animals.kind_of(index) == animal_kind::RABBIT;
                                }

                                int tile_x = _player.position().x().integer() >> world::tile_shift;
                                int tile_y = _player.position().y().integer() >> world::tile_shift;
                                BN_LOG("KXWILD7 f=", bench_frame, " cell=", tile_x >> macro::cell_shift, ",",
                                       tile_y >> macro::cell_shift, " zone=",
                                       int(macro::zone_at(tile_x >> macro::cell_shift, tile_y >> macro::cell_shift)),
                                       " deer=", deer, " rabbits=", rabbits, " birds=", _animals.birds(),
                                       " flying=", _animals.flock_flying());
                            }

                            // Migration check: an hour passes every 2 s, with a loud noise where the player is.
                            if((bench_frame + 1) % 120 == 0)
                            {
                                int hour = (_world_minute / 60) + ((bench_frame + 1) / 120);
                                hordes::noise(_player.position(), 30, hour);
                                hordes::hour_passed(hour, false);
                                int edges = 0;

                                for(int direction = 0; direction < hordes::edge_directions; ++direction)
                                {
                                    edges += hordes::edge_count(direction);
                                }

                                BN_LOG("KXHORDE hour=", hour, " world=", hordes::total_counted() + _zombies.count(),
                                       " near=", hordes::counted_near(_player.position()), " here=", _zombies.count(),
                                       " edges=", edges);
                            }
                        #endif

                        #if KX_BENCH == 8
                            // Save bench: the state before the save and right after the load must match.
                            auto log_state = [this](const char* label)
                            {
                                int tile_x = _player.position().x().integer() >> world::tile_shift;
                                int tile_y = (_player.position().y().integer() - 2) >> world::tile_shift;
                                int checksum;
                                int stacks = _containers.stored_stacks(checksum);
                                BN_LOG("KXSAVE ", label, " holding=", _containers.holding(), " records=",
                                       _containers.count(), " room=", _containers.room());
                                BN_LOG("KXSAVE ", label, " stacks=", stacks, " checksum=", checksum,
                                       " full=", storage_percent(), " containers%=", _containers.fill_percent(),
                                       " changes%=", world::changes_percent(), " pieces%=", base::pieces_percent());
                                // Milestone 13: the snare, the hunted cells and the Smoker's clock.
                                int snares = 0;

                                for(int trap = 0; trap < trap_tuning::max_traps; ++trap)
                                {
                                    snares += traps::snare(trap);
                                }

                                BN_LOG("KXSAVE ", label, " wild: snares=", snares, " traps=", traps::count(),
                                       " hunted cells=", _animals.kills_listed(), " since smoke=",
                                       _player.body().minutes_since_smoke());
                                BN_LOG("KXSAVE ", label, " x=", _player.position().x().integer(), " y=",
                                       _player.position().y().integer(), " minute=", _world_minute, " kills=", _kills,
                                       " weight=", _player.items().weight(), " food=", _player.body().food_percent(),
                                       " world=", hordes::total_counted() + _zombies.count(), " containers=",
                                       _containers.count(), " ground=", int(world::ground_gfx(tile_x, tile_y)));

                                // Milestone 12.5's saved state: what is explored, the marks, the flashlight in the bag
                                // (item extra), a stitched wound, the traits past the old 16-bit mask.
                                int explored = 0;

                                for(int y = 0; y < world_map::chunks; ++y)
                                {
                                    for(int x = 0; x < world_map::chunks; ++x)
                                    {
                                        explored += world_map::explored(x, y);
                                    }
                                }

                                int mark_x = -1;
                                int mark_y = -1;

                                if(world_map::mark_count())
                                {
                                    world_map::mark(world_map::mark_count() - 1, mark_x, mark_y);
                                }

                                const item_stack* torch = _held_flashlight();
                                BN_LOG("KXSAVE ", label, " explored=", explored, " marks=", world_map::mark_count(),
                                       " last_mark=", mark_x, ",", mark_y, " off hand=",
                                       int(_player.items().equipped(equip_slot::OFF_HAND).id), " torch=",
                                       torch && light::flashlight_on(*torch), "/",
                                       torch ? light::flashlight_charge(*torch) : -1, " stitched=",
                                       _player.body().wound(body_part::TORSO).stitched, " keen=",
                                       _player.skills().has(trait::KEEN_HEARING), " eagle=",
                                       _player.skills().has(trait::EAGLE_EYED), " hearing=",
                                       _player.skills().hearing_percent());

                                // Milestone 12.7: the doors, the stump and its canopy, the automatic marks, the safe.
                                // (The remembered changes, as the save keeps them: their chunks may not be generated.)
                                int safe_container = bench_safe[0] >= 0 ?
                                                     _containers.at(bench_safe[0], bench_safe[1]) : -1;
                                BN_LOG("KXSAVE ", label, " door ", bench_open_door[0], ",", bench_open_door[1], "=",
                                       world::remembered_gfx(world::layer::WALLS, bench_open_door[0],
                                                             bench_open_door[1]), " door ", bench_broken_door[0], ",",
                                       bench_broken_door[1], "=",
                                       world::remembered_gfx(world::layer::WALLS, bench_broken_door[0],
                                                             bench_broken_door[1]), " stump ", bench_stump[0], ",",
                                       bench_stump[1], "=",
                                       world::remembered_gfx(world::layer::WALLS, bench_stump[0], bench_stump[1]),
                                       " canopy=",
                                       world::remembered_gfx(world::layer::CANOPY, bench_stump[0], bench_stump[1]),
                                       " auto marks=", world_map::auto_mark_count(), " key mark=",
                                       world_map::has_auto_mark(world_map::auto_mark::KEY, 1234), " note mark=",
                                       world_map::has_auto_mark(world_map::auto_mark::NOTE, 4321), " safe opened=",
                                       safe_container >= 0 ? int(_containers.record(safe_container).opened) : -1);

                                // Milestone 12.6: the gun in hand (its rounds and jam live in the item), and the
                                // spear and a Molotov in the bag.
                                const item_stack& hand = _player.items().equipped(equip_slot::HAND);
                                BN_LOG("KXSAVE ", label, " hand=", info(hand.id).name, " loaded=",
                                       is_gun(hand.id) ? gun_loaded(hand) : -1, " jammed=",
                                       is_gun(hand.id) && gun_jammed(hand), " carried 9mm=",
                                       _player.items().count(item_id::AMMO_9MM), " spears=",
                                       _player.items().count(item_id::SPEAR), " molotovs=",
                                       _player.items().count(item_id::MOLOTOV), " spear level=",
                                       _player.skills().level(skill::SPEAR));
                            };

                            if(bench_frame == 1 && ! _loaded)
                            {
                                // The timeline for this seed: shutoff days, weather hours over 200 days, seasons.
                                int hours[4] = {};

                                for(int hour = 0; hour < 200 * 24; ++hour)
                                {
                                    ++hours[int(weather::at(hour * 60))];
                                }

                                BN_LOG("KXTIME water_off=", timeline::water_off_day(), " power_off=",
                                       timeline::power_off_day(), " clear=", hours[0], " rain=", hours[1], " fog=",
                                       hours[2], " storm=", hours[3]);
                                BN_LOG("KXTIME season temp day1=", weather::season_temp(1), " day60=",
                                       weather::season_temp(60), " day100=", weather::season_temp(100), " day150=",
                                       weather::season_temp(150), " autumn100=", weather::autumn_percent(100),
                                       " winter160=", weather::winter_percent(160));
                            }

                            if(! _loaded && bench_frame >= 60 && bench_frame <= 860 && bench_frame % 100 == 60)
                            {
                                // Vehicles the seed put around the route.
                                BN_LOG("KXCAR seeded active=", _vehicles.active_count(), " cars=",
                                       int(_vehicles.first(vehicle_kind::CAR) >= 0), " wrecks=",
                                       int(_vehicles.first(vehicle_kind::WRECK) >= 0), " bicycles=",
                                       int(_vehicles.first(vehicle_kind::BICYCLE) >= 0), " mossy=",
                                       _vehicles.mossy_count(), " x=",
                                       _player.position().x().integer(), " y=", _player.position().y().integer());
                            }

                            if(bench_frame == 870 && ! _loaded)
                            {
                                // A car beside the player, to be found again after the load.
                                spawn_vehicle(0);
                            }

                            if(bench_frame == 879 && ! _loaded)
                            {
                                BN_LOG("KXCAR saved tracked=", _vehicles.tracked_count(), " checksum=",
                                       _vehicles.tracked_checksum());
                            }

                            if(bench_frame == 40 && _loaded && ! _new_survivor)
                            {
                                BN_LOG("KXCAR loaded tracked=", _vehicles.tracked_count(), " checksum=",
                                       _vehicles.tracked_checksum(), " active=", _vehicles.active_count(), " car=",
                                       _vehicles.first(vehicle_kind::CAR));
                            }

                            if(bench_frame == 199 && ! _loaded)
                            {
                                // Milestone 14: the item pool past 450 containers that hold something (stashes
                                // opened in a far corner of the map), so the round trip carries a full world.
                                for(int index = 0; index < 600 && _containers.holding() < 470; ++index)
                                {
                                    int container = _containers.add(container_kind::FURNITURE, loot_table::STASH,
                                                                    3000 + ((index % 30) * 3), 3000 + ((index / 30) * 3));

                                    if(container < 0)
                                    {
                                        break;
                                    }

                                    static_cast<void>(_containers.open(container, day()));
                                }

                                BN_LOG("KXSAVE pool filled: holding=", _containers.holding(), " records=",
                                       _containers.count(), " room=", _containers.room(), " full=",
                                       _containers.fill_percent());
                            }

                            if(bench_frame == 200 && ! _loaded)
                            {
                                // Base building: a barricade on a window, a crate and a campfire on open grass,
                                // as build mode would place them.
                                int px = _player.position().x().integer() >> world::tile_shift;
                                int py = (_player.position().y().integer() - 1) >> world::tile_shift;
                                auto open_grass = [this](int x, int y)
                                {
                                    return world::wall_gfx(x, y) == world_gfx::EMPTY && ! world::solid(x, y) &&
                                           world::terrain_at(x, y) == terrain::GRASS && _containers.at(x, y) < 0;
                                };

                                int windows = 0;

                                for(int y = py - 40; y <= py + 40; ++y)
                                {
                                    for(int x = px - 40; x <= px + 40; ++x)
                                    {
                                        world_gfx wall = world::wall_gfx(x, y);
                                        windows += wall == world_gfx::WINDOW_FACE || wall == world_gfx::WINDOW_TOP;
                                    }
                                }

                                BN_LOG("KXBASE windows in range=", windows);

                                if(bench_find(px, py, 40, bench_barricade_x, bench_barricade_y, [](int x, int y)
                                   { return world::wall_gfx(x, y) == world_gfx::WINDOW_FACE ||
                                            world::wall_gfx(x, y) == world_gfx::WINDOW_TOP; }))
                                {
                                    bool face = world::wall_gfx(bench_barricade_x, bench_barricade_y) ==
                                                world_gfx::WINDOW_FACE;
                                    static_cast<void>(base::add(piece_kind::BARRICADE, bench_barricade_x,
                                                                bench_barricade_y, face ? covered::WINDOW_FACE :
                                                                covered::WINDOW_TOP, 100, _world_minute));
                                    static_cast<void>(world::set_wall_gfx(bench_barricade_x, bench_barricade_y,
                                                                          face ? world_gfx::BARRICADE_FACE :
                                                                                 world_gfx::BARRICADE_TOP));
                                }

                                if(bench_find(px + 2, py, 6, bench_crate_x, bench_crate_y, open_grass))
                                {
                                    static_cast<void>(base::add(piece_kind::CRATE, bench_crate_x, bench_crate_y,
                                                                covered::DOOR_FACE, 0, _world_minute));
                                    static_cast<void>(world::set_wall_gfx(bench_crate_x, bench_crate_y,
                                                                          world_gfx::CRATE));
                                    static_cast<void>(_containers.add(container_kind::CRATE, loot_table::CRATE,
                                                                      bench_crate_x, bench_crate_y));
                                }

                                if(bench_find(px - 3, py + 2, 6, bench_fire_x, bench_fire_y, open_grass))
                                {
                                    static_cast<void>(base::add(piece_kind::CAMPFIRE, bench_fire_x, bench_fire_y,
                                                                covered::DOOR_FACE, ((_world_minute / 60) + 6) & 0xFF,
                                                                _world_minute));
                                    static_cast<void>(world::set_wall_gfx(bench_fire_x, bench_fire_y,
                                                                          world_gfx::CAMPFIRE));
                                }

                                refresh_tile(bench_barricade_x, bench_barricade_y);
                                refresh_tile(bench_crate_x, bench_crate_y);
                                refresh_tile(bench_fire_x, bench_fire_y);
                                BN_LOG("KXBASE built barricade=", bench_barricade_x, ",", bench_barricade_y, " crate=",
                                       bench_crate_x, ",", bench_crate_y, " fire=", bench_fire_x, ",", bench_fire_y,
                                       " pieces=", base::pieces_at(bench_crate_x, bench_crate_y));

                                // Build mode open for a moment (the HUD dump shows its bar), with materials.
                                item_stack planks = make_stack(item_id::PLANKS, 4);
                                item_stack nails = make_stack(item_id::NAILS, 12);
                                item_stack hammer = make_stack(item_id::HAMMER);
                                static_cast<void>(_player.items().add(planks));
                                static_cast<void>(_player.items().add(nails));
                                static_cast<void>(_player.items().add(hammer));
                                begin_build();
                            }
                            else if(bench_frame == 260 && ! _loaded)
                            {
                                _build.close(*this);
                            }
                            else if(bench_frame == 50 && _loaded && ! _new_survivor && bench_barricade_x >= 0)
                            {
                                // After the load: the pieces are back; zombie hits wear the barricade down, and a
                                // window smashes after three.
                                teleport_to_tile(bench_barricade_x, bench_barricade_y + 2);
                                int full;
                                int before = base::barricade_health(bench_barricade_x, bench_barricade_y, full);
                                BN_LOG("KXBASE loaded barricade_gfx=", int(world::wall_gfx(bench_barricade_x,
                                       bench_barricade_y)), " health=", before, "/", full, " crate_gfx=",
                                       int(world::wall_gfx(bench_crate_x, bench_crate_y)), " crate_container=",
                                       _containers.at(bench_crate_x, bench_crate_y), " fire_lit=",
                                       int(base::fire_lit(bench_fire_x, bench_fire_y, _world_minute / 60)));

                                for(int hit = 0; hit < 3; ++hit)
                                {
                                    bash(bench_barricade_x, bench_barricade_y, _player.position());
                                }

                                int window_x;
                                int window_y;
                                bool window = bench_find(bench_barricade_x, bench_barricade_y, 40, window_x, window_y,
                                                         [](int x, int y)
                                                         { return world::wall_gfx(x, y) == world_gfx::WINDOW_FACE ||
                                                                  world::wall_gfx(x, y) == world_gfx::WINDOW_TOP; });

                                for(int hit = 0; hit < 3 && window; ++hit)
                                {
                                    bash(window_x, window_y, _player.position());
                                }

                                BN_LOG("KXBASE after hits barricade=", base::barricade_health(bench_barricade_x,
                                       bench_barricade_y, full), " window_gfx=",
                                       window ? int(world::wall_gfx(window_x, window_y)) : -1, " broken_face=",
                                       int(world_gfx::BROKEN_FACE), " breakable=", int(window && world::breakable(window_x,
                                       window_y)));
                            }

                            if(bench_frame == 0 && _loaded)
                            {
                                log_state(_new_survivor ? "new survivor" : "loaded");
                                int gear = _containers.at(survivors::gear_tile_x, 0);
                                int gear_stacks = 0;

                                if(gear >= 0)
                                {
                                    for(const item_stack& stack : _containers.get(gear).items)
                                    {
                                        gear_stacks += ! stack.empty();
                                    }
                                }

                                BN_LOG("KXSAVE survivors deaths=", survivors::deaths(), " gear=", gear,
                                       " gear_stacks=", gear_stacks);

                                // Playtest 2: scramble the settings, then read them back from the save's header.
                                {
                                    constexpr uint8_t scrambled[settings::save_bytes] = { 0x53, 0, 1, 1, 0 };
                                    settings::unpack(scrambled);
                                    static_cast<void>(save::read_summary());
                                    BN_LOG("KXSETTINGS loaded music=", settings::value(settings::setting::MUSIC),
                                           " sound=", settings::value(settings::setting::SOUND), " shake=",
                                           settings::value(settings::setting::SHAKE), " cpu=",
                                           settings::value(settings::setting::CPU));
                                }

                                const character& skills = _player.skills();
                                BN_LOG("KXSAVE sandbox ", sandbox::value(sandbox::setting::ZOMBIE_SPEED), ",",
                                       sandbox::value(sandbox::setting::ZOMBIES), ",",
                                       sandbox::value(sandbox::setting::LOOT), ",",
                                       sandbox::value(sandbox::setting::SHUTOFF), ",",
                                       sandbox::value(sandbox::setting::DAY_LENGTH), " since smoke=",
                                       _player.body().minutes_since_smoke());
                                BN_LOG("KXSAVE character job=", info(skills.job()).name, " strong=",
                                       int(skills.has(trait::STRONG)), " axe=", skills.level(skill::AXE),
                                       " fitness=", skills.level(skill::FITNESS), " strength=",
                                       skills.level(skill::STRENGTH), " capacity=", _player.items().capacity());
                            }
                            else if(bench_frame == 150 && _loaded && ! _new_survivor)
                            {
                                // Once the chunks round the loaded position are generated: milestone 12.7's tiles.
                                log_state("loaded, chunks ready");
                            }
                            else if(bench_frame == 300 && _loaded && ! _new_survivor)
                            {
                                // The loaded survivor dies; the next one starts in a village.
                                BN_LOG("KXSAVE dies at x=", _player.position().x().integer(), " y=",
                                       _player.position().y().integer());
                                _player.die();
                                player_bitten();
                            }
                            else if(bench_frame == 10 && _new_survivor)
                            {
                                // Reading: an Axe book, volume 1 (the band the fire officer's Axe 1 needs is 2).
                                item_stack book = make_stack(item_id::BOOK_AXE);
                                book.extra = 1;
                                static_cast<void>(_player.items().add(book));
                                bool started = begin_reading(skill::AXE, 1);
                                BN_LOG("KXSAVE reading started=", int(started), " minute=", _world_minute);
                            }
                            else if(bench_frame == 20 && _new_survivor)
                            {
                                const character& skills = _player.skills();
                                BN_LOG("KXSAVE read axe vol1=", int(skills.read(skill::AXE, 1)), " vol2=",
                                       int(skills.read(skill::AXE, 2)), " multiplier=",
                                       skills.book_multiplier(skill::AXE), " minute=", _world_minute);
                            }
                            else if(bench_frame == 30 && _new_survivor)
                            {
                                // Back towards where the last one died: their zombie appears off-screen.
                                int tile_x;
                                int tile_y;
                                survivors::start_tile(tile_x, tile_y);
                                BN_LOG("KXSAVE start village x=", tile_x, " y=", tile_y);
                                int survivor_tile_x;
                                int survivor_tile_y;
                                survivors::position(0, survivor_tile_x, survivor_tile_y);
                                teleport_to_tile(survivor_tile_x + 14, survivor_tile_y);
                            }
                            else if(bench_frame == 100 && _new_survivor)
                            {
                                // World events on demand: the helicopter, a crash site, an alarm; and the radio.
                                _events.trigger(*this, 0);
                                _events.trigger(*this, 3);
                                _events.trigger(*this, 2);
                                item_stack radio = make_stack(item_id::RADIO);
                                item_stack battery = make_stack(item_id::BATTERY);
                                static_cast<void>(_player.items().add(radio));
                                static_cast<void>(_player.items().add(battery));
                                listen_radio();
                            }
                            else if(bench_frame == 140 && _new_survivor)
                            {
                                // Foraging where the player stands: the finds, then the patch goes on closing.
                                int before = _containers.count();
                                int tile_x = _player.position().x().floor_integer() >> world::tile_shift;
                                int tile_y = (_player.position().y().floor_integer() - 2) >> world::tile_shift;
                                forage();
                                int patch = _containers.at(tile_x, tile_y);
                                int finds = 0;

                                if(patch >= 0)
                                {
                                    for(const item_stack& stack : _containers.get(patch).items)
                                    {
                                        finds += ! stack.empty();
                                    }
                                }

                                BN_LOG("KXFORAGE terrain=", int(world::terrain_at(tile_x, tile_y)), " patch=", patch,
                                       " finds=", finds, " searching=", int(_search.active()));
                                _search.close(*this);
                                BN_LOG("KXFORAGE containers before=", before, " after=", _containers.count(),
                                       " foraging xp=", _player.skills().xp(skill::FORAGING));
                            }
                            else if(bench_frame == 145 && _new_survivor)
                            {
                                // Cooking: a gas stove and the makings of a vegetable soup; the Craft tab open a moment.
                                for(item_id id : { item_id::GAS_STOVE, item_id::PROPANE, item_id::COOKING_POT,
                                                   item_id::HERBS, item_id::MUSHROOMS, item_id::WATER_BOTTLE })
                                {
                                    give_item(id);
                                }

                                const char* problem;
                                bool ready = recipe_ready(2, problem);
                                BN_LOG("KXCOOK heat=", heat_name(heat_source()), " soup ready=", int(ready), " problem=",
                                       problem ? problem : "-");
                                _open_tab(2);
                            }
                            else if(bench_frame == 185 && _new_survivor)
                            {
                                close_menus();
                                cook(2);
                            }
                            else if(bench_frame == 190 && _new_survivor)
                            {
                                BN_LOG("KXCOOK soup=", _player.items().count(item_id::SOUP), " herbs=",
                                       _player.items().count(item_id::HERBS), " cooking xp=",
                                       _player.skills().xp(skill::COOKING), " minute=", _world_minute);
                            }
                            else if(bench_frame == 210 && _new_survivor)
                            {
                                // Farming: dig two plots next to the player, plant carrots in both. One is watered
                                // once and left (it rots), the other every day until it is ripe and harvested.
                                for(item_id id : { item_id::SHOVEL, item_id::SEEDS_CARROT, item_id::WATERING_CAN })
                                {
                                    give_item(id);
                                }

                                int feet_x = _player.position().x().floor_integer() >> world::tile_shift;
                                int feet_y = (_player.position().y().floor_integer() - 2) >> world::tile_shift;
                                int plots_x[2];
                                int plots_y[2];
                                int found = 0;

                                for(int y = feet_y - 8; y <= feet_y + 8 && found < 2; ++y)
                                {
                                    for(int x = feet_x - 8; x <= feet_x + 8 && found < 2; ++x)
                                    {
                                        terrain ground = world::terrain_at(x, y);

                                        if((ground == terrain::GRASS || ground == terrain::SOIL ||
                                            ground == terrain::MUD || ground == terrain::CROPS) &&
                                           world::wall_gfx(x, y) == world_gfx::EMPTY && ! world::solid(x, y) &&
                                           _containers.at(x, y) < 0 && (x != feet_x || y != feet_y))
                                        {
                                            plots_x[found] = x;
                                            plots_y[found] = y;
                                            ++found;
                                        }
                                    }
                                }

                                int today = day();

                                for(int which = 0; which < found; ++which)
                                {
                                    static_cast<void>(farm::add(plots_x[which], plots_y[which], today));
                                    static_cast<void>(world::set_wall_gfx(plots_x[which], plots_y[which],
                                                                          world_gfx::PLOT_SOIL));
                                    _tend_at(plots_x[which], plots_y[which]);           // plant
                                    _tend_at(plots_x[which], plots_y[which]);           // water
                                }

                                if(found == 2)
                                {
                                    int left = farm::at(plots_x[0], plots_y[0]);
                                    int kept = farm::at(plots_x[1], plots_y[1]);
                                    BN_LOG("KXFARM planted crop=", int(farm::crop_of(left)), " stage=",
                                           int(farm::stage(left, today)), " wet=", int(farm::wet(left, today)),
                                           " can=", _watering_can() ? _watering_can()->amount : 0, " plots=",
                                           farm::count());
                                    BN_LOG("KXFARM left day+1 stage=", int(farm::stage(left, today + 1)),
                                           " day+8 stage=", int(farm::stage(left, today + 8)));

                                    for(int later = 1; later <= 10; ++later)
                                    {
                                        farm::water(kept, today + later);
                                    }

                                    BN_LOG("KXFARM kept day+5 stage=", int(farm::stage(kept, today + 5)),
                                           " day+11 stage=", int(farm::stage(kept, today + 11)));
                                    _tend_at(plots_x[1], plots_y[1]);                   // harvest
                                    _tend_at(plots_x[0], plots_y[0]);                   // clear the rotten one
                                    BN_LOG("KXFARM harvest carrots=", _player.items().count(item_id::CARROT),
                                           " seeds=", _player.items().count(item_id::SEEDS_CARROT), " stages=",
                                           int(farm::stage(kept, today)), ",", int(farm::stage(left, today)),
                                           " farming xp=", _player.skills().xp(skill::FARMING));
                                }
                                else
                                {
                                    BN_LOG("KXFARM no room for plots terrain=", int(world::terrain_at(feet_x, feet_y)));
                                }
                            }
                            else if(bench_frame == 240 && _new_survivor)
                            {
                                // Generators, traps and can lines: placed on free tiles next to the player.
                                _player.skills().learn(magazine::GENERATOR);
                                _player.skills().learn(magazine::TRAPPING);
                                give_item(item_id::FUEL_CAN);
                                int feet_x = _player.position().x().floor_integer() >> world::tile_shift;
                                int feet_y = (_player.position().y().floor_integer() - 2) >> world::tile_shift;
                                int spots_x[3];
                                int spots_y[3];
                                int found = 0;

                                for(int y = feet_y - 8; y <= feet_y + 8 && found < 3; ++y)
                                {
                                    for(int x = feet_x - 8; x <= feet_x + 8 && found < 3; ++x)
                                    {
                                        terrain ground = world::terrain_at(x, y);

                                        if((ground == terrain::GRASS || ground == terrain::MUD) &&
                                           world::wall_gfx(x, y) == world_gfx::EMPTY && ! world::solid(x, y) &&
                                           _containers.at(x, y) < 0 && (x != feet_x || y != feet_y))
                                        {
                                            spots_x[found] = x;
                                            spots_y[found] = y;
                                            ++found;
                                        }
                                    }
                                }

                                if(found == 3)
                                {
                                    int now = _world_minute;
                                    static_cast<void>(power::add(spots_x[0], spots_y[0], now));
                                    static_cast<void>(world::set_wall_gfx(spots_x[0], spots_y[0], world_gfx::GENERATOR));
                                    static_cast<void>(traps::add(spots_x[1], spots_y[1], day()));
                                    static_cast<void>(world::set_wall_gfx(spots_x[1], spots_y[1], world_gfx::TRAP));
                                    static_cast<void>(base::add(piece_kind::CAN_LINE, spots_x[2], spots_y[2],
                                                                covered::DOOR_FACE, 0, now));
                                    static_cast<void>(world::set_wall_gfx(spots_x[2], spots_y[2], world_gfx::CAN_LINE));

                                    for(int spot = 0; spot < 3; ++spot)
                                    {
                                        refresh_tile(spots_x[spot], spots_y[spot]);
                                    }

                                    int generator = power::at(spots_x[0], spots_y[0]);
                                    const char* before = _device_prompt(spots_x[0], spots_y[0]);
                                    _tend_at(spots_x[0], spots_y[0]);       // fuel
                                    const char* after = _device_prompt(spots_x[0], spots_y[0]);
                                    _tend_at(spots_x[0], spots_y[0]);       // on
                                    int can_slot = _player.items().find(item_id::FUEL_CAN);
                                    BN_LOG("KXPOWER prompts=", before, "|", after, " fuel=", power::fuel(generator, now),
                                           " can=", can_slot >= 0 ? int(_player.items().slot(can_slot).amount) : -1,
                                           " running=", int(power::running(generator, now)), " generators=",
                                           power::count());
                                    BN_LOG("KXPOWER powered near=", int(power::powered(spots_x[0] + 9, spots_y[0], now)),
                                           " far=", int(power::powered(spots_x[0] + 11, spots_y[0], now)),
                                           " light=", int(_reading_light()), " heat=", heat_name(heat_source()));
                                    BN_LOG("KXPOWER fuel after 10h=", power::fuel(generator, now + 600), " after 40h=",
                                           power::fuel(generator, now + 2400), " running=",
                                           int(power::running(generator, now + 2400)));

                                    int trap = traps::at(spots_x[1], spots_y[1]);
                                    int rolled_day = day();

                                    while(! traps::caught(trap, rolled_day, 0) && rolled_day < day() + 30)
                                    {
                                        ++rolled_day;
                                    }

                                    int meat_before = _player.items().count(item_id::RAW_MEAT);
                                    _tend_at(spots_x[1], spots_y[1]);
                                    BN_LOG("KXTRAP caught after nights=", rolled_day - day(), " meat=", meat_before,
                                           "->", _player.items().count(item_id::RAW_MEAT), " caught now=",
                                           int(traps::caught(trap, day(), 0)), " foraging xp=",
                                           _player.skills().xp(skill::FORAGING), " traps=", traps::count());

                                    int lines_x[8];
                                    int lines_y[8];
                                    BN_LOG("KXTRAP can lines near=", base::can_lines_in(feet_x - 10, feet_y - 10,
                                                                                         feet_x + 10, feet_y + 10,
                                                                                         lines_x, lines_y, 8),
                                           " pieces=", base::pieces_at(spots_x[2], spots_y[2]));
                                }
                                else
                                {
                                    BN_LOG("KXPOWER no room");
                                }
                            }
                            else if(bench_frame == 211 && _new_survivor)
                            {
                                begin_build(true);      // the HUD dump shows the bar scrolled to the farm plot
                                BN_LOG("KXFARM build bar open=", int(_build.active()), " sleep=", int(_sleep_state),
                                       " search=", int(_search.active()));
                            }
                            else if(bench_frame == 220 && _new_survivor)
                            {
                                BN_LOG("KXFARM build bar still open=", int(_build.active()));
                            }
                            else if(bench_frame == 236 && _new_survivor)
                            {
                                _build.close(*this);
                            }
                            else if(bench_frame == 160 && _new_survivor)
                            {
                                BN_LOG("KXTIME events heli=", int(_events.helicopter_flying()), " crashes=",
                                       _events.crashes(), " smoke=", int(_events.smoke_shown()), " weather=",
                                       int(_weather), " world=", hordes::total_counted() + _zombies.count());
                            }
                            else if(bench_frame == 200 && _new_survivor)
                            {
                                int walking = -1;

                                for(int index = 0; index < _zombies.count(); ++index)
                                {
                                    if(_zombies.survivor(index) >= 0)
                                    {
                                        walking = index;
                                    }
                                }

                                BN_LOG("KXSAVE survivor zombie=", walking, " of ", _zombies.count());
                            }
                            else if(bench_frame == 400 && ! _loaded)
                            {
                                _save(false);
                                BN_LOG("KXSAVE autosave started");
                            }
                            else if(bench_frame == 870 && ! _loaded)
                            {
                                // Stress the store: 300 corpses opened far away, more than there are item pages,
                                // so the oldest corpse pages give way (the floor pile below must survive).
                                for(int corpse = 0; corpse < 300; ++corpse)
                                {
                                    int index = _containers.add(container_kind::CORPSE, loot_table::SOLDIER_CORPSE,
                                                                8 + (corpse % 100), 8 + (corpse / 100));

                                    if(index >= 0)
                                    {
                                        static_cast<void>(_containers.open(index, day()));
                                    }
                                }
                            }
                            else if(bench_frame == 880 && ! _loaded)
                            {
                                // Milestone 12.5 data for the round trip: two marks, a flashlight on with 123
                                // minutes left in the off hand (12.6), a stitched deep wound, two of the new traits
                                // (bits 16 and 20). Milestone 12.6: a jammed pistol with 7 rounds in hand, 9mm, a
                                // spear and a Molotov in the bag.
                                static_cast<void>(world_map::toggle_mark(10, 20));
                                static_cast<void>(world_map::toggle_mark(70, 90));
                                item_stack torch = make_stack(item_id::FLASHLIGHT);
                                light::set_flashlight(torch, true, 123);
                                static_cast<void>(_player.items().unequip(equip_slot::HAND));
                                _player.items().equipped(equip_slot::OFF_HAND) = torch;
                                item_stack pistol = make_stack(item_id::PISTOL);
                                set_gun_loaded(pistol, 7);
                                set_gun_jammed(pistol, true);
                                _player.items().equipped(equip_slot::HAND) = pistol;

                                for(item_id id : { item_id::SPEAR, item_id::MOLOTOV, item_id::AMMO_9MM })
                                {
                                    item_stack stack = make_stack(id, 12);
                                    static_cast<void>(_player.items().add(stack));
                                }
                                _player.body().add_wound(body_part::TORSO, wound_type::DEEP_WOUND, false, _random);
                                item_stack needle = make_stack(item_id::SUTURE_NEEDLE);
                                static_cast<void>(_player.items().add(needle));
                                static_cast<void>(_player.body().treat(body_part::TORSO, _player.items(), true));
                                _player.skills().start(occupation::UNEMPLOYED, (1u << int(trait::KEEN_HEARING)) |
                                                       (1u << int(trait::EAGLE_EYED)));
                                static_cast<void>(drop_item(make_stack(item_id::APPLE)));
                                _kills = 7;

                                // Milestone 12.7: the nearest closed doors (one opened, one broken), a tree chopped,
                                // a gun safe opened, a key's and a note's marks.
                                int tile_x = _player.position().x().integer() >> world::tile_shift;
                                int tile_y = (_player.position().y().integer() - 1) >> world::tile_shift;

                                for(int radius = 1; radius < 40; ++radius)
                                {
                                    for(int y = tile_y - radius; y <= tile_y + radius; ++y)
                                    {
                                        for(int x = tile_x - radius; x <= tile_x + radius; ++x)
                                        {
                                            if(world::tree_at(x, y) && bench_stump[0] < 0)
                                            {
                                                bench_stump[0] = x;
                                                bench_stump[1] = y;
                                                static_cast<void>(world::chop_tree(x, y));
                                            }
                                        }
                                    }
                                }

                                // Doors and a safe of the start settlement, found from the seed (their chunks need
                                // not be generated): the first house door opened, the next broken, the first safe
                                // opened (its container registered as facing it would).
                                uint16_t ids[64];
                                kx::building kinds[64];

                                for(int town = macro::start_settlement(), tries = 0;
                                    tries < macro::settlement_count() &&
                                    (bench_safe[0] < 0 || bench_broken_door[0] < 0);
                                    town = (town + 1) % macro::settlement_count(), ++tries)
                                {
                                    int count = world::settlement_buildings(town, ids, kinds, 64);

                                    for(int index = 0; index < count; ++index)
                                    {
                                        int x;
                                        int y;
                                        bool side_wall = false;

                                        if(world::house_door(ids[index], x, y, &side_wall))
                                        {
                                            int* door = bench_open_door[0] < 0 ? bench_open_door :
                                                        bench_broken_door[0] < 0 ? bench_broken_door : nullptr;

                                            if(door)
                                            {
                                                door[0] = x;
                                                door[1] = y;
                                                static_cast<void>(world::set_wall_gfx(x, y, world::door_gfx(
                                                    door == bench_open_door ? world::door_state::OPEN :
                                                                              world::door_state::BROKEN, side_wall)));
                                            }
                                        }

                                        if(bench_safe[0] < 0 && world::house_safe(ids[index], x, y))
                                        {
                                            int safe = _containers.add(container_kind::FURNITURE,
                                                                       loot_table::GUN_SAFE, x, y);

                                            if(safe >= 0)
                                            {
                                                bench_safe[0] = x;
                                                bench_safe[1] = y;
                                                static_cast<void>(_containers.open(safe, day()));
                                            }
                                        }
                                    }
                                }

                                world_map::add_auto_mark(world_map::auto_mark::KEY, 1234, tile_x + 40, tile_y);
                                world_map::add_auto_mark(world_map::auto_mark::NOTE, 4321, tile_x, tile_y + 40);

                                // Milestone 13: a snare beside the survivor, a deer killed with an axe (its cell
                                // hunted) and 100 minutes without a cigarette.
                                static_cast<void>(traps::add(tile_x + 1, tile_y + 1, day(), true));
                                static_cast<void>(_animals.spawn(*this, animal_kind::DEER,
                                                                 _player.position() + bn::fixed_point(12, 0)));

                                for(int hit = 0; hit < 4 && _animals.carcass_at(_player.position(), 40) < 0; ++hit)
                                {
                                    static_cast<void>(_animals.strike(*this, _player.position(),
                                                                      bn::fixed_point(1, 0), 30, 360, weapon::AXE));
                                }

                                survival_input craving;
                                craving.smoker = true;

                                for(int minute = 0; minute < 100; ++minute)
                                {
                                    _player.body().tick_minute(craving, _random);
                                }

                                // Playtest 2: the game's settings go into the save's header.
                                static_cast<void>(settings::step(settings::setting::MUSIC, -1));
                                static_cast<void>(settings::step(settings::setting::SHAKE, -1));
                                static_cast<void>(settings::step(settings::setting::CPU, 1));
                                BN_LOG("KXSETTINGS saved music=", settings::value(settings::setting::MUSIC), " sound=",
                                       settings::value(settings::setting::SOUND), " shake=",
                                       settings::value(settings::setting::SHAKE), " cpu=",
                                       settings::value(settings::setting::CPU));

                                log_state("saved");
                                _save(true);
                                return game_exit::QUIT;
                            }
                        #endif

                        #if KX_BENCH == 10
                            _bench_night(bench_frame);
                        #endif

                        #if KX_BENCH == 11
                            _bench_guns(bench_frame);
                        #endif

                        #if KX_BENCH == 13
                            _bench_wild(bench_frame);
                        #endif

                        #if KX_BENCH == 15
                            _bench_animals(bench_frame);
                        #endif

                        #if KX_BENCH == 16
                            _bench_animal_census(bench_frame);
                        #endif

                        #if KX_BENCH == 12
                            _bench_doors(bench_frame);
                            _bench_notes(bench_frame);
                        #endif

                        #if KX_BENCH == 9
                            // Vehicles bench: the car west of the start is looked over and fixed, then the input
                            // script drives it; afterwards the bicycle, then the wreck is stripped and siphoned.
                            static int stage = 0;
                            static int mark = 0;
                            auto log_car = [this](const char* label, int vehicle)
                            {
                                int fuel = 0;
                                int heading = 0;
                                int tyres = 0;
                                int charge = 0;

                                if(vehicle >= 0)
                                {
                                    _vehicles.numbers(vehicle, fuel, heading, tyres, charge);
                                }

                                BN_LOG("KXCAR ", label, " vehicle=", vehicle, " x=",
                                       vehicle >= 0 ? _vehicles.position(vehicle).x().integer() : -1, " y=",
                                       vehicle >= 0 ? _vehicles.position(vehicle).y().integer() : -1, " heading=",
                                       heading, " speed=", _vehicles.speed_milli(), " fuel_dl=", fuel, " tyres=", tyres,
                                       " charge=", charge, " condition=", vehicle >= 0 ? _vehicles.condition(vehicle) : -1,
                                       " kills=", _kills);
                            };

                            int car = _vehicles.first(vehicle_kind::CAR);

                            if(bench_frame == 0)
                            {
                                stage = 0;
                            }
                            else if(bench_frame == 10)
                            {
                                BN_LOG("KXCAR tracked=", _vehicles.tracked_count(), " active=", _vehicles.active_count(),
                                       " checksum=", _vehicles.tracked_checksum());
                                teleport_to_tile(6, 20);
                            }
                            else if(bench_frame == 30)
                            {
                                BN_LOG("KXCAR context=", _vehicle_context, " car=", car, " bicycle=",
                                       _vehicles.first(vehicle_kind::BICYCLE), " wreck=",
                                       _vehicles.first(vehicle_kind::WRECK), " active=", _vehicles.active_count());
                                use_vehicle();
                                BN_LOG("KXCAR screen=", int(_car.active()), " name=", car >= 0 ? _vehicles.name(car) : "-");
                            }
                            else if(bench_frame == 70 && car >= 0)
                            {
                                // What the car needs (mockup 10: a battery and fuel), then the parts and tools.
                                const char* problem;
                                bool fits = _vehicles.can_fit(*this, car, car_part::BATTERY, problem);
                                BN_LOG("KXCAR battery fit=", int(fits), " problem=", problem ? problem : "-");
                                bool drives = _vehicles.can_do(*this, car, car_action::DRIVE, problem);
                                BN_LOG("KXCAR drive=", int(drives), " problem=", problem ? problem : "-");

                                for(item_id id : { item_id::WRENCH, item_id::CAR_BATTERY, item_id::FUEL_CAN,
                                                   item_id::TYRE })
                                {
                                    give_item(id);
                                }

                                log_car("before", car);
                                _vehicles.fit(*this, car, car_part::BATTERY);
                            }
                            else if(bench_frame == 71 && car >= 0)
                            {
                                _vehicles.fit(*this, car, car_part::FUEL);
                            }
                            else if(bench_frame == 72 && car >= 0)
                            {
                                _vehicles.fit(*this, car, car_part::TYRES);
                            }
                            else if(bench_frame == 73 && car >= 0)
                            {
                                const char* problem;
                                log_car("fixed", car);
                                bool drives = _vehicles.can_do(*this, car, car_action::DRIVE, problem);
                                BN_LOG("KXCAR drive=", int(drives), " problem=", problem ? problem : "-", " minute=",
                                       _world_minute, " mechanics xp=", _player.skills().xp(skill::MECHANICS));
                                bool hotwire = _vehicles.can_do(*this, car, car_action::HOTWIRE, problem);
                                BN_LOG("KXCAR hotwire=", int(hotwire), " problem=", problem ? problem : "-");
                                give_item(item_id::CAR_KEY);
                                BN_LOG("KXCAR key=", int(_vehicles.has_key(*this, car)), " screen=", int(_car.active()));
                            }
                            else if(bench_frame == 74 && car >= 0)
                            {
                                // In and off: three zombies wait on the pavement ahead.
                                _car.close(*this);
                                _vehicles.act(*this, car, car_action::DRIVE);

                                for(int tile_x : { 13, 15, 17 })
                                {
                                    _zombies.spawn(tile_feet(tile_x, 19), _random);
                                }

                                BN_LOG("KXCAR driving=", _vehicles.driven(), " ride=", int(_player.ride()), " zombies=",
                                       _zombies.count());
                                stage = 1;
                            }

                            if(stage == 1)
                            {
                                if(_vehicles.driven() >= 0 && bench_frame % 20 == 0)
                                {
                                    log_car("drive", _vehicles.driven());
                                }
                                else if(_vehicles.driven() < 0)
                                {
                                    log_car("parked", car);
                                    BN_LOG("KXCAR out at x=", _player.position().x().integer(), " y=",
                                           _player.position().y().integer(), " lit=", int(_vehicles.lit()));
                                    stage = 2;
                                    mark = bench_frame;
                                }
                            }
                            else if(stage == 2 && bench_frame == mark + 20)
                            {
                                // The bicycle in the shed; a zombie stands outside its door.
                                _zombies.clear();
                                teleport_to_tile(23, 13);
                                _zombies.spawn(tile_feet(23, 17), _random);
                                stage = 3;
                            }
                            else if(stage == 3 && bench_frame == mark + 45)
                            {
                                int bicycle = _vehicles.first(vehicle_kind::BICYCLE);
                                bool mounted = bicycle >= 0 && _vehicles.mount(*this, bicycle);
                                BN_LOG("KXBIKE bicycle=", bicycle, " mounted=", int(mounted), " ride=",
                                       int(_player.ride()), " tracked=", _vehicles.tracked_count());
                                stage = 4;
                            }
                            else if(stage == 4)
                            {
                                if(! _vehicles.riding_bicycle())
                                {
                                    int bicycle = _vehicles.first(vehicle_kind::BICYCLE);
                                    BN_LOG("KXBIKE off at x=", _player.position().x().integer(), " y=",
                                           _player.position().y().integer(), " mode=", mode_name(_player.mode()),
                                           " bicycle x=", bicycle >= 0 ? _vehicles.position(bicycle).x().integer() : -1,
                                           " y=", bicycle >= 0 ? _vehicles.position(bicycle).y().integer() : -1);
                                    stage = 5;
                                    mark = bench_frame;
                                }
                                else if(bench_frame % 20 == 0)
                                {
                                    BN_LOG("KXBIKE x=", _player.position().x().integer(), " y=",
                                           _player.position().y().integer(), " speed=",
                                           (_player.speed() * 1000).integer(), " ground=", info(_player.ground()).name,
                                           " noise=", _player.noise_radius());
                                }
                            }
                            else if(stage == 5 && bench_frame == mark + 60)
                            {
                                // The wreck: a tyre off it, then its fuel through a hose into an empty can.
                                _zombies.clear();
                                teleport_to_tile(29, 47);
                                stage = 6;
                            }
                            else if(stage == 6 && bench_frame == mark + 90)
                            {
                                int wreck = _vehicles.first(vehicle_kind::WRECK);
                                give_item(item_id::HOSE);

                                if(item_stack* can = _fuel_can(true))
                                {
                                    can->amount = 0;
                                }

                                log_car("wreck", wreck);

                                if(wreck >= 0)
                                {
                                    _vehicles.remove(*this, wreck, car_part::TYRES);
                                }

                                stage = 7;
                            }
                            else if(stage == 7 && bench_frame == mark + 91)
                            {
                                int wreck = _vehicles.first(vehicle_kind::WRECK);

                                if(wreck >= 0)
                                {
                                    _vehicles.act(*this, wreck, car_action::SIPHON);
                                }

                                stage = 8;
                            }
                            else if(stage == 8 && bench_frame == mark + 92)
                            {
                                int wreck = _vehicles.first(vehicle_kind::WRECK);
                                item_stack* can = _fuel_can(true);
                                log_car("stripped", wreck);
                                BN_LOG("KXCAR siphoned can=", can ? int(can->amount) : 0, " tyres carried=",
                                       _player.items().count(item_id::TYRE), " mechanics xp=",
                                       _player.skills().xp(skill::MECHANICS), " tracked=", _vehicles.tracked_count(),
                                       " checksum=", _vehicles.tracked_checksum());
                                int trunk = wreck >= 0 ? _vehicles.container_of(*this, wreck, true) : -1;
                                int glove = wreck >= 0 ? _vehicles.container_of(*this, wreck, false) : -1;
                                BN_LOG("KXCAR trunk=", trunk, " items=", trunk >= 0 ? _containers.open(trunk, day()) : -1,
                                       " glove=", glove, " items=", glove >= 0 ? _containers.open(glove, day()) : -1);
                                _car.close(*this);

                                // Playtest 4: the trunk searched from the car screen, B, and the car screen is back.
                                if(trunk >= 0)
                                {
                                    search_car_container(trunk, wreck);
                                    input_frame back;
                                    back.b_pressed = true;
                                    bool searched = _search.active();
                                    _update_search(back);
                                    BN_LOG("KXCAR trunk from the car screen: searched=", searched, " then B: search=",
                                           int(_search.active()), " car screen=", int(_car.active()));
                                    _car.close(*this);
                                }

                                stage = 9;
                            }
                        #endif

                        #if KX_RENDER_CHECK
                            // Milestone 14: the renderer's visible cells against the world (stale cells after
                            // scrolling, effects or teleports).
                            if(bench_frame % 30 == 0)
                            {
                                static int checks = 0;
                                static int stale_checks = 0;
                                int stale = _renderer.verify();
                                ++checks;
                                stale_checks += stale > 0;

                                if(stale || checks % 20 == 0)
                                {
                                    BN_LOG("KXRENDER f=", bench_frame, " stale cells=", stale, " checks=", checks,
                                           " with stale=", stale_checks, " camera=", _camera_controller.left(), ",",
                                           _camera_controller.top());
                                }
                            }
                        #endif

                        if(++bench_frame % 60 == 0)
                        {
                            BN_LOG("KXPOS f=", bench_frame, " x=", _player.position().x().integer(),
                                   " y=", _player.position().y().integer(), " mode=", mode_name(_player.mode()),
                                   " action=", int(_player.action()), " kills=", _kills,
                                   " zombies=", _zombies.count(), " grabbers=", _player.grabbers(),
                                   " weapon=", int(_player.current_weapon()), " searching=", int(_search.active()),
                                   " counted=", hordes::counted_near(_player.position()),
                                   " cpu=", (bn::core::last_cpu_usage() * 100).integer());
                        }
                    }
                #endif
            }
        }

        #if KX_DUMP
            if(++dump_frame % 20 == 0)
            {
                _hud.pixels().dump(dump_frame);
            }
        #endif

        // Music (milestone 14): the director looks at the night, sleep, death and the chase every few frames.
        if(++_music_frames >= music::music_tuning::update_frames)
        {
            music::situation now;
            now.night = is_night(minute_of_day());
            now.asleep = _sleep_state != 0;
            now.dead = _dead && _death_shown;
            now.chasers = _zombies.chasing();
            music::update(now, _music_frames);
            _music_frames = 0;

            #if KX_BENCH
                if(music::playing() != _bench_track)
                {
                    _bench_track = music::playing();
                    BN_LOG("KXMUSIC f=", bench_frame, " ", music::track_name(_bench_track), " chasers=", now.chasers,
                           " night=", now.night, " sounding=", music::sounding());
                }
            #endif
        }

        _update_menu_dim();
        bn::core::update();
        sfx::new_frame();
    }
}

void game::_update_context()
{
    // The container A would search: the tile in front of the feet (furniture), or the one under them
    // (corpses and dropped items).
    const bn::fixed_point& feet = _player.position();
    bn::fixed_point probe = feet + (direction_unit(_player.facing_x(), _player.facing_y()) *
                                    tuning(inventory_param::INTERACT_REACH).integer());

    // Riding or driving, aiming a gun (A fires) or swimming: nothing else to do with A.
    if(_player.ride() != ride_kind::NONE || _player.aiming() || _player.swimming())
    {
        _player.set_context(-1);
        _hud.set_prompt(-1, 0, "");
        return;
    }

    // A bicycle, car or wreck within reach comes first: A rides it or looks it over.
    _vehicle_context = _vehicles.reachable(feet, probe);

    if(_vehicle_context >= 0 && _player.action() == player_action::NONE && ! _player.fighting() &&
       ! zombie_near(tuning(inventory_param::PROMPT_SAFE_PX).integer()))
    {
        vehicle_kind kind = _vehicles.kind(_vehicle_context);
        bn::fixed_point centre = _vehicles.position(_vehicle_context);
        _player.set_context(player::vehicle_context);
        _hud.set_prompt(centre.x().integer() - _camera_controller.left(),
                        centre.y().integer() - 14 - _camera_controller.top(),
                        kind == vehicle_kind::BICYCLE ? "[A] RIDE" : kind == vehicle_kind::WRECK ? "[A] WRECK" :
                                                                                                    "[A] CAR");
        return;
    }

    // A door in front (milestone 12.7): A opens or closes it, even with zombies about (slamming it is the point),
    // but not mid-swing, and an open one not while anyone stands in its doorway.
    {
        int door_x = probe.x().floor_integer() >> world::tile_shift;
        int door_y = (probe.y().floor_integer() - 2) >> world::tile_shift;
        bool side_wall;
        world::door_state door = world::door_of(world::wall_gfx(door_x, door_y), side_wall);

        if((door == world::door_state::CLOSED ||
            (door == world::door_state::OPEN && ! _doorway_occupied(door_x, door_y))) &&
           _player.action() == player_action::NONE)
        {
            _door_x = door_x;
            _door_y = door_y;
            _player.set_context(player::door_context);
            const char* prompt = door == world::door_state::OPEN ? "[A] CLOSE" : "[A] OPEN";

            if(door == world::door_state::CLOSED && world::door_locked(door_x, door_y))
            {
                work_kind way = _lock_way(door_x, door_y);
                prompt = way == work_kind::UNLOCK ? "[A] UNLOCK" : way == work_kind::FORCE ? "[A] FORCE" :
                         way == work_kind::PICK ? "[A] PICK" : "[A] LOCKED";
            }

            // Forcing or picking: the bar over the head says it, so no prompt.
            _hud.set_prompt(_door_work != work_kind::NONE ? -1 :
                            (door_x * world::tile_size) + (world::tile_size / 2) - _camera_controller.left(),
                            (door_y * world::tile_size) - _camera_controller.top(), prompt);
            return;
        }
    }

    // A tree in front with an axe in hand (milestone 12.7): A chops it, when no zombie is close (else A swings).
    {
        int tree_x = probe.x().floor_integer() >> world::tile_shift;
        int tree_y = (probe.y().floor_integer() - 2) >> world::tile_shift;

        if(world::tree_at(tree_x, tree_y) && chops_trees(_player.items().equipped(equip_slot::HAND).id) &&
           _player.action() == player_action::NONE &&
           (_door_work == work_kind::CHOP || ! zombie_near(tuning(inventory_param::PROMPT_SAFE_PX).integer())))
        {
            _door_x = tree_x;
            _door_y = tree_y;
            _player.set_context(player::chop_context);
            _hud.set_prompt(_door_work != work_kind::NONE ? -1 :
                            (tree_x * world::tile_size) + (world::tile_size / 2) - _camera_controller.left(),
                            (tree_y * world::tile_size) - 8 - _camera_controller.top(), "[A] CHOP");
            return;
        }
    }

    // A deer carcass in front with a knife or an axe in hand (milestone 13): A held butchers it.
    {
        weapon held = info(_player.items().equipped(equip_slot::HAND).id).melee;
        int carcass = _door_work == work_kind::BUTCHER ? _butcher_id :
                      _animals.carcass_at(probe, animal_tuning::butcher_reach);

        if(carcass >= 0 && (held == weapon::KNIFE || held == weapon::AXE) &&
           _player.action() == player_action::NONE &&
           (_door_work == work_kind::BUTCHER || ! zombie_near(tuning(inventory_param::PROMPT_SAFE_PX).integer())))
        {
            _butcher_id = carcass;
            bn::fixed_point at = _animals.position_of(carcass);
            _player.set_context(player::butcher_context);
            _hud.set_prompt(_door_work != work_kind::NONE ? -1 : at.x().integer() - _camera_controller.left(),
                            at.y().integer() - 20 - _camera_controller.top(), "[A] BUTCHER");
            return;
        }
    }

    // A hen within reach: A catches it; a goat not milked today, with an empty bottle carried: A milks it.
    if(_animals.count() > 0 && _door_work == work_kind::NONE && _player.action() == player_action::NONE)
    {
        int hen = _animals.hen_at(probe, animal_tuning::catch_reach);
        int goat = hen < 0 && _player.items().find(item_id::EMPTY_BOTTLE) >= 0 ?
                   _animals.goat_to_milk(*this, probe, animal_tuning::milk_reach) : -1;

        if((hen >= 0 || goat >= 0) && ! zombie_near(tuning(inventory_param::PROMPT_SAFE_PX).integer()))
        {
            _farm_animal_id = hen >= 0 ? hen : goat;
            bn::fixed_point at = _animals.position_of(_farm_animal_id);
            _player.set_context(hen >= 0 ? player::catch_context : player::milk_context);
            _hud.set_prompt(at.x().integer() - _camera_controller.left(), at.y().integer() - 20 - _camera_controller.top(),
                            hen >= 0 ? "[A] CATCH" : "[A] MILK");
            return;
        }
    }

    int index = _containers.at(probe.x().floor_integer() >> world::tile_shift,
                               (probe.y().floor_integer() - 2) >> world::tile_shift);

    if(index < 0)
    {
        index = _containers.furniture(probe.x().floor_integer() >> world::tile_shift,
                                      (probe.y().floor_integer() - 2) >> world::tile_shift);
    }

    if(index < 0)
    {
        index = _containers.at(feet.x().floor_integer() >> world::tile_shift,
                               (feet.y().floor_integer() - 2) >> world::tile_shift);

        if(index >= 0 && _containers.record(index).kind == container_kind::FURNITURE)
        {
            index = -1;
        }
    }

    // Beds: A sleeps. Shallow water and rain collectors: A drinks.
    int probe_x = probe.x().floor_integer() >> world::tile_shift;
    int probe_y = (probe.y().floor_integer() - 2) >> world::tile_shift;
    bool bed = index < 0 && world::is_bed(probe_x, probe_y);
    bool water = index < 0 && ! bed && (base::rain_water(probe_x, probe_y) > 0 ||
                                        world::terrain_at(probe_x, probe_y) == terrain::SHALLOW_WATER ||
                                        world::wall_gfx(probe_x, probe_y) == world_gfx::WELL);

    // A farm plot in front, or else underfoot: plant, water, harvest or clear.
    const char* tend = nullptr;

    if(index < 0 && ! bed && ! water)
    {
        int feet_x = feet.x().floor_integer() >> world::tile_shift;
        int feet_y = (feet.y().floor_integer() - 2) >> world::tile_shift;
        int plot = farm::at(probe_x, probe_y);
        _tend_x = probe_x;
        _tend_y = probe_y;
        tend = _device_prompt(probe_x, probe_y);

        if(plot < 0 && ! tend)
        {
            plot = farm::at(feet_x, feet_y);
            _tend_x = feet_x;
            _tend_y = feet_y;
        }

        if(plot >= 0 && ! tend)
        {
            tend = _plot_prompt(plot);
        }
    }

    // Crouched on wild ground: A forages (unless this area was foraged today).
    bool forage = false;

    if(index < 0 && ! bed && ! water && ! tend && _player.crouched())
    {
        int feet_x = feet.x().floor_integer() >> world::tile_shift;
        int feet_y = (feet.y().floor_integer() - 2) >> world::tile_shift;
        terrain ground = world::terrain_at(feet_x, feet_y);

        if(ground == terrain::FOREST || ground == terrain::MUD || ground == terrain::GRASS || ground == terrain::CROPS)
        {
            forage = true;

            for(const foraged_area& area : _foraged)
            {
                if(area.x == (feet_x >> 3) && area.y == (feet_y >> 3) && area.day == day())
                {
                    forage = false;
                }
            }
        }
    }

    // A only searches, sleeps or drinks when no zombie is close and not right after a swing; otherwise it swings,
    // so the prompt hides too.
    if((index >= 0 || bed || water || forage || tend) &&
       (_player.action() != player_action::NONE || _player.fighting() ||
        zombie_near(tuning(inventory_param::PROMPT_SAFE_PX).integer())))
    {
        index = -1;
        bed = false;
        water = false;
        forage = false;
        tend = nullptr;
    }

    _player.set_context(bed ? player::bed_context : water ? player::drink_context :
                        tend ? player::farm_context : forage ? player::forage_context : index);

    if(tend)
    {
        int screen_x = (_tend_x * world::tile_size) + (world::tile_size / 2) - _camera_controller.left();
        int screen_y = (_tend_y * world::tile_size) - _camera_controller.top();
        _hud.set_prompt(screen_x, screen_y, tend);
        return;
    }

    if(forage)
    {
        int screen_x = feet.x().integer() - _camera_controller.left();
        int screen_y = feet.y().integer() - 20 - _camera_controller.top();
        _hud.set_prompt(screen_x, screen_y, "[A] FORAGE");
        return;
    }

    if(water)
    {
        _drink_tile_x = probe_x;
        _drink_tile_y = probe_y;
        int screen_x = (probe_x * world::tile_size) + (world::tile_size / 2) - _camera_controller.left();
        int screen_y = (probe_y * world::tile_size) - _camera_controller.top();
        bool fill = false;

        for(int slot = 0; slot < inventory::slots; ++slot)
        {
            const item_stack& can = _player.items().slot(slot);
            fill |= waters_plots(can.id) && can.amount < info(can.id).durability;
        }

        _hud.set_prompt(screen_x, screen_y, fill ? "[A] DRINK, FILL CAN" : "[A] DRINK");
        return;
    }

    if(bed)
    {
        int screen_x = (probe_x * world::tile_size) + (world::tile_size / 2) - _camera_controller.left();
        int screen_y = (probe_y * world::tile_size) - _camera_controller.top();
        _hud.set_prompt(screen_x, screen_y, "[A] SLEEP");
        return;
    }

    if(index < 0)
    {
        _hud.set_prompt(-1, 0, "");
        return;
    }

    const container_record& value = _containers.record(index);
    int screen_x = (value.tile_x * world::tile_size) + (world::tile_size / 2) - _camera_controller.left();
    int screen_y = (value.tile_y * world::tile_size) - _camera_controller.top();
    _hud.set_prompt(screen_x, screen_y,
                    value.kind == container_kind::FLOOR ? "[A] PICK UP" :
                    value.table == loot_table::GUN_SAFE && ! value.opened ? "[A] OPEN SAFE" : "[A] SEARCH");
}

void game::_update_play()
{
    // Night: the darkness is worked out first, so the world's sprites (shaded as they update below) and the world
    // renderer (further down) change in the same frame.
    int darkness = light::darkness(minute_of_day(), _weather == weather_kind::FOG,
                                   _player.skills().dusk_shift_minutes());
    light::set_sprites_dark(darkness > 0);

    // Rendering-side updates run every frame, also during hitstop.
    BN_PROFILER_START("visuals");
    KX_BENCH_BEGIN(bench::VISUALS);
    _player.update_visuals(_camera_controller.top());
    _zombies.update_visuals(_camera_controller.left(), _camera_controller.top());
    _animals.update_visuals(_camera_controller.left(), _camera_controller.top());
    _vehicles.update_visuals(*this, _camera_controller.left(), _camera_controller.top());

    char drive_lines[2][16];
    bool driving = _vehicles.status(drive_lines[0], drive_lines[1]);
    _hud.set_drive(driving ? drive_lines[0] : nullptr, driving ? drive_lines[1] : nullptr);
    _update_flames();
    _weather_effects.update(*this, _weather, _inside);
    _events.update(*this);
    KX_BENCH_END();
    BN_PROFILER_STOP();

    BN_PROFILER_START("camera");
    KX_BENCH_BEGIN(bench::CAMERA);
    _camera_controller.update(_player);
    KX_BENCH_END();
    BN_PROFILER_STOP();

    BN_PROFILER_START("world_gen");
    KX_BENCH_BEGIN(bench::WORLD_GEN);
    world::update(_player.position().x().integer(), _player.position().y().integer(), _camera_controller.left(),
                  _camera_controller.top());
    KX_BENCH_END();
    BN_PROFILER_STOP();

    // The roof of the building the player is in fades out (looked up again when the feet change tile).
    int feet_tile_x = _player.position().x().floor_integer() >> world::tile_shift;
    int feet_tile_y = (_player.position().y().floor_integer() - 2) >> world::tile_shift;

    if(feet_tile_x != _roof_tile_x || feet_tile_y != _roof_tile_y)
    {
        _roof_tile_x = feet_tile_x;
        _roof_tile_y = feet_tile_y;
        bool was_inside = _inside;
        _inside = world::building_over(feet_tile_x, feet_tile_y, _building_left, _building_top, _building_width,
                                       _building_height);

        // Milestone 14: a smashed window's glass crunches underfoot (quietly when sneaking).
        world_gfx under = world::wall_gfx(feet_tile_x, feet_tile_y);

        if(under == world_gfx::BROKEN_FACE || under == world_gfx::BROKEN_TOP)
        {
            sfx::play(sfx::id::GLASS_STEP, _player.crouched() ? bn::fixed(1) / 3 : bn::fixed(3) / 4);
        }

        #if KX_BENCH == 7
            if(_inside != was_inside)
            {
                BN_LOG("KXROOF inside=", _inside, " building=", _building_left, ",", _building_top, " ",
                       _building_width, "x", _building_height);
            }
        #else
            static_cast<void>(was_inside);
        #endif
    }

    world_map::explore(_player.position().x().integer() >> world::tile_shift,
                       _player.position().y().integer() >> world::tile_shift);
    _mark_arrow.update(*this);

    // Aiming a gun: the cone's edges, the target's brackets, and the last shot's streaks.
    aim_view aim;
    aim.aiming = _player.aiming() && ! _dead;
    aim.muzzle = _player.aim_muzzle();
    aim.angle = _player.aim_angle();
    aim.cone = _player.aim_cone();
    aim.target = aim.aiming && _player.aim_locked();
    aim.target_feet = _player.aim_target_feet();

    if(aim.aiming && ! is_gun(_player.items().equipped(equip_slot::HAND).id))
    {
        // A throw: one line along it and a small box round where it lands.
        constexpr int landing_box_half = 6;
        constexpr int landing_box_height = 11;
        aim.single_line = true;
        aim.box_half_width = landing_box_half;
        aim.box_height = landing_box_height;
        aim.target_feet = aim.target_feet + bn::fixed_point(0, 4);
    }

    _gun_fx.update(aim);

    // Over the player's head while forcing or picking a lock: its bar (milestone 12.7).
    {
        constexpr int head_height = 24;
        _hud.set_work_bar(screen_x(_player.position()) + (bn::display::width() / 2),
                          screen_y(_player.position()) + (bn::display::height() / 2) - head_height,
                          _door_work_frame, _door_work != work_kind::NONE ? _door_work_length : 0);
    }

    // Over the player's head while aiming: the reload bar, or the JAMMED box.
    if(aim.aiming && (_player.reloading() || _player.gun_jammed_now()))
    {
        constexpr int head_height = 24;
        _hud.set_gun_box(screen_x(_player.position()) + (bn::display::width() / 2),
                         screen_y(_player.position()) + (bn::display::height() / 2) - head_height);
    }
    else
    {
        _hud.set_gun_box(-1, 0);
    }

    world_effects effects;
    effects.inside = _inside;
    effects.left = _building_left * world::tile_size;
    effects.top = _building_top * world::tile_size;
    effects.width = _building_width * world::tile_size;
    effects.height = _building_height * world::tile_size;
    effects.haze = _weather == weather_kind::FOG ? bn::fixed(weather_tuning::haze_percent) / 100 : bn::fixed(0);
    effects.darkness = darkness;
    effects.flash = _weather_effects.flashing();
    effects.lit_inside = darkness > 0 && _inside && _building_lit();
    _renderer.update_effects(effects);
    _update_lights(darkness);
    _renderer.set_season(weather::autumn_percent(day()), weather::winter_percent(day()));

    BN_PROFILER_START("bg_stream");
    KX_BENCH_BEGIN(bench::BG_STREAM);
    _renderer.update(_camera_controller.left(), _camera_controller.top());
    KX_BENCH_END();
    BN_PROFILER_STOP();

    BN_PROFILER_START("hud");
    KX_BENCH_BEGIN(bench::HUD);
    _hud.update(_player, _world_minute);
    KX_BENCH_END();
    BN_PROFILER_STOP();
}

bool game::_update_death()
{
    if(_hitstop > 0)
    {
        --_hitstop;
    }

    #if KX_BENCH == 8
        // Save bench: no one to press A; the next survivor starts once the death screen shows.
        if(_death_shown)
        {
            return true;
        }
    #endif

    // Let the fall play out before the death screen.
    if(! _death_shown)
    {
        input_frame none;
        _player.update(none, *this);
        _zombies.update(*this);
        _fx.update();
        _update_play();

        if(--_death_timer <= 0)
        {
            _hud.death_screen(death_text(_player.body().cause_of_death()), _kills, _world_minute - _start_minute,
                              _player.distance_tiles());

            #if KX_BENCH
                BN_LOG("KXSURV death=", death_text(_player.body().cause_of_death()), " day=", day(),
                       " h=", (_world_minute / 60) % 24);
            #endif
            _death_shown = true;
        }

        return false;
    }

    return _input.read().a_pressed;
}

int game::screen_x(const bn::fixed_point& position) const
{
    return position.x().integer() - _camera_controller.left() - (bn::display::width() / 2);
}

int game::screen_y(const bn::fixed_point& position) const
{
    return position.y().integer() - _camera_controller.top() - (bn::display::height() / 2);
}

void game::request_hitstop(int frames)
{
    _hitstop = bn::max(_hitstop, frames);
}

void game::shake(int pixels, int frames)
{
    _camera_controller.shake(pixels, frames);
}

void game::add_decal(const bn::fixed_point& position, world_decal decal)
{
    _add_decal(position.x().floor_integer() >> world::tile_shift,
               (position.y().floor_integer() - 1) >> world::tile_shift, decal);
}

void game::_add_decal(int tile_x, int tile_y, world_decal decal)
{
    int current = int(world::ground_gfx(tile_x, tile_y));
    int base = world_gfx_base[current];

    // A corpse is never covered by blood.
    if(decal != world_decal::CORPSE && current != base && world_gfx_decals[base][int(world_decal::CORPSE)] == current)
    {
        return;
    }

    int replacement = world_gfx_decals[base][int(decal)];

    if(replacement)
    {
        world::set_ground_gfx(tile_x, tile_y, world_gfx(replacement));
        _renderer.refresh_tile(tile_x, tile_y);
    }
}

void game::noise(const bn::fixed_point& position, int radius_tiles)
{
    // Spec: rain halves all noise radii.
    if(weather::wet(_weather))
    {
        radius_tiles = (radius_tiles * weather_tuning::noise_percent) / 100;
    }

    if(radius_tiles > 0)
    {
        _zombies.hear(position, radius_tiles * world::tile_size);
        hordes::noise(position, radius_tiles, _world_minute / 60);
        _animals.hear(*this, position, radius_tiles);
    }
}

void game::toast(const char* text, int bank, int frames)
{
    _hud.toast(text, bank, frames);
}

void game::player_bitten()
{
    if(_dead)
    {
        return;
    }

    _dead = true;
    _death_timer = death_screen_delay;
    _car.close(*this);
    static_cast<void>(_vehicles.leave(*this, false));

    // Permadeath: the survivor rises as a zombie with their gear, and the save loses them at once, so the death
    // can't be undone by switching off.
    if(saving)
    {
        survivors::bury(*this);
        _save(true);
    }
}

bool game::add_corpse(const bn::fixed_point& position, loot_table table, int body)
{
    int tile_x = position.x().floor_integer() >> world::tile_shift;
    int tile_y = (position.y().floor_integer() - 1) >> world::tile_shift;

    if(_containers.at(tile_x, tile_y) < 0)
    {
        int index = _containers.add(container_kind::CORPSE, table, tile_x, tile_y);

        if(index >= 0)
        {
            _containers.set_body(index, body);
            return true;
        }
    }

    return false;
}

bool game::drop_item(const item_stack& stack)
{
    const bn::fixed_point& feet = _player.position();
    return drop_item_at(stack, feet.x().floor_integer() >> world::tile_shift,
                        (feet.y().floor_integer() - 2) >> world::tile_shift);
}

bool game::drop_item_at(const item_stack& stack, int tile_x, int tile_y)
{
    // The save's stack budget (milestone 14): a world this full takes nothing more.
    if(_containers.room() <= 0)
    {
        toast("WORLD STORAGE FULL", hud_tiles::bank_danger, 90);
        return false;
    }

    int index = _containers.at(tile_x, tile_y);

    if(index >= 0 && _containers.record(index).kind == container_kind::FURNITURE)
    {
        return false;
    }

    if(index < 0)
    {
        index = _containers.add(container_kind::FLOOR, loot_table::FLOOR, tile_x, tile_y);

        if(index < 0)
        {
            return false;
        }
    }

    container& pile = _containers.get(index);
    _containers.open(index, day());

    // Needs a free slot, so the whole stack always fits (a partial drop would split the item).
    bool has_room = false;

    for(const item_stack& existing : pile.items)
    {
        has_room |= existing.empty();
    }

    if(! has_room)
    {
        remove_empty_pile(index);
        return false;
    }

    item_stack moving = stack;
    pile.add(moving);

    // Floor piles show as a sack on the ground (corpses already show).
    if(pile.kind == container_kind::FLOOR)
    {
        _add_decal(tile_x, tile_y, world_decal::ITEMS);
    }

    return true;
}

void game::remove_empty_pile(int index)
{
    if(index < 0 || index >= _containers.count())
    {
        return;
    }

    const container& pile = _containers.get(index);

    if(pile.kind != container_kind::FLOOR || ! pile.empty())
    {
        return;
    }

    int tile_x = pile.tile_x;
    int tile_y = pile.tile_y;
    int current = int(world::ground_gfx(tile_x, tile_y));
    int base = world_gfx_base[current];

    if(current != base && world_gfx_decals[base][int(world_decal::ITEMS)] == current)
    {
        world::set_ground_gfx(tile_x, tile_y, world_gfx(base));
        _renderer.refresh_tile(tile_x, tile_y);
    }

    _containers.remove_if_empty_floor(index);
}

bool game::zombie_near(int distance) const
{
    return _zombies.standing_within(_player.position(), distance);
}

namespace
{
    // Sleep states.
    constexpr int sleep_none = 0;
    constexpr int sleep_choosing = 1;
    constexpr int sleep_fading_out = 2;
    constexpr int sleep_sleeping = 3;
    constexpr int sleep_fading_in = 4;
    constexpr int sleep_fade_frames = 30;
    constexpr int sleep_minutes_per_frame = 20;
    constexpr int sleep_max_hours = 12;
    constexpr int sleep_box_top = 7;
    constexpr int zombie_sleep_distance = 160;     // aware or not, nobody sleeps with zombies this close
}

void game::begin_sleep()
{
    const survival& body = _player.body();

    if(body.level(moodle::TIRED) == 0)
    {
        toast("NOT TIRED", hud_tiles::bank_quiet, 60);
        sfx::play(sfx::id::DENIED, milli(600));
        return;
    }

    if(_zombies.standing_within(_player.position(), zombie_sleep_distance))
    {
        toast("ZOMBIES NEARBY", hud_tiles::bank_danger, 60);
        sfx::play(sfx::id::DENIED, milli(600));
        return;
    }

    _sleep_state = sleep_choosing;
    _sleep_hours = 8;
    _hud.set_prompt(-1, 0, "");
}

bool game::_reading_light()
{
    // Daylight, a flashlight held and switched on, a campfire close by, lights a generator powers, a lit building, or a
    // car's headlights: the car being driven, or lamps close by (as close as a campfire must be).
    int x = _player.position().x().integer() >> world::tile_shift;
    int y = (_player.position().y().integer() - 1) >> world::tile_shift;
    const item_stack* torch = _held_flashlight();

    if(! is_night(minute_of_day()) || (torch && light::flashlight_on(*torch)) ||
       base::fire_near(x, y, base_tuning::light_tiles, _world_minute / 60) ||
       power::powered(x, y, _world_minute) || (_inside && _building_lit()) || _vehicles.driven_lit())
    {
        return true;
    }

    bn::fixed_point lamps;
    int angle;

    if(_vehicles.headlights(lamps, angle))
    {
        int reach = base_tuning::light_tiles * world::tile_size;
        bn::fixed_point offset = lamps - _player.position();
        return bn::abs(offset.x()) <= reach && bn::abs(offset.y()) <= reach;
    }

    return false;
}

bool game::begin_reading(skill taught, int volume)
{
    const char* problem = nullptr;
    bool lit = _reading_light();

    if(_player.skills().read(taught, volume))
    {
        problem = "ALREADY READ";
    }
    else if(! lit)
    {
        problem = "TOO DARK TO READ";
    }
    else if(_zombies.standing_within(_player.position(), zombie_sleep_distance))
    {
        problem = "ZOMBIES NEARBY";
    }

    if(problem)
    {
        toast(problem, hud_tiles::bank_danger, 60);
        sfx::play(sfx::id::DENIED, milli(600));
        return false;
    }

    close_menus();
    _search.close(*this);
    _start_sleeping(0, false);
    _sleep_minutes_left = _player.body().action_time(volume * character_tuning::reading_minutes_per_volume);
    _activity = activity::READ_BOOK;
    _activity_value = int8_t(taught);
    _reading_volume = int8_t(volume);
    sfx::play(sfx::id::RUMMAGE, milli(500));
    return true;
}

bool game::begin_reading_magazine(magazine value)
{
    const char* problem = nullptr;
    bool lit = _reading_light();

    if(_player.skills().knows(value))
    {
        problem = "ALREADY READ";
    }
    else if(! lit)
    {
        problem = "TOO DARK TO READ";
    }
    else if(_zombies.standing_within(_player.position(), zombie_sleep_distance))
    {
        problem = "ZOMBIES NEARBY";
    }

    if(problem)
    {
        toast(problem, hud_tiles::bank_danger, 60);
        sfx::play(sfx::id::DENIED, milli(600));
        return false;
    }

    close_menus();
    _search.close(*this);
    _start_sleeping(0, false);
    _sleep_minutes_left = _player.body().action_time(cooking_tuning::magazine_minutes);
    _activity = activity::READ_MAGAZINE;
    _activity_value = int8_t(value);
    sfx::play(sfx::id::RUMMAGE, milli(500));
    return true;
}

heat_kind game::heat_source()
{
    int px = _player.position().x().integer() >> world::tile_shift;
    int py = (_player.position().y().integer() - 1) >> world::tile_shift;

    if(base::fire_near(px, py, cooking_tuning::heat_tiles, _world_minute / 60))
    {
        return heat_kind::CAMPFIRE;
    }

    // A kitchen stove works while the power is on, or near a running generator.
    if(timeline::power_on(day()) || power::powered(px, py, _world_minute))
    {
        for(int y = py - cooking_tuning::heat_tiles; y <= py + cooking_tuning::heat_tiles; ++y)
        {
            for(int x = px - cooking_tuning::heat_tiles; x <= px + cooking_tuning::heat_tiles; ++x)
            {
                if(fixtures::at(x, y) == fixtures::fixture::STOVE)
                {
                    return heat_kind::STOVE;
                }
            }
        }
    }

    // A gas stove carried, with propane left.
    const inventory& items = _player.items();

    if(items.find(item_id::GAS_STOVE) >= 0)
    {
        for(int slot = 0; slot < inventory::slots; ++slot)
        {
            if(items.slot(slot).id == item_id::PROPANE && items.slot(slot).amount > 0)
            {
                return heat_kind::GAS_STOVE;
            }
        }
    }

    return heat_kind::NONE;
}

int game::water_source()
{
    int px = _player.position().x().integer() >> world::tile_shift;
    int py = (_player.position().y().integer() - 1) >> world::tile_shift;
    int found = 0;

    for(int y = py - cooking_tuning::water_tiles; y <= py + cooking_tuning::water_tiles && found != 1; ++y)
    {
        for(int x = px - cooking_tuning::water_tiles; x <= px + cooking_tuning::water_tiles && found != 1; ++x)
        {
            if(world::terrain_at(x, y) == terrain::SHALLOW_WATER || world::wall_gfx(x, y) == world_gfx::WELL ||
               (timeline::water_on(day()) && fixtures::sink(x, y)))
            {
                found = 1;
            }
            else if(base::rain_water(x, y) > 0)
            {
                found = 2;
            }
        }
    }

    return found;
}

int game::carries(item_id id) const
{
    const inventory& items = _player.items();

    if(id == any_produce || id == fuel_litres)
    {
        int result = 0;

        for(int slot = 0; slot < inventory::slots; ++slot)
        {
            const item_stack& stack = items.slot(slot);

            if(id == fuel_litres ? stack.id == item_id::FUEL_CAN : produce(stack.id))
            {
                result += id == fuel_litres ? stack.amount : stack.quantity();
            }
        }

        return result;
    }

    if(info(id).flags & item_flag::tool || info(id).category == item_category::TOOL ||
       info(id).category == item_category::WEAPON)
    {
        return items.find(id) >= 0 || items.equipped(equip_slot::HAND).id == id;
    }

    return items.count(id);
}

bool game::recipe_ready(int index, const char*& problem)
{
    const recipe_info& recipe = recipes[index];
    heat_kind heat = heat_source();
    problem = nullptr;

    if(! _player.skills().knows(recipe.needs))
    {
        problem = "READ THE MAGAZINE FIRST";
    }
    else if(! recipe.no_heat && (heat == heat_kind::NONE || (recipe.fire_only && heat != heat_kind::CAMPFIRE)))
    {
        problem = recipe.fire_only ? "NEEDS AN OPEN FIRE" : "NO HEAT NEARBY";
    }
    else if(recipe.tool != item_id::NONE && carries(recipe.tool) <= 0 &&
            ! (kettle_does(recipe) && carries(item_id::KETTLE) > 0))
    {
        problem = recipe.tool == item_id::COOKING_POT ? "NEEDS A COOKING POT" :
                  recipe.tool == item_id::FRYING_PAN ? "NEEDS A FRYING PAN" :
                  recipe.tool == item_id::PLIERS ? "NEEDS PLIERS" : "NEEDS A HAMMER";
    }
    else if(recipe.water && water_source() <= 0)
    {
        problem = "NO WATER NEARBY";
    }
    else
    {
        for(int which = 0; which < recipe.ingredient_count; ++which)
        {
            if(carries(recipe.ingredients[which].item) < recipe.ingredients[which].amount)
            {
                problem = "MISSING INGREDIENTS";
            }
        }
    }

    return ! problem;
}

int game::cook_minutes(int index) const
{
    const survival& body = _player.body();

    if(recipes[index].no_heat)
    {
        return body.action_time(recipes[index].minutes);
    }

    int level = _player.skills().level(skill::COOKING);
    return body.action_time(
        bn::max((recipes[index].minutes * (100 - (level * cooking_tuning::faster_percent_per_level))) / 100, 5));
}

void game::cook(int index)
{
    const char* problem;

    if(_player.swimming())
    {
        toast("NOT WHILE SWIMMING", hud_tiles::bank_danger, 60);
        sfx::play(sfx::id::DENIED, milli(600));
        return;
    }

    if(! recipe_ready(index, problem))
    {
        toast(problem, hud_tiles::bank_danger, 60);
        sfx::play(sfx::id::DENIED, milli(600));
        return;
    }

    if(_zombies.standing_within(_player.position(), zombie_sleep_distance))
    {
        toast("ZOMBIES NEARBY", hud_tiles::bank_danger, 60);
        sfx::play(sfx::id::DENIED, milli(600));
        return;
    }

    const recipe_info& recipe = recipes[index];
    inventory& items = _player.items();

    // The ingredients go in (produce takes whatever is carried first); water comes from a rain collector if that is
    // the source; a gas stove burns a little propane.
    for(int which = 0; which < recipe.ingredient_count; ++which)
    {
        const ingredient& need = recipe.ingredients[which];

        if(need.item == any_produce)
        {
            int left = need.amount;

            for(int slot = 0; slot < inventory::slots && left > 0; ++slot)
            {
                if(produce(items.slot(slot).id))
                {
                    items.consume_one(slot);
                    --left;
                    --slot;
                }
            }
        }
        else if(need.item == fuel_litres)
        {
            // Poured from the cans carried, a litre at a time; an emptied can stays in the bag.
            int left = need.amount;

            for(int slot = 0; slot < inventory::slots && left > 0; ++slot)
            {
                item_stack& can = items.slot(slot);

                while(can.id == item_id::FUEL_CAN && can.amount > 0 && left > 0)
                {
                    --can.amount;
                    --left;
                }
            }
        }
        else if(items.count(need.item) < need.amount && items.equipped(equip_slot::HAND).id == need.item)
        {
            // A tool used up (the spear's kitchen knife) may be the one in hand.
            items.equipped(equip_slot::HAND) = item_stack();
        }
        else
        {
            items.remove(need.item, need.amount);
        }
    }

    if(recipe.water && water_source() == 2)
    {
        int px = _player.position().x().integer() >> world::tile_shift;
        int py = (_player.position().y().integer() - 1) >> world::tile_shift;
        bool taken = false;

        for(int y = py - cooking_tuning::water_tiles; y <= py + cooking_tuning::water_tiles && ! taken; ++y)
        {
            for(int x = px - cooking_tuning::water_tiles; x <= px + cooking_tuning::water_tiles && ! taken; ++x)
            {
                taken = base::take_rain_water(x, y);
            }
        }
    }

    if(! recipe.no_heat && heat_source() == heat_kind::GAS_STOVE)
    {
        for(int slot = 0; slot < inventory::slots; ++slot)
        {
            item_stack& tank = items.slot(slot);

            if(tank.id == item_id::PROPANE && tank.amount > 0)
            {
                if(--tank.amount == 0)
                {
                    tank = item_stack();
                }

                break;
            }
        }
    }

    close_menus();
    _search.close(*this);
    _start_sleeping(0, false);
    _sleep_minutes_left = cook_minutes(index);
    _activity = activity::COOK;
    _activity_value = int8_t(index);
    sfx::play(sfx::id::RUMMAGE, milli(500));
}

void game::use_vehicle()
{
    if(_vehicle_context < 0 || _search.active() || _sleep_state || _dead)
    {
        return;
    }

    _hud.set_prompt(-1, 0, "");

    if(_vehicles.kind(_vehicle_context) == vehicle_kind::BICYCLE)
    {
        static_cast<void>(_vehicles.mount(*this, _vehicle_context));
        return;
    }

    _vehicles.opened(*this, _vehicle_context);
    _car.open(*this, _vehicle_context);
}

bool game::begin_car_work(int minutes)
{
    if(_zombies.standing_within(_player.position(), zombie_sleep_distance))
    {
        toast("ZOMBIES NEARBY", hud_tiles::bank_danger, 60);
        sfx::play(sfx::id::DENIED, milli(600));
        return false;
    }

    close_menus();
    _car.close(*this);
    _search.close(*this);
    _start_sleeping(0, false);
    _sleep_minutes_left = minutes;
    _activity = activity::MECHANIC;
    sfx::play(sfx::id::HAMMER, milli(500));
    return true;
}

void game::spawn_vehicle(int which)
{
    static_cast<void>(_vehicles.spawn(*this, which ? vehicle_kind::BICYCLE : vehicle_kind::CAR));
}

void game::_start_sleeping(int hours, bool forced)
{
    _activity = activity::SLEEP;
    _reading_dark = false;
    _sleep_state = sleep_fading_out;
    _sleep_minutes_left = hours * 60;
    _sleep_fade = 0;
    _forced_sleep = forced;
    _hud.begin_screen();
    _hud.end_screen();

    // Sleeping, cooking and reading skip minutes or hours: fires burn out and clocks stop ringing meanwhile.
    _throws.settle(*this);
}

void game::_update_sleep(const input_frame& input)
{
    switch(_sleep_state)
    {

    case sleep_choosing:
    {
        if(input.b_pressed || input.start_pressed)
        {
            _sleep_state = sleep_none;
            _hud.end_screen();
            return;
        }

        if(input.left_pressed || input.down_pressed)
        {
            _sleep_hours = bn::max(_sleep_hours - 1, 1);
        }
        else if(input.right_pressed || input.up_pressed)
        {
            _sleep_hours = bn::min(_sleep_hours + 1, sleep_max_hours);
        }

        if(input.a_pressed)
        {
            _start_sleeping(_sleep_hours, false);
            return;
        }

        // Small sleep box over the world: "SLEEP < 8 H >".
        char text[24];
        int length = ui::append(text, 0, "SLEEP < ");
        length = ui::append_int(text, length, _sleep_hours);
        ui::append(text, length, " H >");
        _hud.box(7, sleep_box_top, 22, sleep_box_top + 3, hud_tiles::bank_accent);
        _hud.text(8, sleep_box_top + 1, text, 14, hud_tiles::bank_accent);
        _hud.text(8, sleep_box_top + 2, "A SLEEP  B NO", 14, hud_tiles::bank_quiet);
        break;
    }

    case sleep_fading_out:
    case sleep_fading_in:
    {
        bool out = _sleep_state == sleep_fading_out;
        ++_sleep_fade;
        // Falling asleep where you stand (Tired 4) dims the screen slowly, over 3 s.
        int fade_frames = out && _forced_sleep ? tired_tuning::forced_dim_frames : sleep_fade_frames;
        bn::fixed intensity = bn::fixed(out ? _sleep_fade : fade_frames - _sleep_fade) / fade_frames;
        bn::bg_palettes::set_fade(bn::color(0, 0, 0), intensity);
        bn::sprite_palettes::set_fade(bn::color(0, 0, 0), intensity);

        if(_sleep_fade >= fade_frames)
        {
            _sleep_fade = 0;

            if(out)
            {
                _sleep_state = sleep_sleeping;
            }
            else
            {
                _sleep_state = sleep_none;
                bn::bg_palettes::set_fade_intensity(0);
                bn::sprite_palettes::set_fade_intensity(0);
                if(_reading_dark)
                {
                    _reading_dark = false;
                    toast("TOO DARK TO READ ON", hud_tiles::bank_danger, 120);

                    #if KX_BENCH == 10
                        BN_LOG("KXNIGHT reading stopped: too dark at ", minute_of_day() / 60, ":",
                               minute_of_day() % 60, " read=", _player.skills().read(skill::CARPENTRY, 1));
                    #endif
                }
                else if(_activity == activity::READ_BOOK)
                {
                    // Done reading: the book counts from now on.
                    skill taught = skill(_activity_value);
                    _player.skills().mark_read(taught, _reading_volume);

                    char text[32];
                    int length = ui::append(text, 0, "READ ");
                    length = ui::append(text, length, info(taught).name);
                    length = ui::append(text, length, " VOL ");
                    ui::append_int(text, length, _reading_volume);
                    toast(text, hud_tiles::bank_accent, 120);
                }
                else if(_activity == activity::READ_MAGAZINE)
                {
                    _player.skills().learn(magazine(_activity_value));
                    toast(magazine(_activity_value) == magazine::CANNING ? "YOU CAN CAN FOOD NOW" :
                          magazine(_activity_value) == magazine::TRAPPING ? "YOU CAN MAKE TRAPS NOW" :
                                                                           "YOU CAN RUN GENERATORS NOW",
                          hud_tiles::bank_accent, 120);
                }
                else if(_activity == activity::MECHANIC)
                {
                    // The work on the car is done; its screen comes back.
                    int vehicle = _vehicles.finish_work(*this);

                    if(vehicle >= 0 && ! _dead)
                    {
                        _car.open(*this, vehicle);
                    }
                }
                else if(_activity == activity::COOK)
                {
                    // The dish is ready: into the bag, or at your feet if it is full.
                    const recipe_info& recipe = recipes[_activity_value];

                    for(int made = 0; made < recipe.result_count; ++made)
                    {
                        item_stack dish = make_stack(recipe.result);

                        if(info(recipe.result).category == item_category::FOOD)
                        {
                            dish.extra = uint16_t(day());
                        }

                        if(! _player.items().add(dish))
                        {
                            static_cast<void>(drop_item(dish));
                        }
                    }

                    if(recipe.xp > 0)
                    {
                        _player.train(*this, skill::COOKING, recipe.xp);
                    }

                    char text[32];
                    int length = ui::append(text, 0, recipe.no_heat ? "MADE " : "COOKED ");
                    ui::append(text, length, recipe.name);
                    toast(text, hud_tiles::bank_accent, 120);
                }
                else
                {
                    toast(_forced_sleep ? "YOU PASSED OUT" : "YOU SLEPT", hud_tiles::bank_normal, 90);
                }

                _activity = activity::SLEEP;
            }
        }
        break;
    }

    default:
        // Asleep: time runs fast (the world is frozen; hordes and night attacks come in milestone 6).
        for(int minute = 0; minute < sleep_minutes_per_frame && _sleep_minutes_left > 0; ++minute)
        {
            --_sleep_minutes_left;
            _tick_minute(_activity == activity::SLEEP);

            // Reading stops when the light goes (the flashlight's battery dies, the campfire burns out, night
            // falls): the book or magazine has to be read again.
            if((_activity == activity::READ_BOOK || _activity == activity::READ_MAGAZINE) && ! _reading_light())
            {
                _sleep_minutes_left = 0;
                _reading_dark = true;
            }

            if(_dead)
            {
                bn::bg_palettes::set_fade_intensity(0);
                bn::sprite_palettes::set_fade_intensity(0);
                _sleep_state = sleep_none;
                return;
            }
        }

        if(_sleep_minutes_left <= 0)
        {
            if(_activity == activity::SLEEP)
            {
                _player.body().woke_up();
            }

            _sleep_state = sleep_fading_in;
            _save(true);
        }
        break;
    }

    _update_play();
}

void game::_tick_minute(bool sleeping)
{
    #if KX_BENCH == 6
        // Log the body every in-game hour, and bite the player after the sleep (zombie infection).
        if(_world_minute % 60 == 0)
        {
            const survival& state = _player.body();
            char moodles[int(moodle::COUNT) + 1];

            for(int index = 0; index < int(moodle::COUNT); ++index)
            {
                moodles[index] = char('0' + state.level(moodle(index)));
            }

            moodles[int(moodle::COUNT)] = 0;
            const body_wound& arm = state.wound(body_part::LEFT_ARM);
            BN_LOG("KXSURV day=", day(), " h=", (_world_minute / 60) % 24, " food=", state.food_percent(),
                   " water=", state.water_percent(), " fatigue=", state.fatigue_percent(), " endur=",
                   state.endurance_percent(), " blood=", state.blood(), " temp=", state.body_temperature(),
                   " health=", state.health(), " moodles=", moodles, " arm=", int(arm.type), "/", int(arm.bandage),
                   " infected=", int(state.zombie_infected()), " sleep=", _sleep_state);
        }

        if(_world_minute == (24 + 9) * 60)
        {
            _player.body().add_wound(body_part::RIGHT_ARM, wound_type::BITE, true, _random);
            BN_LOG("KXSURV bitten");
        }
    #endif

    survival& body = _player.body();
    const inventory& items = _player.items();

    survival_input input;
    input.exertion = sleeping ? 0 : _player.take_exertion();
    input.zombies_in_view = sleeping ? 0 : _zombies.aware_on_screen(_camera_controller.left(),
                                                                     _camera_controller.top());
    input.load_percent = (items.weight() * 100) / bn::max(items.capacity(), 1);
    input.minute_of_day = _world_minute % (24 * 60);
    input.wading = ! sleeping && _player.ground() == terrain::SHALLOW_WATER;
    input.sleeping = sleeping;

    // Weather and season: rain soaks you outdoors (a raincoat keeps most of it off), the season cools the air.
    _weather = weather::at(_world_minute);
    input.season_temp = weather::season_temp(day());

    if(weather::wet(_weather) && ! _inside && ! sleeping)
    {
        bool raincoat = items.equipped(equip_slot::JACKET).id == item_id::RAINCOAT;
        input.rain_wetness = raincoat ? (weather_tuning::rain_wetness * weather_tuning::raincoat_percent) / 100 + 1 :
                                        weather_tuning::rain_wetness;
    }

    if(weather::wet(_weather) && _world_minute % 60 == 0)
    {
        base::rain(weather_tuning::collector_fill);
    }

    // A campfire close by warms and dries.
    if(base::fire_near(_player.position().x().integer() >> world::tile_shift,
                       (_player.position().y().integer() - 1) >> world::tile_shift, base_tuning::warmth_tiles,
                       _world_minute / 60))
    {
        input.insulation += base_tuning::warmth_insulation;
        input.drying_percent = base_tuning::drying_percent;
    }

    const character& skills = _player.skills();
    input.hunger_percent = skills.hunger_percent();
    input.heal_percent = skills.heal_percent();
    input.panic_percent = skills.panic_percent();
    input.endurance_percent = skills.endurance_percent();
    input.bandage_percent = skills.bandage_percent();
    input.weather_percent = skills.weather_percent();

    // Skills levelled by doing, over the minute: Fitness from exertion, Strength from a heavy load, Lightfoot
    // from walking and jogging, Sneaking from crouch-walking near zombies that have not noticed you.
    if(! sleeping && ! _dead)
    {
        int walked = _player.distance_tiles() - _minute_distance;
        _player.train(*this, skill::FITNESS, input.exertion / character_tuning::fitness_xp_per);

        if(input.load_percent >= character_tuning::heavy_load_percent && walked > 0)
        {
            _player.train(*this, skill::STRENGTH, character_tuning::strength_heavy_xp);
        }

        if(walked > 0 && _player.mode() != move_mode::SPRINT && _player.ride() == ride_kind::NONE)
        {
            _player.train(*this, skill::LIGHTFOOT, character_tuning::lightfoot_xp);
        }

        if(walked > 0 && _player.crouched() &&
           _zombies.unaware_on_screen(_camera_controller.left(), _camera_controller.top()) > 0)
        {
            _player.train(*this, skill::SNEAKING, character_tuning::sneak_xp);
        }
    }

    _minute_distance = _player.distance_tiles();

    for(int slot = int(equip_slot::HEAD); slot < int(equip_slot::COUNT); ++slot)
    {
        const item_stack& worn = items.equipped(equip_slot(slot));
        input.insulation += worn.empty() ? 0 : info(worn.id).insulation;
    }

    int before[int(moodle::COUNT)];

    for(int index = 0; index < int(moodle::COUNT); ++index)
    {
        before[index] = body.level(moodle(index));
    }

    input.smoker = _player.skills().has(trait::SMOKER);
    bool craved = body.craving();
    body.tick_minute(input, _random);

    // The Smoker's craving starts (milestone 13).
    if(! sleeping && ! craved && body.craving())
    {
        toast("YOU NEED A SMOKE", hud_tiles::bank_danger, 150);
    }
    ++_world_minute;

    if(! sleeping)
    {
        _tick_flashlight();
    }

    // Hordes migrate every hour (at once while sleeping, when the zombies are not being simulated).
    if(_world_minute % 60 == 0)
    {
        base::expire_fires(_world_minute / 60);
        _campfires_call(_world_minute / 60);
        hordes::hour_passed(_world_minute / 60, sleeping);
    }

    // Chest freezers (milestone 13): food in one with power (the grid, or a generator running near it) doesn't age;
    // worked out once a day, at midnight.
    if(_world_minute % (24 * 60) == 0)
    {
        _freeze_day();
    }

    // Spec: caps show in the UI when close to full. The toast warns once, the pause menu keeps showing it.
    if(_world_minute % 60 == 0)
    {
        bool warn = storage_percent() >= save::storage_warn_percent;

        if(warn && ! _storage_warned)
        {
            toast("WORLD STORAGE NEARLY FULL", hud_tiles::bank_danger, 150);
        }

        _storage_warned = warn;
    }

    // World events: each in-game hour while awake, maybe gunshots, an alarm or a crash somewhere.
    if(! sleeping && _world_minute % 60 == 0)
    {
        _events.hour(*this);
    }

    // Autosave every hour while awake, written over the next frames.
    if(! sleeping && _world_minute % 60 == save::autosave_minute)
    {
        _save(false);
    }

    // Tired 3 warns once before Tired 4 puts the survivor to sleep where they stand.
    if(! sleeping && body.level(moodle::TIRED) >= tired_tuning::warn_level &&
       before[int(moodle::TIRED)] < tired_tuning::warn_level)
    {
        toast("YOU NEED SLEEP", hud_tiles::bank_danger, 180);
    }

    // Each moodle change plays a short sound (spec: stomach growl, shiver).
    if(! sleeping)
    {
        for(int index = 0; index < int(moodle::COUNT); ++index)
        {
            if(body.level(moodle(index)) > before[index])
            {
                moodle changed = moodle(index);
                sfx::id sound = changed == moodle::HUNGRY ? sfx::id::GROWL :
                                changed == moodle::COLD || changed == moodle::FEVER ? sfx::id::SHIVER :
                                changed == moodle::PANIC ? sfx::id::HEARTBEAT :
                                changed == moodle::SICK ? sfx::id::COUGH : sfx::id::MOODLE;
                sfx::play(sound, milli(700));
                break;
            }
        }
    }

    if(body.cause_of_death() != death_cause::NONE)
    {
        _player.die();
        player_bitten();
        return;
    }

    // Exhaustion beyond the limit: the survivor collapses where they stand (spec: then forced sleep).
    if(! sleeping && body.must_pass_out() && ! _sleep_state)
    {
        close_menus();
        _search.close(*this);
        _car.close(*this);
        _vehicles.halt();
        _start_sleeping(tuning(survival_param::PASS_OUT_HOURS), true);
    }
}

void game::_update_drowning()
{
    // Swimming exhausted (milestone 13): a point of health a second, with bubbles, until you reach land or drown.
    if(_dead || ! _player.swimming() ||
       _player.body().level(moodle::EXHAUSTED) < swim_tuning::drown_level)
    {
        _drown_frames = 0;
        return;
    }

    if(_drown_frames == 0)
    {
        toast("YOU ARE DROWNING", hud_tiles::bank_danger, 120);
    }

    if(++_drown_frames % swim_tuning::drown_frames == 0)
    {
        survival& body = _player.body();
        body.drown();
        sfx::play(sfx::id::BUBBLES, milli(800));

        if(body.cause_of_death() != death_cause::NONE)
        {
            _player.die();
            player_bitten();
        }
    }
}

void game::_update_cues()
{
    const survival& body = _player.body();

    // Dehydration and blood loss drain the colour (spec: palette fades); panic shakes the camera by 1 px.
    int thirst = body.water_percent() < 25 ? ((25 - body.water_percent()) * 600) / 25 : 0;
    int blood = body.blood() < 4000 ? bn::min(((4000 - body.blood()) * 800) / 2000, 800) : 0;
    bn::fixed grayscale = bn::fixed(bn::max(thirst, blood)) / 1000;

    if(grayscale != bn::bg_palettes::grayscale_intensity())
    {
        bn::bg_palettes::set_grayscale_intensity(grayscale);
        bn::sprite_palettes::set_grayscale_intensity(grayscale);
    }

    if(body.level(moodle::PANIC) >= 2)
    {
        _camera_controller.shake(1, 2);
    }
}

void game::_freeze_day()
{
    int today = day();
    bool grid = timeline::power_on(today);

    for(int index = 0; index < _containers.count(); ++index)
    {
        const container_record& record = _containers.record(index);

        if(record.table != loot_table::FREEZER || record.page == container_record::no_page ||
           (! grid && ! power::powered(record.tile_x, record.tile_y, _world_minute)))
        {
            continue;
        }

        for(item_stack& stack : _containers.get(index).items)
        {
            if(! stack.empty() && info(stack.id).fresh_days > 0 && stack.extra < today)
            {
                ++stack.extra;
            }
        }
    }
}

int game::storage_percent() const
{
    return bn::max(bn::max(_containers.fill_percent(), world::changes_percent()), base::pieces_percent());
}

void game::_open_tab(int tab)
{
    close_menus();

    if(tab == 0)
    {
        _gear.open(*this);
    }
    else if(tab == 1)
    {
        _health.open(*this);
    }
    else if(tab == 2)
    {
        _craft.open(*this);
    }
    else if(tab == 3)
    {
        _skills.open(*this);
    }
    else if(tab == 4)
    {
        _map.open(*this);
    }
    else
    {
        _menu_tab.open(*this);
    }
}

bool game::_menu_open() const
{
    return _gear.active() || _health.active() || _skills.active() || _craft.active() || _map.active() ||
           _menu_tab.active();
}

void game::_update_menu_dim()
{
    // Menu screens show the world behind their panels, dimmed (playtest 1, mockup 03): the world's palette and the
    // sprites' fade towards black while one is open. Checked once a frame, before the frame is shown, so a tab
    // switch (close then open) never flashes.
    bool want = _hud.screen_open();

    if(want == _menu_dimmed)
    {
        return;
    }

    _menu_dimmed = want;
    _renderer.set_dimmed(want);

    // The sleep fade owns the sprites' fade (reading from the Gear tab closes it and starts the fast-forward in
    // the same frame); it clears it when it ends.
    if(! _sleep_state)
    {
        bn::sprite_palettes::set_fade(bn::color(0, 0, 0),
                                      want ? bn::fixed(menu_sprite_dim_percent) / 100 : bn::fixed(0));
    }
}

void game::_screen_toast()
{
    // Popups raised in a menu show over it (playtest 1); when one goes, the screen redraws what it covered.
    if(_hud.screen_toast())
    {
        _gear.refresh();
        _health.refresh();
        _craft.refresh();
        _skills.refresh();
        _map.refresh();
        _menu_tab.refresh();
        _search.refresh();
        _car.refresh();
        _safe.refresh();
    }
}

void game::close_menus()
{
    _gear.close(*this);
    _health.close(*this);
    _skills.close(*this);
    _craft.close(*this);
    _map.close(*this);
    _menu_tab.close(*this);
}

void game::_update_search(const input_frame& input)
{
    _search.update(*this, input);

    // A car's trunk or glove box closed: back to the car screen (playtest 4).
    if(! _search.active() && _search_car >= 0)
    {
        _car.open(*this, _search_car);
        _search_car = -1;
    }
}

void game::search_car_container(int container_index, int vehicle)
{
    begin_search(container_index);
    _search_car = _search.active() ? vehicle : -1;
}

void game::begin_search(int container_index, int extra_items, bool unlocked)
{
    _search_car = -1;
    _hud.set_prompt(-1, 0, "");
    const container_record& value = _containers.record(container_index);

    if(value.table == loot_table::GUN_SAFE && ! value.opened && ! unlocked)
    {
        _safe.open(*this, container_index);
        return;
    }

    _search.open(*this, container_index, extra_items);
}

void game::respawn_zombies()
{
    _zombies.clear();

    if(! world::test_world())
    {
        hordes::fill(*this);
        return;
    }

    for(const auto& tile : test_zombies)
    {
        int tile_x = tile[0];
        int tile_y = tile[1];

        if(open_tile_near(tile_x, tile_y))
        {
            _zombies.spawn(tile_feet(tile_x, tile_y), _random);
        }
    }
}

void game::spawn_horde()
{
    _zombies.clear();

    int player_tile_x = _player.position().x().integer() >> world::tile_shift;
    int player_tile_y = _player.position().y().integer() >> world::tile_shift;

    // The test map, or the generated chunks around the player.
    bool test = world::test_world();
    int span = world::chunk_tiles + (world::chunk_tiles / 2);

    for(int attempt = 0; attempt < horde_size * 8 && _zombies.count() < horde_size; ++attempt)
    {
        int tile_x = test ? _random.get_int(world::width()) : player_tile_x + _random.get_int(span * 2) - span;
        int tile_y = test ? _random.get_int(world::height()) : player_tile_y + _random.get_int(span * 2) - span;

        if(bn::abs(tile_x - player_tile_x) + bn::abs(tile_y - player_tile_y) < horde_min_distance_tiles)
        {
            continue;
        }

        if(open_tile_near(tile_x, tile_y))
        {
            _zombies.spawn(tile_feet(tile_x, tile_y), _random);
        }
    }
}

void game::give_item(item_id id)
{
    const item_info& item = info(id);
    item_stack stack = make_stack(id, item.durability == 0 ? item.stack : 1);

    if(item.fresh_days > 0)
    {
        stack.extra = uint16_t(day());
    }

    if(skill taught; book_skill(id, taught))
    {
        stack.extra = uint16_t(bn::max(_player.skills().current_volume(taught), 1));
    }

    if(_player.items().add(stack))
    {
        sfx::play(sfx::id::PICKUP, milli(700));
    }
    else
    {
        sfx::play(sfx::id::DENIED, milli(600));
    }
}

void game::give_test_kit()
{
    _player.give_test_kit(*this);
}

void game::teleport_to_settlement(int index)
{
    const macro::settlement& value = macro::settlement_at(index);
    teleport_to_tile((value.cell_x * macro::cell_tiles) + (macro::cell_tiles / 2),
                     (value.cell_y * macro::cell_tiles) + (macro::cell_tiles / 2));
}

void game::teleport_to_tile(int tile_x, int tile_y)
{
    _zombies.dissolve(_player.position(), 0);
    flow::reset();
    _edges.hide();
    _build.close(*this);
    _car.close(*this);
    static_cast<void>(_vehicles.leave(*this, false));
    _vehicles.rescan();
    _events.hide();
    _player.teleport(tile_feet(tile_x, tile_y));
    world::prepare(_player.position().x().integer(), _player.position().y().integer());
    _camera_controller.snap_to(_player);
    _renderer.update(_camera_controller.left(), _camera_controller.top());
    hordes::fill(*this);
}

void game::drink()
{
    // Rain water is cleaner than a river or pond, but neither is boiled (that comes with cooking).
    bool rain = base::take_rain_water(_drink_tile_x, _drink_tile_y);
    // A well's water (milestone 13) is as clean as rain water, and never runs dry.
    bool well = world::wall_gfx(_drink_tile_x, _drink_tile_y) == world_gfx::WELL;
    int sick = rain || well ? timeline_tuning::rain_sick_percent : timeline_tuning::river_sick_percent;
    _player.body().drink(timeline_tuning::raw_water, sick, timeline_tuning::sick_amount, _random);
    sfx::play(sfx::id::GULP, milli(600));

    // Watering cans are topped up at the same time.
    for(int slot = 0; slot < inventory::slots; ++slot)
    {
        item_stack& can = _player.items().slot(slot);

        if(waters_plots(can.id))
        {
            can.amount = info(can.id).durability;
        }
    }
}

item_stack* game::_watering_can()
{
    for(int slot = 0; slot < inventory::slots; ++slot)
    {
        item_stack& can = _player.items().slot(slot);

        if(waters_plots(can.id) && can.amount > 0)
        {
            return &can;
        }
    }

    return nullptr;
}

const char* game::_plot_prompt(int plot)
{
    switch(farm::stage(plot, day()))
    {

    case plot_stage::EMPTY:
        for(int kind = 1; kind < crop_count; ++kind)
        {
            const crop_info& value = info(crop(kind));

            if(_player.items().count(value.seeds) > 0)
            {
                ui::append(_tend_text, ui::append(_tend_text, 0, "[A] PLANT "), value.name);
                return _tend_text;
            }
        }

        return nullptr;

    case plot_stage::RIPE:
        return "[A] HARVEST";

    case plot_stage::ROTTEN:
        return "[A] CLEAR ROTTEN CROP";

    default:
        return ! farm::wet(plot, day()) && _watering_can() ? "[A] WATER" : nullptr;
    }
}

void game::_update_can_lines()
{
    using namespace base_tuning;
    int feet_x = _player.position().x().floor_integer() >> world::tile_shift;
    int feet_y = (_player.position().y().floor_integer() - 2) >> world::tile_shift;
    int left = _camera_controller.left() >> world::tile_shift;
    int top = _camera_controller.top() >> world::tile_shift;
    int tiles_x[8];
    int tiles_y[8];
    int found = base::can_lines_in(left - 1, top - 1, left + 16, top + 11, tiles_x, tiles_y, 8);
    bool player_on = false;

    for(int line = 0; line < found; ++line)
    {
        // The player rattles a line stepping onto it (not sneaking over it crouched); zombies on it rattle it.
        bool here = feet_x == tiles_x[line] && feet_y == tiles_y[line];
        bool crossed = here && ! _on_can_line && ! _player.crouched();
        bool zombie = false;
        player_on |= here;

        for(int index = 0; index < _zombies.count() && ! crossed; ++index)
        {
            const bn::fixed_point& position = _zombies.position(index);
            zombie = (position.x().floor_integer() >> world::tile_shift) == tiles_x[line] &&
                     ((position.y().floor_integer() - 2) >> world::tile_shift) == tiles_y[line];
            crossed = zombie;
        }

        if(crossed && _frames - _can_rattle_frame >= can_rattle_frames)
        {
            _can_rattle_frame = _frames;
            bn::fixed_point centre((tiles_x[line] * world::tile_size) + (world::tile_size / 2),
                                   (tiles_y[line] * world::tile_size) + (world::tile_size / 2));
            noise(centre, can_noise);
            sfx::play(sfx::id::GLASS, milli(500));

            if(zombie)
            {
                toast("THE CANS RATTLE", hud_tiles::bank_danger, 90);
            }
        }
    }

    _on_can_line = player_on;
}

item_stack* game::_fuel_can(bool full_ok)
{
    for(int slot = 0; slot < inventory::slots; ++slot)
    {
        item_stack& can = _player.items().slot(slot);

        if(can.id == item_id::FUEL_CAN && (full_ok ? can.amount > 0 : can.amount < info(can.id).durability))
        {
            return &can;
        }
    }

    return nullptr;
}

const char* game::_device_prompt(int tile_x, int tile_y)
{
    if(int generator = power::at(tile_x, tile_y); generator >= 0)
    {
        int room = power_tuning::tank_minutes - power::fuel(generator, _world_minute);

        if(room >= power_tuning::minutes_per_litre && _fuel_can(true))
        {
            return "[A] ADD FUEL";
        }

        return power::running(generator, _world_minute) ? "[A] TURN OFF" : "[A] TURN ON";
    }

    if(int trap = traps::at(tile_x, tile_y); trap >= 0)
    {
        return traps::caught(trap, day(), _player.skills().level(skill::FORAGING)) ? "[A] TAKE CATCH" : nullptr;
    }

    if(world::wall_gfx(tile_x, tile_y) == world_gfx::PUMP && timeline::power_on(day()) && _fuel_can(false))
    {
        return "[A] FILL FUEL CAN";
    }

    if(world::wall_gfx(tile_x, tile_y) == world_gfx::TV)
    {
        return "[A] WATCH TV";
    }

    return nullptr;
}

bool game::_doorway_occupied(int tile_x, int tile_y) const
{
    // A feet box (half its width each side, its height above the feet) overlapping the doorway's tile: the
    // survivor's hitbox, or a zombie's body (a little wider, as they push past).
    int left = tile_x * world::tile_size;
    int right = left + world::tile_size;
    int top = tile_y * world::tile_size;
    int bottom = top + world::tile_size;

    auto overlaps = [=](const bn::fixed_point& feet, int half_width, int height)
    {
        int x = feet.x().integer();
        int y = feet.y().integer();
        return x + half_width > left && x - half_width < right && y > top && y - height < bottom;
    };

    if(overlaps(_player.position(), tuning(move_param::HITBOX_W).integer() / 2,
                tuning(move_param::HITBOX_H).integer()))
    {
        return true;
    }

    constexpr int zombie_half_width = 5;
    constexpr int zombie_height = 5;

    for(int index = 0, count = _zombies.count(); index < count; ++index)
    {
        if(overlaps(_zombies.position(index), zombie_half_width, zombie_height))
        {
            return true;
        }
    }

    return false;
}

void game::use_door()
{
    bool side_wall;
    world::door_state door = world::door_of(world::wall_gfx(_door_x, _door_y), side_wall);
    bn::fixed_point centre((_door_x * world::tile_size) + (world::tile_size / 2),
                           (_door_y * world::tile_size) + (world::tile_size / 2));
    int pan = screen_x(centre);
    world::door_state next;

    if(door == world::door_state::CLOSED && world::door_locked(_door_x, _door_y))
    {
        switch(_lock_way(_door_x, _door_y))
        {

        case work_kind::UNLOCK:
            _unlocked(_door_x, _door_y, world::door_state::OPEN, "UNLOCKED");
            break;

        case work_kind::FORCE:
            _door_work = work_kind::FORCE;
            _door_work_frame = 0;
            _door_work_length = world::lock_tuning::force_frames;
            sfx::play(sfx::id::CROWBAR, milli(500), pan);
            break;

        case work_kind::PICK:
            _door_work = work_kind::PICK;
            _door_work_frame = 0;
            _door_work_length = _player.skills().job() == occupation::BURGLAR ? world::lock_tuning::burglar_pick_frames :
                                _player.items().find(item_id::LOCKPICK) >= 0 ? item_tuning::lockpick_frames :
                                                                               world::lock_tuning::pick_frames;
            sfx::play(sfx::id::LOCKPICK, milli(600), pan);
            break;

        default:
            sfx::play(sfx::id::RATTLE, milli(800), pan);
            toast("LOCKED", hud_tiles::bank_danger, 60);
            break;
        }

        #if KX_BENCH == 12
            BN_LOG("KXLOCK A on a locked door ", _door_x, ",", _door_y, " way=", int(_lock_way(_door_x, _door_y)),
               " inside=", _inside, " house here=", world::house_id_at(_player.position().x().integer() >> 4,
                                                                         (_player.position().y().integer() - 1) >> 4),
               " door's house=", world::house_id_at(_door_x, _door_y));
        #endif

        return;
    }

    if(door == world::door_state::CLOSED)
    {
        next = world::door_state::OPEN;
    }
    else if(door == world::door_state::OPEN && ! _doorway_occupied(_door_x, _door_y))
    {
        next = world::door_state::CLOSED;
    }
    else
    {
        return;
    }

    // Remembered like a smashed window (the world's wall changes): every door touched keeps its state.
    if(! world::set_wall_gfx(_door_x, _door_y, world::door_gfx(next, side_wall)))
    {
        sfx::play(sfx::id::DENIED, milli(600));
        return;
    }

    refresh_tile(_door_x, _door_y);

    if(next == world::door_state::OPEN)
    {
        sfx::play(sfx::id::DOOR_OPEN, milli(700), pan);
    }
    else
    {
        sfx::play(sfx::id::DOOR_CLOSE, milli(800), pan);
        noise(centre, base_tuning::door_close_noise);
    }

    #if KX_BENCH == 12
        BN_LOG("KXDOOR ", next == world::door_state::OPEN ? "opened " : "closed ", _door_x, ",", _door_y,
               " side=", side_wall, " solid=", world::solid(_door_x, _door_y));
    #endif
}

game::work_kind game::_lock_way(int tile_x, int tile_y)
{
    uint16_t house = world::house_id_at(tile_x, tile_y);
    const inventory& items = _player.items();

    // From inside, the latch turns; outside, its key.
    if(_inside && world::house_id_at(_player.position().x().integer() >> world::tile_shift,
                                      (_player.position().y().integer() - 1) >> world::tile_shift) == house)
    {
        return work_kind::UNLOCK;
    }

    for(int slot = 0; slot < inventory::slots; ++slot)
    {
        if(items.slot(slot).id == item_id::HOUSE_KEY && items.slot(slot).extra == house)
        {
            return work_kind::UNLOCK;
        }
    }

    if(forces_doors(items.equipped(equip_slot::HAND).id))
    {
        return work_kind::FORCE;
    }

    // Milestone 14: a lockpick, else a screwdriver.
    return items.find(item_id::LOCKPICK) >= 0 || items.find(item_id::SCREWDRIVER) >= 0 ? work_kind::PICK :
                                                                                        work_kind::NONE;
}

void game::_unlocked(int tile_x, int tile_y, world::door_state state, const char* message)
{
    bool side_wall;
    static_cast<void>(world::door_of(world::wall_gfx(tile_x, tile_y), side_wall));
    uint16_t house = world::house_id_at(tile_x, tile_y);
    static_cast<void>(world::set_wall_gfx(tile_x, tile_y, world::door_gfx(state, side_wall)));
    refresh_tile(tile_x, tile_y);

    // The house is open: its key's mark has done its job.
    world_map::remove_auto_marks(world_map::auto_mark::KEY, house);
    toast(message, hud_tiles::bank_accent, 60);

    #if KX_BENCH == 12
        BN_LOG("KXLOCK ", message, " door ", tile_x, ",", tile_y, " house=", house, " state=", int(state),
               " locked now=", world::door_locked(tile_x, tile_y), " auto marks=", world_map::auto_mark_count());
    #endif
}

void game::chop()
{
    _door_work = work_kind::CHOP;
    _door_work_frame = 0;
    _door_work_length = world::chop_tuning::frames;
    sfx::play(sfx::id::CHOP, milli(700),
              screen_x(bn::fixed_point(_door_x * world::tile_size, _door_y * world::tile_size)));
}

void game::catch_hen()
{
    static_cast<void>(_animals.catch_hen(*this, _farm_animal_id));
    _farm_animal_id = -1;
}

void game::milk_goat()
{
    static_cast<void>(_animals.milk_goat(*this, _farm_animal_id));
    _farm_animal_id = -1;
}

void game::butcher()
{
    _door_work = work_kind::BUTCHER;
    _door_work_frame = 0;
    _door_work_length = animal_tuning::butcher_frames;
    sfx::play(sfx::id::HIT_BLADE, milli(500));
}

void game::_update_door_work(const input_frame& input)
{
    if(_door_work == work_kind::NONE)
    {
        return;
    }

    bool side_wall;
    weapon held = info(_player.items().equipped(equip_slot::HAND).id).melee;
    bool still_locked = _door_work == work_kind::BUTCHER ?
                        _animals.carcass_exists(_butcher_id) && (held == weapon::KNIFE || held == weapon::AXE) :
                        _door_work == work_kind::CHOP ?
                        world::tree_at(_door_x, _door_y) &&
                        chops_trees(_player.items().equipped(equip_slot::HAND).id) :
                        world::door_of(world::wall_gfx(_door_x, _door_y), side_wall) == world::door_state::CLOSED &&
                        world::door_locked(_door_x, _door_y);

    // Letting go of A, moving, being grabbed or the door changing stops it; it starts over next time.
    if(! input.a_held || input.dx || input.dy || _player.action() != player_action::NONE || _player.grabbed() ||
       _dead || ! still_locked)
    {
        #if KX_BENCH == 12
            BN_LOG("KXLOCK work stopped at ", _door_work_frame, " of ", _door_work_length);
        #endif

        _door_work = work_kind::NONE;
        return;
    }

    bn::fixed_point centre((_door_x * world::tile_size) + (world::tile_size / 2),
                           (_door_y * world::tile_size) + (world::tile_size / 2));

    if(_door_work == work_kind::BUTCHER)
    {
        centre = _animals.position_of(_butcher_id);
    }

    int pan = screen_x(centre);

    if(_door_work == work_kind::BUTCHER)
    {
        if(++_door_work_frame % world::lock_tuning::sound_interval == 0)
        {
            sfx::play(sfx::id::HIT_BLADE, milli(350), pan);
        }

        if(_door_work_frame < _door_work_length)
        {
            return;
        }

        // Venison and the hide (at your feet if the bag is full); Foraging for the work. A boar gives more meat.
        _door_work = work_kind::NONE;
        animal_kind kind = _animals.kind_of_id(_butcher_id);
        bool boar = kind == animal_kind::BOAR;
        bool wolf = kind == animal_kind::WOLF;
        bool goat = kind == animal_kind::GOAT;
        int meat_pieces = boar ? animal_tuning::boar_meat : wolf ? animal_tuning::wolf_meat :
                          goat ? animal_tuning::goat_meat : animal_tuning::venison;
        int hides = boar ? animal_tuning::boar_hides : wolf ? animal_tuning::wolf_hides :
                    goat ? animal_tuning::goat_hides : animal_tuning::hides;
        _animals.remove(_butcher_id);
        _butcher_id = -1;
        // Raw meat doesn't stack: one piece at a time.
        for(int piece = 0; piece < meat_pieces; ++piece)
        {
            item_stack meat = make_stack(item_id::RAW_MEAT);
            meat.extra = uint16_t(day());

            if(! _player.items().add(meat))
            {
                static_cast<void>(drop_item(meat));
            }
        }

        item_stack hide = make_stack(item_id::HIDE, hides);

        if(! _player.items().add(hide))
        {
            static_cast<void>(drop_item(hide));
        }

        _player.train(*this, skill::FORAGING, animal_tuning::butcher_xp);
        sfx::play(sfx::id::PICKUP, milli(800));
        toast(boar ? "6 RAW MEAT, 1 HIDE" : wolf ? "2 RAW MEAT, 1 HIDE" : goat ? "3 RAW MEAT, 1 HIDE" :
              "4 VENISON, 1 HIDE", hud_tiles::bank_accent, 120);

        #if KX_BENCH == 13 || KX_BENCH == 15
            BN_LOG("KXWILD butchered: raw meat=", _player.items().count(item_id::RAW_MEAT), " hide=",
                   _player.items().count(item_id::HIDE), " foraging xp level=",
                   _player.skills().level(skill::FORAGING), " animals=", _animals.count());
        #endif

        return;
    }

    if(++_door_work_frame % world::lock_tuning::sound_interval == 0)
    {
        sfx::play(_door_work == work_kind::FORCE ? sfx::id::CROWBAR : _door_work == work_kind::CHOP ? sfx::id::CHOP :
                  sfx::id::LOCKPICK, milli(450), pan);

        // Forcing and chopping are loud all the way through.
        if(_door_work == work_kind::FORCE)
        {
            noise(centre, world::lock_tuning::force_noise / 2);
        }
        else if(_door_work == work_kind::CHOP)
        {
            noise(centre, world::chop_tuning::noise / 2);
        }
    }

    if(_door_work_frame < _door_work_length)
    {
        return;
    }

    work_kind work = _door_work;
    _door_work = work_kind::NONE;

    if(work == work_kind::CHOP)
    {
        // Timber: a stump left, 2 logs (at your feet if the bag is full), the axe worn.
        static_cast<void>(world::chop_tree(_door_x, _door_y));
        refresh_tile(_door_x, _door_y);
        refresh_tile(_door_x, _door_y - 1);
        sfx::play(sfx::id::TIMBER, 1, pan);
        noise(centre, world::chop_tuning::noise);
        item_stack logs = make_stack(item_id::LOGS, world::chop_tuning::logs);

        if(! _player.items().add(logs))
        {
            static_cast<void>(drop_item(logs));
        }

        item_stack& axe = _player.items().equipped(equip_slot::HAND);

        if(chops_trees(axe.id))
        {
            if(axe.amount > world::chop_tuning::axe_wear)
            {
                axe.amount = uint8_t(axe.amount - world::chop_tuning::axe_wear);
            }
            else
            {
                axe = item_stack();
                sfx::play(sfx::id::WEAPON_BREAK, milli(700));
            }
        }

        _player.train(*this, skill::AXE, world::chop_tuning::axe_xp);
        toast("TIMBER: 2 LOGS", hud_tiles::bank_accent, 90);

        #if KX_BENCH == 12
            BN_LOG("KXCHOP felled ", _door_x, ",", _door_y, " logs=", _player.items().count(item_id::LOGS),
                   " axe=", int(_player.items().equipped(equip_slot::HAND).amount), " wall now=",
                   int(world::wall_gfx(_door_x, _door_y)), " canopy=",
                   int(world::gfx_at(world::layer::CANOPY, _door_x, _door_y)), " above=",
                   int(world::gfx_at(world::layer::CANOPY, _door_x, _door_y - 1)));
        #endif

        return;
    }

    if(work == work_kind::FORCE)
    {
        // The frame splits: the door is broken open for good, and the crowbar wears.
        sfx::play(sfx::id::HIT_HEAVY, 1, pan);
        noise(centre, world::lock_tuning::force_noise);
        item_stack& crowbar = _player.items().equipped(equip_slot::HAND);

        if(forces_doors(crowbar.id))
        {
            if(crowbar.amount > world::lock_tuning::force_wear)
            {
                crowbar.amount = uint8_t(crowbar.amount - world::lock_tuning::force_wear);
            }
            else
            {
                crowbar = item_stack();
                sfx::play(sfx::id::WEAPON_BREAK, milli(700));
            }
        }

        _unlocked(_door_x, _door_y, world::door_state::BROKEN, "FORCED OPEN");
        return;
    }

    // Picking: the Burglar never fails; anyone else may slip (less with Sneaking), with a clatter.
    int base_fail = _player.items().find(item_id::LOCKPICK) >= 0 ? item_tuning::lockpick_fail_percent :
                                                                    world::lock_tuning::pick_fail_percent;
    int fail = _player.skills().job() == occupation::BURGLAR ? 0 :
               bn::max(base_fail -
                       (_player.skills().level(skill::SNEAKING) * world::lock_tuning::pick_fail_per_sneaking), 0);
    bool failed = _random.get_int(100) < fail;

    #if KX_BENCH == 12
        BN_LOG("KXLOCK pick done: fail chance=", fail, " failed=", failed);
    #endif

    if(failed)
    {
        sfx::play(sfx::id::DROP, milli(800), pan);
        noise(centre, world::lock_tuning::pick_fail_noise);
        toast("THE PICK SLIPS", hud_tiles::bank_danger, 60);
        return;
    }

    sfx::play(sfx::id::DOOR_OPEN, milli(700), pan);
    _unlocked(_door_x, _door_y, world::door_state::OPEN, "LOCK PICKED");
}

void game::item_taken(const item_stack& stack)
{
    // A house key: its house goes on the Map tab's automatic marks (seen with a map).
    if(stack.id != item_id::HOUSE_KEY || ! world::house_locked(stack.extra))
    {
        return;
    }

    int door_x = 0;
    int door_y = 0;

    // Not once its door has been opened (or broken): the key has nothing left to show.
    if(world::house_door(stack.extra, door_x, door_y) && ! world::wall_changed(door_x, door_y))
    {
        world_map::add_auto_mark(world_map::auto_mark::KEY, stack.extra, door_x, door_y);

        if(_player.items().find(item_id::MAP) >= 0)
        {
            toast("ITS HOUSE IS ON YOUR MAP", hud_tiles::bank_accent, 90);
        }
    }

    #if KX_BENCH == 12
        BN_LOG("KXLOCK key taken: house=", stack.extra, " door=", door_x, ",", door_y, " auto marks=",
               world_map::auto_mark_count());
    #endif
}

namespace
{
    constexpr int note_message_frames = 360;

    // Letters (milestone 12.7): a few handwritten notes that name a place ("@" is the town) and what happened there.
    struct letter
    {
        const char* lines[3];
    };

    constexpr letter letters[] = {
        { { "IF YOU FIND THIS, WE WENT TO", "@. THE CHURCH WAS SAFE.", "- M." } },
        { { "THEY CAME OUT OF @ AT NIGHT.", "HUNDREDS OF THEM.", "DON'T GO THERE." } },
        { { "DAD, I'M AT GRANDMA'S IN @.", "WAIT FOR ME THERE.", "LOVE, ANA." } },
        { { "THE ARMY TRUCKS LEFT @ ON", "THE 3RD. NOBODY CAME BACK.", nullptr } },
        { { "DON'T DRINK FROM THE RIVER", "BY @. SOMETHING'S IN IT.", nullptr } },
        { { "THE DOCTOR IN @ SAYS IT'S", "A FLU. IT IS NOT A FLU.", nullptr } },
    };

    constexpr const char* directions[] = { "NORTH-EAST", "SOUTH-EAST", "SOUTH-WEST", "NORTH-WEST" };

    // A line with "@" replaced by a town's name.
    void fill_line(char* output, const char* line, const char* town)
    {
        int length = 0;

        for(const char* c = line; *c && length < 40; ++c)
        {
            if(*c == '@')
            {
                for(const char* t = town; *t && length < 40; ++t)
                {
                    output[length++] = *t;
                }
            }
            else
            {
                output[length++] = *c;
            }
        }

        output[length] = 0;
    }

    void code_text(char* output, int code)
    {
        output[0] = char('0' + (code / 100));
        output[1] = '-';
        output[2] = char('0' + ((code / 10) % 10));
        output[3] = '-';
        output[4] = char('0' + (code % 10));
        output[5] = 0;
    }
}

void game::read_note(const item_stack& stack)
{
    char lines[3][41] = {};
    const char* shown[3] = {};
    close_menus();

    if(stack.id == item_id::NOTE_SAFE)
    {
        // A safe's code: its building goes on the map.
        uint16_t house = stack.extra;
        int settlement = world::house_settlement(house);
        const char* town = settlement >= 0 ? world_map::settlement_name(settlement) : "TOWN";
        char code[6];
        code_text(code, world::safe_code(house));

        if(world::house_is_police(house))
        {
            fill_line(lines[0], "STATION GUN SAFE, @.", town);
            fill_line(lines[1], "NEW CODE: @.", code);
            fill_line(lines[2], "SHRED THIS NOTE.", "");
        }
        else if(world::house_is_armory(house))
        {
            fill_line(lines[0], "ARMORY SAFES, @.", town);
            fill_line(lines[1], "CODE @. ALL OF THEM.", code);
            fill_line(lines[2], "SHRED THIS NOTE.", "");
        }
        else
        {
            fill_line(lines[0], "THE SAFE IN THE BEDROOM,", "");
            fill_line(lines[1], "A HOUSE IN @.", town);
            fill_line(lines[2], "CODE @. DON'T TELL ANYONE.", code);
        }

        int safe_x;
        int safe_y;

        if(world::house_safe(house, safe_x, safe_y))
        {
            world_map::add_auto_mark(world_map::auto_mark::NOTE, house, safe_x, safe_y);
        }
    }
    else if(stack.id == item_id::NOTE_STASH)
    {
        // A stash: put out in the woods now (a bag on the ground, searched like a crate), and on the map.
        int settlement = stack.extra;
        int stash_x;
        int stash_y;
        int way = 0;
        const char* town = world_map::settlement_name(settlement);

        if(world::stash_tile(settlement, stash_x, stash_y, way))
        {
            if(_containers.at(stash_x, stash_y) < 0 && world::wall_gfx(stash_x, stash_y) != world_gfx::STASH)
            {
                static_cast<void>(world::set_wall_gfx(stash_x, stash_y, world_gfx::STASH));
                refresh_tile(stash_x, stash_y);
            }

            world_map::add_auto_mark(world_map::auto_mark::NOTE, uint16_t(0xF000 | settlement), stash_x,
                                     stash_y);

            #if KX_BENCH == 12
                BN_LOG("KXNOTE stash of ", town, " at ", stash_x, ",", stash_y, " way=", way);
            #endif
        }

        fill_line(lines[0], "LEFT SUPPLIES @ OF", directions[way & 3]);
        fill_line(lines[1], "@, IN A GREEN BAG UNDER", town);
        fill_line(lines[2], "THE TREES. COME BACK FOR THEM.", "");
    }
    else
    {
        const letter& value = letters[(stack.extra >> 8) % (sizeof(letters) / sizeof(letters[0]))];
        int settlement = stack.extra & 0xFF;
        const char* town = settlement < macro::settlement_count() ? world_map::settlement_name(settlement) : "TOWN";

        for(int line = 0; line < 3; ++line)
        {
            if(value.lines[line])
            {
                fill_line(lines[line], value.lines[line], town);
            }
        }
    }

    for(int line = 0; line < 3; ++line)
    {
        shown[line] = lines[line][0] ? lines[line] : nullptr;
    }

    _hud.message(shown[0], shown[1], shown[2], note_message_frames);
    sfx::play(sfx::id::RUMMAGE, milli(400));

    #if KX_BENCH == 12
        BN_LOG("KXNOTE read ", int(stack.id), ": ", lines[0], " / ", lines[1], " / ", lines[2], " auto marks=",
               world_map::auto_mark_count());
    #endif
}

void game::tend()
{
    _tend_at(_tend_x, _tend_y);
}

void game::_tend_at(int tile_x, int tile_y)
{
    using namespace farm_tuning;

    // A generator: fuel first, then the switch.
    if(int generator = power::at(tile_x, tile_y); generator >= 0)
    {
        item_stack* can = _fuel_can(true);
        int room = power_tuning::tank_minutes - power::fuel(generator, _world_minute);

        if(can && room >= power_tuning::minutes_per_litre)
        {
            int taken = power::refuel(generator, can->amount, _world_minute);
            can->amount = uint8_t(can->amount - taken);
            toast("FUEL ADDED", hud_tiles::bank_accent, 60);
            sfx::play(sfx::id::GULP, milli(600));
        }
        else if(power::running(generator, _world_minute))
        {
            static_cast<void>(power::switch_on(generator, false, _world_minute));
            toast("GENERATOR OFF", hud_tiles::bank_accent, 60);
            sfx::play(sfx::id::SWAP, milli(600));
        }
        else if(power::switch_on(generator, true, _world_minute))
        {
            toast("GENERATOR ON", hud_tiles::bank_accent, 60);
            sfx::play(sfx::id::SWAP, milli(800));
        }
        else
        {
            toast("THE GENERATOR HAS NO FUEL", hud_tiles::bank_danger, 60);
            sfx::play(sfx::id::DENIED, milli(600));
        }

        return;
    }

    // A trap: small game, raw meat to cook.
    if(int trap = traps::at(tile_x, tile_y); trap >= 0)
    {
        int foraging = _player.skills().level(skill::FORAGING);

        if(traps::caught(trap, day(), foraging))
        {
            int pieces = 1 + (foraging >= trap_tuning::extra_meat_level);

            for(int piece = 0; piece < pieces; ++piece)
            {
                item_stack meat = make_stack(item_id::RAW_MEAT);
                meat.extra = uint16_t(day());

                if(! _player.items().add(meat))
                {
                    static_cast<void>(drop_item(meat));
                }
            }

            bool bird = traps::caught_bird(trap);
            traps::reset_trap(trap, day());
            _player.train(*this, skill::FORAGING, trap_tuning::catch_xp);
            toast(bird ? "CAUGHT A BIRD" : "CAUGHT A RABBIT", hud_tiles::bank_accent, 90);
            sfx::play(sfx::id::PICKUP, milli(800));
        }

        return;
    }

    // A television (milestone 13): a broadcast while the power is on (the grid or a running generator nearby),
    // static after.
    if(world::wall_gfx(tile_x, tile_y) == world_gfx::TV)
    {
        _events.watch_tv(*this, timeline::power_on(day()) || power::powered(tile_x, tile_y, _world_minute));
        return;
    }

    // A fuel pump: fills every fuel can carried.
    if(world::wall_gfx(tile_x, tile_y) == world_gfx::PUMP)
    {
        while(item_stack* can = _fuel_can(false))
        {
            can->amount = info(can->id).durability;
        }

        toast("FUEL CAN FILLED", hud_tiles::bank_accent, 60);
        sfx::play(sfx::id::GULP, milli(600));
        return;
    }

    int plot = farm::at(tile_x, tile_y);

    if(plot < 0)
    {
        return;
    }

    inventory& items = _player.items();
    int today = day();
    plot_stage stage = farm::stage(plot, today);
    char text[32];

    switch(stage)
    {

    case plot_stage::EMPTY:
        for(int kind = 1; kind < crop_count; ++kind)
        {
            const crop_info& value = info(crop(kind));

            if(items.count(value.seeds) > 0)
            {
                items.remove(value.seeds, 1);
                farm::plant(plot, crop(kind), today);
                _player.train(*this, skill::FARMING, plant_xp);
                ui::append(text, ui::append(text, 0, value.name), " PLANTED");
                toast(text, hud_tiles::bank_accent, 90);
                sfx::play(sfx::id::DROP, milli(700));
                break;
            }
        }
        break;

    case plot_stage::RIPE:
    {
        const crop_info& value = info(farm::crop_of(plot));
        int level = _player.skills().level(skill::FARMING);
        int amount = value.yield_min + _random.get_int(value.yield_max - value.yield_min + 1) +
                     (level / levels_per_extra);

        for(int count = 0; count < amount; ++count)
        {
            item_stack crop_stack = make_stack(value.harvest);
            crop_stack.extra = uint16_t(today);

            if(! items.add(crop_stack))
            {
                static_cast<void>(drop_item(crop_stack));
            }
        }

        if(_random.get_int(100) < seed_percent + (level * seed_percent_per_level))
        {
            item_stack seeds = make_stack(value.seeds);

            if(! items.add(seeds))
            {
                static_cast<void>(drop_item(seeds));
            }
        }

        farm::clear(plot, today);
        _player.train(*this, skill::FARMING, harvest_xp);
        int length = ui::append(text, 0, "HARVESTED ");
        length = ui::append_int(text, length, amount);
        ui::append(text, ui::append(text, length, " "), value.name);
        toast(text, hud_tiles::bank_accent, 120);
        sfx::play(sfx::id::PICKUP, milli(800));
        break;
    }

    case plot_stage::ROTTEN:
        farm::clear(plot, today);
        toast("PLOT CLEARED", hud_tiles::bank_accent, 60);
        sfx::play(sfx::id::DROP, milli(600));
        break;

    default:
        if(item_stack* can = _watering_can(); can && ! farm::wet(plot, today))
        {
            --can->amount;
            farm::water(plot, today);
            _player.train(*this, skill::FARMING, water_xp);
            sfx::play(sfx::id::GULP, milli(500));
        }
        break;
    }

    // The tile follows the stage.
    world_gfx wanted = farm::gfx(farm::stage(plot, today));

    if(world::wall_gfx(tile_x, tile_y) != wanted)
    {
        static_cast<void>(world::set_wall_gfx(tile_x, tile_y, wanted));
        refresh_tile(tile_x, tile_y);
    }
}

void game::forage()
{
    int feet_x = _player.position().x().floor_integer() >> world::tile_shift;
    int feet_y = (_player.position().y().floor_integer() - 2) >> world::tile_shift;

    if(_containers.at(feet_x, feet_y) >= 0)
    {
        return;
    }

    // What grows here: by water (fish), in woods (mushrooms), or in the open.
    bool waterside = false;

    for(int y = feet_y - 1; y <= feet_y + 1; ++y)
    {
        for(int x = feet_x - 1; x <= feet_x + 1; ++x)
        {
            waterside |= world::terrain_at(x, y) == terrain::SHALLOW_WATER;
        }
    }

    terrain ground = world::terrain_at(feet_x, feet_y);
    bool woods = ground == terrain::FOREST || ground == terrain::MUD;
    loot_table table = waterside ? loot_table::FORAGE_WATER : woods ? loot_table::FORAGE_FOREST :
                                                                     loot_table::FORAGE_FIELD;
    int index = _containers.add(container_kind::FORAGE, table, feet_x, feet_y);

    if(index < 0)
    {
        return;
    }

    _foraged[_next_foraged] = foraged_area{ int16_t(feet_x >> 3), int16_t(feet_y >> 3), int16_t(day()) };
    _next_foraged = (_next_foraged + 1) % _foraged.size();

    // Skill: an extra find every 2 Foraging levels; park rangers find one more in the woods.
    const character& skills = _player.skills();
    int extra = skills.level(skill::FORAGING) / character_tuning::forage_levels_per_find;
    extra += woods && skills.job() == occupation::PARK_RANGER;
    extra += waterside && _player.items().find(item_id::FISHING_ROD) >= 0 ? item_tuning::fishing_rod_finds : 0;
    _player.train(*this, skill::FORAGING, character_tuning::forage_xp);
    begin_search(index, extra);

    // Animals plan stage 4: by a farm with hens left, eggs.
    int eggs = _animals.eggs_near(feet_x, feet_y);

    if(eggs > 0)
    {
        for(int egg = 0; egg < eggs; ++egg)
        {
            item_stack found = make_stack(item_id::EGGS);
            found.extra = uint16_t(day());
            static_cast<void>(_containers.get(index).add(found));
        }

        #if KX_BENCH == 15
            BN_LOG("KXFARM foraging by the farm finds ", eggs, " eggs");
        #endif
    }
}

void game::begin_build(bool farming)
{
    if(_search.active() || _car.active() || _sleep_state || _dead || _player.ride() != ride_kind::NONE ||
       _player.swimming())
    {
        return;
    }

    _hud.set_prompt(-1, 0, "");
    _build.open(*this, farming);
}

void game::refresh_tile(int tile_x, int tile_y)
{
    _renderer.refresh_tile(tile_x, tile_y);
}

void game::bash(int tile_x, int tile_y, const bn::fixed_point& from)
{
    int pan = screen_x(from);
    int full;

    // A barricade takes the hit; when it breaks, what it covered is back.
    if(base::barricade_health(tile_x, tile_y, full) > 0)
    {
        sfx::play(sfx::id::BANG, milli(700), pan);
        noise(from, base_tuning::bang_noise);

        if(base::damage_barricade(*this, tile_x, tile_y, base_tuning::zombie_damage))
        {
            sfx::play(sfx::id::HIT_HEAVY, 1, pan);
        }

        return;
    }

    world_gfx wall = world::wall_gfx(tile_x, tile_y);
    bool side_wall;
    bool door = world::door_of(wall, side_wall) == world::door_state::CLOSED;

    if(wall != world_gfx::WINDOW_FACE && wall != world_gfx::WINDOW_TOP && ! door)
    {
        return;
    }

    // A window: a few knocks, then it smashes (loud) and stays open. A closed door (milestone 12.7) takes 12 and
    // breaks open for good.
    dent* found = nullptr;

    for(dent& value : _dents)
    {
        if(value.health > 0 && value.tile_x == tile_x && value.tile_y == tile_y)
        {
            found = &value;
        }
    }

    if(! found)
    {
        found = &_dents[_next_dent];
        _next_dent = (_next_dent + 1) % _dents.size();
        *found = dent{ int16_t(tile_x), int16_t(tile_y),
                       int16_t(door ? base_tuning::door_health : base_tuning::window_health) };
    }

    found->health = int16_t(found->health - base_tuning::zombie_damage);
    sfx::play(sfx::id::BANG, milli(door ? 700 : 500), pan);

    if(door)
    {
        noise(from, base_tuning::bang_noise);

        if(found->health <= 0)
        {
            static_cast<void>(world::set_wall_gfx(tile_x, tile_y,
                                                  world::door_gfx(world::door_state::BROKEN, side_wall)));
            refresh_tile(tile_x, tile_y);
            sfx::play(sfx::id::HIT_HEAVY, 1, pan);
            noise(from, base_tuning::door_break_noise);
        }

        #if KX_BENCH == 12
            BN_LOG("KXDOOR bash ", tile_x, ",", tile_y, " health=", found->health);
        #endif

        return;
    }

    if(found->health <= 0)
    {
        static_cast<void>(world::set_wall_gfx(tile_x, tile_y, wall == world_gfx::WINDOW_FACE ? world_gfx::BROKEN_FACE :
                                                                                           world_gfx::BROKEN_TOP));
        refresh_tile(tile_x, tile_y);
        sfx::play(sfx::id::GLASS, 1, pan);
        noise(from, base_tuning::glass_noise);
    }
}

void game::_update_flames()
{
    // A flickering flame sprite on each lit campfire in view.
    constexpr int capacity = 3;
    int tiles_x[capacity];
    int tiles_y[capacity];
    int left = _camera_controller.left() >> world::tile_shift;
    int top = _camera_controller.top() >> world::tile_shift;
    int count = (_frames % 4 == 0 || ! _flames[0]) ?
                base::lit_fires_in(left - 1, top - 1, left + 16, top + 11, _world_minute / 60, tiles_x, tiles_y,
                                   capacity) : -1;

    if(count < 0)
    {
        count = 0;

        for(bn::optional<bn::sprite_ptr>& flame : _flames)
        {
            count += flame.has_value();
        }

        for(int index = 0; index < count; ++index)
        {
            _flames[index]->set_tiles(bn::sprite_items::fx.tiles_item(), 15 + ((_frames / 10 + index) & 1));
        }

        return;
    }

    for(int index = 0; index < capacity; ++index)
    {
        if(index >= count)
        {
            _flames[index].reset();
            continue;
        }

        int x = (tiles_x[index] * world::tile_size) + (world::tile_size / 2);
        int y = (tiles_y[index] * world::tile_size) + 6;

        if(! _flames[index])
        {
            bn::sprite_ptr sprite = bn::sprite_items::fx.create_sprite(x, y, 15);
            sprite.set_camera(_camera);
            sprite.set_bg_priority(2);
            _flames[index] = bn::move(sprite);
        }
        else
        {
            _flames[index]->set_position(x, y);
        }
    }
}

void game::_update_population()
{
    // Hordes: nearby head counts become zombies, far zombies counts again (the test map keeps its fixed zombies).
    hordes::update(*this);
    _edges.update(*this);
    survivors::update(*this);

    // Safehouses: horde passes while away are rolled on approach.
    if(_frames % 60 == 0)
    {
        base::update(*this);
        farm::update(*this);
    }

    // Generators: fuel, fridges and their noise; can alarm lines rattle when crossed.
    power::update(*this, _frames);

    if(_frames % 6 == 0)
    {
        _update_can_lines();
    }
}

}

namespace kx
{

void game::_campfires_call(int hour)
{
    // Plan 12.5: a campfire is seen from far away at night. Each night hour every burning one heats its map cell
    // as a loud noise would, so hordes drift towards it when they migrate.
    if(! is_night(minute_of_day()))
    {
        return;
    }

    constexpr int capacity = 8;
    int tiles_x[capacity];
    int tiles_y[capacity];
    int last_tile = (macro::cells << macro::cell_shift) - 1;
    int count = base::lit_fires_in(0, 0, last_tile, last_tile, hour, tiles_x, tiles_y, capacity);
    int radius = tuning(light_param::CAMPFIRE_NOISE);

    for(int index = 0; index < count; ++index)
    {
        hordes::noise(tile_feet(tiles_x[index], tiles_y[index]), radius, hour);

        #if KX_BENCH == 10
            BN_LOG("KXNIGHT campfire calls at ", tiles_x[index], ",", tiles_y[index], " hour=", hour,
                   " radius=", radius);
        #endif
    }
}

bool game::zombie_hits_car(const bn::fixed_point& from)
{
    int condition = _vehicles.hit_driven(vehicle_tuning::besiege_damage);
    noise(from, vehicle_tuning::besiege_noise_tiles);
    sfx::play(sfx::id::BANG, milli(700), screen_x(from));
    shake(1, 4);

    #if KX_BENCH == 9 || KX_BENCH == 10
        BN_LOG("KXCAR besieged condition=", condition);
    #endif

    if(condition >= vehicle_tuning::pull_out_condition)
    {
        return false;
    }

    // The doors give: out of the seat, into their hands.
    static_cast<void>(_vehicles.leave(*this, true));
    toast("PULLED OUT OF THE CAR", hud_tiles::bank_danger, 120);
    return true;
}

bool game::_building_lit() const
{
    // The building's lights work while the grid has power, or with a running generator within its 10 tiles.
    int x = _player.position().x().integer() >> world::tile_shift;
    int y = (_player.position().y().integer() - 1) >> world::tile_shift;
    return timeline::power_on(day()) || power::powered(x, y, _world_minute);
}

item_stack* game::_held_flashlight()
{
    // A flashlight only shines from the off hand (milestone 12.6); in the bag it is off.
    item_stack& held = _player.items().equipped(equip_slot::OFF_HAND);
    return held.id == item_id::FLASHLIGHT ? &held : nullptr;
}

void game::_tick_flashlight()
{
    item_stack* held = _held_flashlight();

    if(! held || _vehicles.driven() >= 0 || ! light::flashlight_on(*held))
    {
        return;
    }

    item_stack& torch = *held;

    int charge = light::flashlight_charge(torch) - 1;

    if(charge <= 0)
    {
        light::set_flashlight(torch, false, 0);
        toast("FLASHLIGHT: BATTERY DEAD", hud_tiles::bank_danger, 120);
        sfx::play(sfx::id::CLICK, milli(500));
    }
    else if(minute_of_day() == tuning(light_param::DAWN_END) - _player.skills().dusk_shift_minutes())
    {
        // Daylight: off, so a forgotten flashlight keeps its battery.
        light::set_flashlight(torch, false, charge);
        toast("DAYLIGHT: FLASHLIGHT OFF", hud_tiles::bank_quiet, 90);
    }
    else
    {
        light::set_flashlight(torch, true, charge);
    }
}

void game::_update_lights(int darkness)
{
    _player_lit = false;
    light::clear_lit();

    // By day nothing lights: once the last light window is gone there is nothing to do.
    if(darkness <= 0 && ! _lights.active())
    {
        return;
    }

    light_view view;

    if(darkness > 0)
    {
        // The lit room, for the sprites (its window lights the ground): the same rectangle the renderer gets.
        if(_inside && _building_lit())
        {
            light::add_lit_rect(_building_left * world::tile_size, _building_top * world::tile_size,
                                (_building_left + _building_width) * world::tile_size,
                                (_building_top + _building_height) * world::tile_size);
        }

        // The flashlight (dark while driving: the headlights light the way), from the chest along the facing.
        const item_stack* torch = _held_flashlight();
        int tile_x = _player.position().x().integer() >> world::tile_shift;
        int tile_y = (_player.position().y().integer() - 1) >> world::tile_shift;

        if(torch && _vehicles.driven() < 0 && light::flashlight_on(*torch))
        {
            constexpr int chest_height = 10;
            view.flashlight = true;
            view.flashlight_apex = bn::fixed_point(_player.position().x(), _player.position().y() - chest_height);
            view.flashlight_angle = light::facing_angle(_player.facing_x(), _player.facing_y());
            view.flashlight_percent = _player.skills().beam_percent();
            view.holder = view.flashlight_apex;
        }

        view.headlights = _vehicles.headlights(view.headlight_apex, view.headlight_angle);

        // Lit, so seen from farther: the flashlight, the headlights of the car being driven, a burning campfire
        // within its light (4 tiles), or the lights of the building.
        _player_lit = view.flashlight || _vehicles.driven_lit() ||
                      base::fire_near(tile_x, tile_y, base_tuning::light_tiles, _world_minute / 60) ||
                      _throws.fire_near(tile_x, tile_y, base_tuning::light_tiles) || (_inside && _building_lit());

        // Fires burning from Molotovs, then the campfires in view (their flames are already found each few frames).
        view.fires = _throws.fire_centres(view.fire, light_view::max_fires);

        for(const bn::optional<bn::sprite_ptr>& flame : _flames)
        {
            if(flame && view.fires < light_view::max_fires)
            {
                view.fire[view.fires++] = flame->position();
            }
        }
    }

    _lights.update(view);
}

}

#if KX_BENCH == 10
namespace kx
{
    void game::_bench_night(int frame)
    {
        // Phases of 600 frames (10 s, long enough to look at):
        // 0     outdoors at 22:55: the flashlight on (the input turns the player round), a campfire to the west, a
        //       car beside the player with its headlights on;
        // 600   in the nearest building, its lights on (the grid has power on day 1);
        // 1200  30 days later (no power): the same building dark, the flashlight lighting it;
        // 1800  the battery nearly flat: it dies within a minute; at 2100 a new battery and on again;
        // 2400  06:59 with the flashlight on: daylight switches it off at 07:00, the roof's cut-away takes over;
        // 2700  outdoors by day; 2850 back into the building, its roof fading out.
        int tile_x = _player.position().x().integer() >> world::tile_shift;
        int tile_y = (_player.position().y().integer() - 1) >> world::tile_shift;

        if(frame == 0)
        {
            // The darkness curve: dusk, dawn, and the night with fog.
            for(int minute = (18 * 60) + 50; minute <= (20 * 60) + 10; minute += 10)
            {
                BN_LOG("KXNIGHT curve ", minute / 60, ":", minute % 60, " dark=", light::darkness(minute, false),
                       " fog=", light::darkness(minute, true));
            }

            for(int minute = (5 * 60) + 50; minute <= (7 * 60) + 10; minute += 10)
            {
                BN_LOG("KXNIGHT curve ", minute / 60, ":", minute % 60, " dark=", light::darkness(minute, false),
                       " fog=", light::darkness(minute, true));
            }

            _bench_tile_x = tile_x;
            _bench_tile_y = tile_y;

            // Stage 5 self-checks. Stitching: a deep wound, a needle, First Aid 2 or more.
            {
                survival& body = _player.body();
                body.add_wound(body_part::LEFT_ARM, wound_type::DEEP_WOUND, false, _random);
                item_stack needle = make_stack(item_id::SUTURE_NEEDLE);
                static_cast<void>(_player.items().add(needle));
                const char* label;
                item_id unskilled = body.treatment(body_part::LEFT_ARM, _player.items(), &label, false);
                BN_LOG("KXFIVE stitch without First Aid 2: ", label, " item=", int(unskilled));
                bool done = body.treat(body_part::LEFT_ARM, _player.items(), true);
                const body_wound& wound = body.wound(body_part::LEFT_ARM);
                BN_LOG("KXFIVE stitched=", done, " ", wound.stitched, " needles left=",
                       _player.items().count(item_id::SUTURE_NEEDLE), " hours=", wound.hours_left);
            }

            // Repair: a worn hunting knife with a whetstone, a worn bat with duct tape then glue.
            {
                inventory& items = _player.items();
                item_stack knife = make_stack(item_id::HUNTING_KNIFE);
                knife.amount = 5;
                item_stack bat = make_stack(item_id::BASEBALL_BAT);
                bat.amount = 4;
                item_stack whetstone = make_stack(item_id::WHETSTONE);
                item_stack tape = make_stack(item_id::DUCT_TAPE);
                item_stack glue = make_stack(item_id::GLUE);
                static_cast<void>(items.add(knife));
                static_cast<void>(items.add(bat));
                static_cast<void>(items.add(whetstone));
                static_cast<void>(items.add(tape));
                static_cast<void>(items.add(glue));
                int knife_slot = items.find(item_id::HUNTING_KNIFE);
                int bat_slot = -1;

                for(int slot = 0; slot < inventory::slots; ++slot)
                {
                    if(items.slot(slot).id == item_id::BASEBALL_BAT && items.slot(slot).amount == 4)
                    {
                        bat_slot = slot;
                    }
                }

                BN_LOG("KXFIVE knife material=", int(_player.repair_material(items.slot(knife_slot))), " bat material=",
                       int(_player.repair_material(items.slot(bat_slot))));
                static_cast<void>(_player.repair(*this, items.slot(knife_slot)));
                static_cast<void>(_player.repair(*this, items.slot(bat_slot)));
                int tape_left = items.count(item_id::DUCT_TAPE);
                static_cast<void>(_player.repair(*this, items.slot(bat_slot)));
                BN_LOG("KXFIVE knife=", items.slot(knife_slot).amount, "/", info(item_id::HUNTING_KNIFE).durability,
                       " whetstone uses=", items.slot(items.find(item_id::WHETSTONE)).amount, " bat=",
                       items.slot(bat_slot).amount, "/", info(item_id::BASEBALL_BAT).durability, " tape left=",
                       tape_left, " glue left=", items.count(item_id::GLUE));
            }

            // Traits: the new pairs' effects.
            {
                character keen;
                keen.start(occupation::UNEMPLOYED, (1u << int(trait::KEEN_HEARING)) |
                           (1u << int(trait::OUTDOORSMAN)) | (1u << int(trait::EAGLE_EYED)));
                character hard;
                hard.start(occupation::UNEMPLOYED, (1u << int(trait::HARD_OF_HEARING)) |
                           (1u << int(trait::HOMEBODY)) | (1u << int(trait::SHORT_SIGHTED)));
                BN_LOG("KXFIVE keen hearing=", keen.hearing_percent(), " stereo=", keen.stereo(), " weather=",
                       keen.weather_percent(), " beam=", keen.beam_percent(), " dusk=", keen.dusk_shift_minutes(),
                       " dark at 19:30=", light::darkness((19 * 60) + 30, false, keen.dusk_shift_minutes()));
                BN_LOG("KXFIVE hard hearing=", hard.hearing_percent(), " stereo=", hard.stereo(), " weather=",
                       hard.weather_percent(), " beam=", hard.beam_percent(), " dusk=", hard.dusk_shift_minutes(),
                       " dark at 18:30=", light::darkness((18 * 60) + 30, false, hard.dusk_shift_minutes()),
                       " traits=", trait_count);
            }

            // The off hand (milestone 12.6). Taking a flashlight up from the bag switches it on and puts the
            // two-handed bat away; taking the bat up again puts the flashlight away, switched off; a knife and a
            // flashlight are held together.
            {
                inventory& items = _player.items();
                auto log_hands = [&items](const char* label)
                {
                    int torch = items.find(item_id::FLASHLIGHT);
                    const item_stack& off = items.equipped(equip_slot::OFF_HAND);
                    BN_LOG("KXHAND ", label, ": hand=", info(items.equipped(equip_slot::HAND).id).name, " off=",
                           info(off.id).name, " on=", light::flashlight_on(off), " in bag=", torch >= 0,
                           " bag one on=", torch >= 0 && light::flashlight_on(items.slot(torch)));
                };

                item_stack torch = make_stack(item_id::FLASHLIGHT);
                static_cast<void>(items.add(torch));
                items.set_hotbar(3, item_id::FLASHLIGHT);
                log_hands("start");
                static_cast<void>(_player.use_item(*this, items.find(item_id::FLASHLIGHT)));
                log_hands("flashlight taken");
                static_cast<void>(_player.use_item(*this, items.find(item_id::BASEBALL_BAT)));
                log_hands("bat taken");
                static_cast<void>(_player.use_item(*this, items.find(item_id::HUNTING_KNIFE)));
                static_cast<void>(_player.use_item(*this, items.find(item_id::FLASHLIGHT)));
                log_hands("knife and flashlight");
                _player.switch_flashlight(*this);
                log_hands("switched off");
                _player.switch_flashlight(*this);
                log_hands("switched on");
            }

            // A campfire 3 tiles west, lit for 6 hours, as build mode lights one.
            int fire_x = tile_x - 3;
            static_cast<void>(base::add(piece_kind::CAMPFIRE, fire_x, tile_y, covered::DOOR_FACE,
                                        ((_world_minute / 60) + base_tuning::campfire_hours) & 0xFF, _world_minute));
            static_cast<void>(world::set_wall_gfx(fire_x, tile_y, world_gfx::CAMPFIRE));
            refresh_tile(fire_x, tile_y);

            // A car beside the player with its headlights on.
            int car = _vehicles.spawn(*this, vehicle_kind::CAR);
            _vehicles.set_lights(car, true);
            BN_LOG("KXNIGHT setup car=", car, " flashlight held=", _held_flashlight() != nullptr);
        }
        #if KX_DUMP
            else if(frame == 60)
            {
                // DUMP=1: the Gear tab with the cursor on the off hand (two presses left from the carried grid),
                // then its action list, logged for tools/render_hud_dump.py; then back to the game.
                _open_tab(0);
                input_frame press;
                _gear.update(*this, press);
                press.left_pressed = true;
                _gear.update(*this, press);
                _gear.update(*this, press);
                _hud.pixels().dump(9001);
                press.left_pressed = false;
                press.a_pressed = true;
                _gear.update(*this, press);
                _hud.pixels().dump(9002);
                close_menus();
            }
        #endif
        else if(frame == 600)
        {
            // The nearest building with a roof, searched outwards from the start.
            for(int radius = 1; radius <= 28; ++radius)
            {
                for(int y = tile_y - radius; y <= tile_y + radius; ++y)
                {
                    for(int x = tile_x - radius; x <= tile_x + radius; ++x)
                    {
                        int left;
                        int top;
                        int width;
                        int height;

                        if(bn::abs(x - tile_x) == radius || bn::abs(y - tile_y) == radius)
                        {
                            if(world::building_over(x, y, left, top, width, height))
                            {
                                BN_LOG("KXNIGHT building ", left, ",", top, " ", width, "x", height);
                                teleport_to_tile(left + (width / 2), top + (height / 2));
                                radius = 1000;
                                y = 1 << 20;
                                break;
                            }
                        }
                    }
                }
            }
        }
        else if(frame == 1200)
        {
            skip_days(30);
        }
        else if(frame == 1800)
        {
            // A book read by the flashlight's last minute of battery: the reading stops when it dies.
            if(item_stack* held = _held_flashlight())
            {
                light::set_flashlight(*held, true, 1);
            }
            BN_LOG("KXNIGHT reading starts: ", begin_reading(skill::CARPENTRY, 1));
        }
        else if(frame == 2100)
        {
            item_stack battery = make_stack(item_id::BATTERY);
            static_cast<void>(_player.items().add(battery));
            _player.switch_flashlight(*this);
        }
        else if(frame == 2400)
        {
            skip_to_hour(6);
            _world_minute += 59;
            _minute_frames = 0;
        }
        else if(frame == 2700)
        {
            teleport_to_tile(_bench_tile_x, _bench_tile_y);
            _bench_building_x = tile_x;
            _bench_building_y = tile_y;
        }
        else if(frame == 2850)
        {
            // Back into the building by day: the roof fades out as it always has.
            teleport_to_tile(_bench_building_x, _bench_building_y);
        }
        else if(frame == 3000)
        {
            // The Map tab with a map and a compass, a mark on another settlement (the input then moves the
            // cursor, sets two marks and closes it; the arrow then points at the nearest mark).
            item_stack map = make_stack(item_id::MAP);
            item_stack compass = make_stack(item_id::COMPASS);
            static_cast<void>(_player.items().add(map));
            static_cast<void>(_player.items().add(compass));
            int other = macro::start_settlement() == 0 ? 1 : 0;
            const macro::settlement& value = macro::settlement_at(other);
            static_cast<void>(world_map::toggle_mark((value.cell_x * 2) + value.size, (value.cell_y * 2) + value.size));
            int explored = 0;
            world_map::reveal_all();

            for(int y = 0; y < world_map::chunks; ++y)
            {
                for(int x = 0; x < world_map::chunks; ++x)
                {
                    explored += world_map::explored(x, y);
                }
            }

            BN_LOG("KXMAP explored=", explored, " marks=", world_map::mark_count(), " settlements=",
                   macro::settlement_count(), " start=", macro::start_settlement(), " ",
                   world_map::settlement_name(macro::start_settlement()), " marked=", other, " ",
                   world_map::settlement_name(other));
            _open_tab(4);
        }

        // Zombies at the car you sit in: back outside, into a battered car (30 of 100), four zombies around it
        // and a noise to bring them; the log shows their hits and the driver being dragged out.
        if(frame == 3001)
        {
            teleport_to_tile(_bench_tile_x, _bench_tile_y);
        }
        else if(frame == 3005)
        {
            int car = _vehicles.spawn(*this, vehicle_kind::CAR);

            if(car >= 0)
            {
                _vehicles.enter(*this, car);
                static_cast<void>(_vehicles.hit_driven(70));
            }

            for(int index = 0; index < 4; ++index)
            {
                int dx = (index & 1) ? 3 : -3;
                int dy = (index & 2) ? 2 : -2;
                _zombies.spawn(tile_feet(_bench_tile_x + dx, _bench_tile_y + dy), _random);
            }

            noise(_player.position(), 10);
            BN_LOG("KXCAR besiege setup car=", car, " ride=", int(_player.ride()), " condition=",
                   car >= 0 ? _vehicles.condition(car) : -1, " zombies=", _zombies.count());
        }

        if(frame > 3005 && frame % 30 == 0)
        {
            BN_LOG("KXCAR besiege f=", frame, " ride=", int(_player.ride()), " grabbed=", _player.grabbed(),
                   " zombies=", _zombies.count());
        }

        if(frame == 3001 || frame == 3060)
        {
            int x = 0;
            int y = 0;
            int arrow = -1;
            bool shown = _mark_arrow.shown(x, y, arrow);
            BN_LOG("KXMAP closed marks=", world_map::mark_count(), " arrow=", shown, " at ", x, ",", y, " frame=",
                   arrow);
        }

        if(frame % 30 == 0 || (frame > 2700 && frame < 2712) || (frame > 2850 && frame < 2862))
        {
            const item_stack* held = _held_flashlight();
            int slot = held ? 0 : -1;
            const item_stack& torch = held ? *held : _player.items().slot(0);
            bn::fixed_point front;
            int angle = 0;
            bool beam = _vehicles.headlights(front, angle);
            int fires = 0;

            for(const bn::optional<bn::sprite_ptr>& flame : _flames)
            {
                fires += flame.has_value();
            }

            BN_LOG("KXNIGHT f=", frame, " day=", day(), " time=", minute_of_day() / 60, ":", minute_of_day() % 60,
                   " dark=", light::darkness(minute_of_day(), _weather == weather_kind::FOG),
                   " weather=", int(_weather), " torch=", slot >= 0 && light::flashlight_on(torch), "/",
                   slot >= 0 ? light::flashlight_charge(torch) : -1, " facing=", _player.facing_x(), ",",
                   _player.facing_y(), " beam=", beam, " angle=", angle, " fires=", fires,
                   " inside=", _inside, " night=", _renderer.night(), " hidden=", _renderer.roof_hidden(),
                   " lit=", _renderer.room_lit(), " cutaway=", _renderer.cutaway(),
                   " roof=", _renderer.roof_level(), " player_lit=", _player_lit, " read=", _reading_light());

            // The display registers that can be read back: what the hardware really does.
            auto reg = [](unsigned address) { return int(*reinterpret_cast<volatile uint16_t*>(address)); };
            BN_LOG("KXNIGHT regs dispcnt=", reg(0x04000000), " winin=", reg(0x04000048), " winout=",
                   reg(0x0400004A), " bldcnt=", reg(0x04000050), " bldalpha=", reg(0x04000052));
        }
    }
}
#endif

#if KX_BENCH == 12
namespace kx
{
    void game::_bench_doors(int frame)
    {
        // Houses: unlocked ones with a door, and locked ones (from every settlement; the sixth for the map's mark).
        static uint16_t open_house = 0;
        static uint16_t open_house_2 = 0;
        static uint16_t locked[6] = {};
        static int locked_count = 0;
        constexpr int phase_length = 400;
        constexpr int first_phase = 60;

        auto door_of_house = [](uint16_t house, int& door_x, int& door_y, int& out_x, int& out_y, int& in_x,
                                int& in_y)
        {
            // Outside is the neighbour across the door's wall that is not roofed (both from the seed, so before the
            // chunk is generated).
            bool side_wall = false;

            if(! world::house_door(house, door_x, door_y, &side_wall))
            {
                return false;
            }

            int dx = side_wall ? 1 : 0;
            int dy = side_wall ? 0 : 1;

            if(world::under_roof(door_x + dx, door_y + dy))
            {
                dx = -dx;
                dy = -dy;
            }

            out_x = door_x + dx;
            out_y = door_y + dy;
            in_x = door_x - dx;
            in_y = door_y - dy;
            return true;
        };

        auto log_door = [this](const char* label, int door_x, int door_y)
        {
            bool side_wall;
            world::door_state state = world::door_of(world::wall_gfx(door_x, door_y), side_wall);
            BN_LOG("KXLOCK ", label, ": door ", door_x, ",", door_y, " state=", int(state), " locked=",
                   world::door_locked(door_x, door_y), " solid=", world::solid(door_x, door_y), " auto marks=",
                   world_map::auto_mark_count(), " context=", _player.context());
        };

        if(frame == 1)
        {
            uint16_t ids[64];
            kx::building kinds[64];
            int door_x;
            int door_y;

            // Houses from every settlement: two unlocked ones with doors, and up to 6 locked ones.
            int settlement = macro::start_settlement();
            int count = 0;

            for(int town = 0; town < macro::settlement_count(); ++town)
            {
                int found = world::settlement_buildings(town, ids, kinds, 64);
                count += found;

                for(int index = 0; index < found; ++index)
                {
                    bool house = kinds[index] == kx::building::SMALL_HOUSE ||
                                 kinds[index] == kx::building::LARGE_HOUSE;

                    if(! house || ! world::house_door(ids[index], door_x, door_y))
                    {
                        continue;
                    }

                    if(world::house_locked(ids[index]))
                    {
                        if(locked_count < 6)
                        {
                            locked[locked_count++] = ids[index];
                            settlement = locked_count == 1 ? town : settlement;
                        }
                    }
                    else if(! open_house)
                    {
                        open_house = ids[index];
                    }
                    else if(! open_house_2)
                    {
                        open_house_2 = ids[index];
                    }
                }
            }

            BN_LOG("KXLOCK settlement ", world_map::settlement_name(settlement), ": buildings=", count, " locked=",
                   locked_count, " open house=", open_house, " second=", open_house_2);

            // Keys found in town (at its first locked house's door): each for another locked house there.
            int tile_x = 0;
            int tile_y = 0;
            static_cast<void>(world::house_door(locked[0], tile_x, tile_y));

            for(uint32_t roll = 0; roll < 4; ++roll)
            {
                uint16_t key = world::key_for(tile_x, tile_y, roll * 7919);
                BN_LOG("KXLOCK key roll ", roll, ": house=", key, " locked=", world::house_locked(key), " town=",
                       world::house_settlement(key));
            }
        }

        if(frame < first_phase || (frame - first_phase) % phase_length != 0)
        {
            int phase = (frame - first_phase) / phase_length;

            if(frame >= first_phase && (frame - first_phase) % phase_length == 300 && phase < 8)
            {
                int door_x;
                int door_y;
                int out_x;
                int out_y;
                int in_x;
                int in_y;
                uint16_t house = phase == 0 ? open_house : phase == 7 ? open_house_2 :
                                 locked[bn::min(phase == 1 || phase == 2 ? 0 : phase - 2, locked_count - 1)];

                if(door_of_house(house, door_x, door_y, out_x, out_y, in_x, in_y))
                {
                    log_door("phase end", door_x, door_y);
                }
            }

            // Phase 7, once the chunks there are generated (a zombie in one that is not would go back to its
            // horde at once): a zombie 3 tiles out, called by a noise; A then shuts the door in its face.
            if(frame == first_phase + (7 * phase_length) + 30)
            {
                int door_x;
                int door_y;
                int out_x;
                int out_y;
                int in_x;
                int in_y;

                if(door_of_house(open_house_2, door_x, door_y, out_x, out_y, in_x, in_y))
                {
                    _zombies.spawn(tile_feet(out_x + ((out_x - door_x) * 3), out_y + ((out_y - door_y) * 3)), _random);
                    noise(_player.position(), 12);
                    BN_LOG("KXLOCK zombie outside: zombies=", _zombies.count());
                }
            }

            if(frame > first_phase + (7 * phase_length) && frame % 120 == 0)
            {
                BN_LOG("KXLOCK waiting: zombies=", _zombies.count(), " kills=", _kills);
            }

            // The end: a key to a house still locked marks it; the Map tab shows it (and a note's icon beside).
            if(frame == 4520 || frame == 4920 || frame == 4960 || frame == 5160)
            {
                _bench_wood(frame, locked[bn::min(1, locked_count - 1)]);
            }

            if(frame == 5320)
            {
                item_stack map = make_stack(item_id::MAP);
                static_cast<void>(_player.items().add(map));
                // Only the land round the player: the dump shows the fog's edge, drawn chunk by chunk (playtest 1).
                world_map::scout(_player.position().x().integer() >> world::tile_shift,
                                 _player.position().y().integer() >> world::tile_shift, 5);
                item_stack key = make_stack(item_id::HOUSE_KEY);
                key.extra = locked[locked_count - 1];
                item_taken(key);
                key.extra = locked[0];      // opened in phase 2: no mark
                item_taken(key);
                int door_x = 0;
                int door_y = 0;
                static_cast<void>(world::house_door(locked[0], door_x, door_y));
                world_map::add_auto_mark(world_map::auto_mark::NOTE, 1, door_x + 96, door_y - 64);
                BN_LOG("KXLOCK map: auto marks=", world_map::auto_mark_count());
                _open_tab(4);
            }

            return;
        }

        int phase = (frame - first_phase) / phase_length;

        if(phase >= 8 || ! locked_count)
        {
            return;
        }

        // Each phase: which house, where to stand, what to carry.
        uint16_t house = phase == 0 ? open_house : phase == 7 ? open_house_2 :
                         locked[bn::min(phase == 1 || phase == 2 ? 0 : phase - 2, locked_count - 1)];
        int door_x;
        int door_y;
        int out_x;
        int out_y;
        int in_x;
        int in_y;

        if(! door_of_house(house, door_x, door_y, out_x, out_y, in_x, in_y))
        {
            BN_LOG("KXLOCK phase ", phase, ": no door for house ", house);
            return;
        }

        inventory& items = _player.items();
        _zombies.clear();
        static_cast<void>(items.unequip(equip_slot::HAND));

        for(item_id id : { item_id::SCREWDRIVER, item_id::HOUSE_KEY, item_id::CROWBAR })
        {
            for(int slot = items.find(id); slot >= 0; slot = items.find(id))
            {
                items.slot(slot) = item_stack();
            }
        }

        bool inside = phase == 6;
        int stand_x = inside ? in_x : out_x;
        int stand_y = inside ? in_y : out_y;
        _player.teleport(bn::fixed_point((stand_x * world::tile_size) + 8, (stand_y * world::tile_size) + 12));
        _player.face(door_x - stand_x, door_y - stand_y);
        const char* names[] = { "open a door", "locked, no tools", "its key", "a crowbar", "a screwdriver",
                                "the Burglar", "from inside", "a zombie outside" };

        if(phase == 2)
        {
            item_stack key = make_stack(item_id::HOUSE_KEY);
            key.extra = house;
            item_taken(key);
            static_cast<void>(items.add(key));
        }
        else if(phase == 3)
        {
            item_stack crowbar = make_stack(item_id::CROWBAR);
            static_cast<void>(items.add(crowbar));
            static_cast<void>(_player.use_item(*this, items.find(item_id::CROWBAR)));
        }
        else if(phase == 4 || phase == 5)
        {
            item_stack screwdriver = make_stack(item_id::SCREWDRIVER);
            static_cast<void>(items.add(screwdriver));

            if(phase == 5)
            {
                _player.skills().start(occupation::BURGLAR, 0);
            }
        }
        else if(phase == 7)
        {
            // Stand inside with the door open, a zombie outside called by a noise; A shuts it in its face.
            _player.teleport(bn::fixed_point((in_x * world::tile_size) + 8, (in_y * world::tile_size) + 12));
            _player.face(door_x - in_x, door_y - in_y);
            static_cast<void>(world::set_wall_gfx(door_x, door_y, world::door_gfx(world::door_state::OPEN,
                                                                                   out_y == door_y)));
            refresh_tile(door_x, door_y);
        }

        BN_LOG("KXLOCK phase ", phase, " (", names[phase], "): house=", house, " locked=",
               world::house_locked(house), " door=", door_x, ",", door_y, " stand=", stand_x, ",", stand_y,
               " hand=", info(items.equipped(equip_slot::HAND).id).name, " job=", int(_player.skills().job()));
    }
}
#endif

#if KX_BENCH == 12
namespace kx
{
    void game::_bench_wood(int frame, uint16_t broken_house)
    {
        inventory& items = _player.items();
        _zombies.clear();
        close_menus();

        if(frame == 4520)
        {
            // A tree near here (the stash's woods): stand south of it, facing it, with an axe.
            int tile_x = _player.position().x().integer() >> world::tile_shift;
            int tile_y = (_player.position().y().integer() - 1) >> world::tile_shift;
            int tree_x = -1;
            int tree_y = -1;

            for(int radius = 1; radius < 40 && tree_x < 0; ++radius)
            {
                for(int y = tile_y - radius; y <= tile_y + radius && tree_x < 0; ++y)
                {
                    for(int x = tile_x - radius; x <= tile_x + radius && tree_x < 0; ++x)
                    {
                        if(world::tree_at(x, y) && ! world::solid(x, y + 1))
                        {
                            tree_x = x;
                            tree_y = y;
                        }
                    }
                }
            }

            item_stack axe = make_stack(item_id::AXE);
            static_cast<void>(items.add(axe));
            static_cast<void>(_player.use_item(*this, items.find(item_id::AXE)));

            if(tree_x >= 0)
            {
                _player.teleport(bn::fixed_point((tree_x * world::tile_size) + 8,
                                                 ((tree_y + 1) * world::tile_size) + 12));
                _player.face(0, -1);
            }

            BN_LOG("KXCHOP tree ", tree_x, ",", tree_y, " kind=", world::tree_at(tree_x, tree_y), " canopy=",
                   int(world::gfx_at(world::layer::CANOPY, tree_x, tree_y)), " above=",
                   int(world::gfx_at(world::layer::CANOPY, tree_x, tree_y - 1)), " axe=",
                   int(items.equipped(equip_slot::HAND).amount), " axe level=", _player.skills().level(skill::AXE));
            return;
        }

        // A Carpenter (Carpentry 3) with the materials: a wooden door in the doorway the crowbar broke, then a log
        // wall outside it; zombies' hits on each until they break.
        if(frame == 4920)
        {
            _player.skills().start(occupation::CARPENTER, 0);

            for(item_id id : { item_id::HAMMER, item_id::HINGE, item_id::DOORKNOB })
            {
                item_stack stack = make_stack(id);
                static_cast<void>(items.add(stack));
            }

            item_stack planks = make_stack(item_id::PLANKS, 4);
            item_stack nails = make_stack(item_id::NAILS, 12);
            item_stack logs = make_stack(item_id::LOGS, 2);
            static_cast<void>(items.add(planks));
            static_cast<void>(items.add(nails));
            static_cast<void>(items.add(logs));
        }
        int door_x = 0;
        int door_y = 0;
        bool side_wall = false;
        static_cast<void>(world::house_door(broken_house, door_x, door_y, &side_wall));
        int dx = side_wall ? 1 : 0;
        int dy = 1 - dx;

        if(world::under_roof(door_x + dx, door_y + dy))
        {
            dx = -dx;
            dy = -dy;
        }

        if(frame == 4920)
        {
            // Stand outside the doorway; the chunks there are generated before building (frame 4960).
            _player.teleport(bn::fixed_point(((door_x + dx) * world::tile_size) + 8,
                                             ((door_y + dy) * world::tile_size) + 12));
            return;
        }

        int wall_x = door_x + (dx * 3);
        int wall_y = door_y + (dy * 3);
        int full;

        if(frame == 5160)
        {
            // Zombies' hits until each breaks.
            int hits = 0;

            while(base::barricade_health(door_x, door_y, full) > 0 && hits < 40)
            {
                bash(door_x, door_y, _player.position());
                ++hits;
            }

            BN_LOG("KXWOOD door broke after ", hits, " hits: state=",
                   int(world::door_of(world::wall_gfx(door_x, door_y), side_wall)), " solid=",
                   world::solid(door_x, door_y));
            hits = 0;

            while(base::barricade_health(wall_x, wall_y, full) > 0 && hits < 40)
            {
                bash(wall_x, wall_y, _player.position());
                ++hits;
            }

            BN_LOG("KXWOOD log wall broke after ", hits, " hits: gfx=", int(world::wall_gfx(wall_x, wall_y)),
                   " solid=", world::solid(wall_x, wall_y));
            return;
        }

        world::door_state before = world::door_of(world::wall_gfx(door_x, door_y), side_wall);
        const char* problem = _build.place(*this, piece_kind::WOODEN_DOOR, door_x, door_y);
        BN_LOG("KXWOOD wooden door at ", door_x, ",", door_y, " (was ", int(before), "): ",
               problem ? problem : "built",
               " state=", int(world::door_of(world::wall_gfx(door_x, door_y), side_wall)), " health=",
               base::barricade_health(door_x, door_y, full), "/", full, " hinges left=", items.count(item_id::HINGE));
        problem = _build.place(*this, piece_kind::LOG_WALL, wall_x, wall_y);
        BN_LOG("KXWOOD log wall at ", wall_x, ",", wall_y, ": ", problem ? problem : "built", " gfx=",
               int(world::wall_gfx(wall_x, wall_y)), " health=", base::barricade_health(wall_x, wall_y, full), "/",
               full, " logs left=", items.count(item_id::LOGS), " solid=", world::solid(wall_x, wall_y),
               " breakable=", world::breakable(wall_x, wall_y));
    }

    void game::_bench_notes(int frame)
    {
        // A house with a gun safe (any settlement), its note, the safe; then that town's stash.
        static uint16_t house = 0;
        static int safe_x = 0;
        static int safe_y = 0;
        static int stash_x = 0;
        static int stash_y = 0;
        constexpr int safe_phase = 3720;
        constexpr int stash_phase = 4120;

        auto log_container = [this](const char* label, int tile_x, int tile_y)
        {
            int index = _containers.at(tile_x, tile_y);
            int count = 0;
            int guns = 0;

            if(index >= 0)
            {
                for(const item_stack& stack : _containers.get(index).items)
                {
                    count += ! stack.empty();
                    guns += is_gun(stack.id);
                }
            }

            BN_LOG("KXNOTE ", label, ": container=", index, " opened=",
                   index >= 0 ? int(_containers.record(index).opened) : -1, " items=", count, " guns=", guns,
                   " table=", index >= 0 ? int(_containers.record(index).table) : -1, " search open=",
                   _search.active(), " dial open=", _safe.active());
        };

        // Stand on a free tile beside a piece of furniture, facing it.
        auto stand_by = [this](int tile_x, int tile_y)
        {
            constexpr int8_t sides[4][2] = { { 0, 1 }, { 0, -1 }, { 1, 0 }, { -1, 0 } };

            for(const int8_t* side : sides)
            {
                int x = tile_x + side[0];
                int y = tile_y + side[1];

                if(! world::solid(x, y))
                {
                    _player.teleport(bn::fixed_point((x * world::tile_size) + 8, (y * world::tile_size) + 12));
                    _player.face(-side[0], -side[1]);
                    return true;
                }
            }

            return false;
        };

        if(frame == 1)
        {
            uint16_t ids[64];
            kx::building kinds[64];
            int safes = 0;
            int police = 0;

            for(int town = 0; town < macro::settlement_count(); ++town)
            {
                int count = world::settlement_buildings(town, ids, kinds, 64);

                for(int index = 0; index < count; ++index)
                {
                    int x;
                    int y;

                    if(world::house_safe(ids[index], x, y))
                    {
                        ++safes;
                        police += kinds[index] == kx::building::POLICE_STATION;

                        // The last home safe found (milestone 14: the first is now in the house of the door
                        // phases, whose zombie stops A opening anything).
                        if(world::detail::home(kinds[index]))
                        {
                            house = ids[index];
                            safe_x = x;
                            safe_y = y;
                        }
                    }
                }
            }

            BN_LOG("KXNOTE safes in the world: ", safes, " (police ", police, ") house=", house, " safe=", safe_x,
                   ",", safe_y, " code=", world::safe_code(house));
        }
        // Milestone 14: the safe can be in a crowded town now; zombies stay away while the dial is worked (the
        // phase is about the safe, not about fighting).
        if(house && frame > safe_phase - 40 && frame < safe_phase + 300)
        {
            _zombies.clear();
            _player.bench_release();
        }

        if(false)
        {
        }
        else if(frame == safe_phase && house)
        {
            _zombies.clear();
            close_menus();
            item_stack note = make_stack(item_id::NOTE_SAFE);
            note.extra = house;
            read_note(note);
            _player.teleport(bn::fixed_point((safe_x * world::tile_size) + 8, ((safe_y + 1) * world::tile_size) + 12));

            // Milestone 14: the safe may be across the map; its chunks are made at once (a scripted moment).
            world::prepare(_player.position().x().integer(), _player.position().y().integer());
        }
        else if(frame == safe_phase + 30 && house)
        {
            _zombies.clear();
            BN_LOG("KXNOTE by the safe: ", stand_by(safe_x, safe_y), " wall=", int(world::wall_gfx(safe_x, safe_y)),
                   " has note mark=", world_map::has_auto_mark(world_map::auto_mark::NOTE, house));
        }
        else if(frame == safe_phase + 60)
        {
            _safe.set_digits((world::safe_code(house) + 1) % 1000);
            log_container("dial", safe_x, safe_y);
        }
        else if(frame == safe_phase + 100)
        {
            _safe.set_digits(world::safe_code(house));
        }
        else if(frame == safe_phase + 300)
        {
            log_container("safe searched", safe_x, safe_y);
        }
        else if(frame == stash_phase && house)
        {
            _zombies.clear();
            close_menus();
            int town = world::house_settlement(house);
            int way;
            item_stack note = make_stack(item_id::NOTE_STASH);
            note.extra = uint16_t(town);
            read_note(note);
            static_cast<void>(world::stash_tile(town, stash_x, stash_y, way));
            _player.teleport(bn::fixed_point((stash_x * world::tile_size) + 8,
                                             ((stash_y + 1) * world::tile_size) + 12));
        }
        else if(frame == stash_phase + 30 && house)
        {
            _zombies.clear();
            BN_LOG("KXNOTE by the stash: ", stand_by(stash_x, stash_y), " wall=",
                   int(world::wall_gfx(stash_x, stash_y)), " stash gfx=", int(world_gfx::STASH));
        }
        else if(frame == stash_phase + 300)
        {
            log_container("stash searched", stash_x, stash_y);
        }
        else if(frame == stash_phase + 380)
        {
            // A letter too.
            item_stack letter = make_stack(item_id::NOTE_LETTER);
            letter.extra = uint16_t((2 << 8) | 0);
            read_note(letter);
        }
    }
}
#endif

#if KX_BENCH == 11
namespace kx
{
    void game::_bench_throws(int frame)
    {
        // Throwables, fire, the spear and the Craft tab (milestone 12.6 stage 5); the input route throws.
        static bn::fixed_point start;
        static int crafted = 0;
        static bool alarm_seen = false;
        static int spear_hits = 0;
        inventory& items = _player.items();
        int tile_x = _player.position().x().integer() >> world::tile_shift;
        int tile_y = (_player.position().y().integer() - 1) >> world::tile_shift;

        auto hold = [this, &items](item_id id)
        {
            int slot = items.find(id);

            if(slot >= 0)
            {
                static_cast<void>(_player.use_item(*this, slot));
            }

            BN_LOG("KXTHROW hold ", info(id).name, ": hand=", info(items.equipped(equip_slot::HAND).id).name,
                   " off=", info(items.equipped(equip_slot::OFF_HAND).id).name, " bottles in bag=",
                   items.count(item_id::EMPTY_BOTTLE));
        };

        auto count_held = [&items](item_id id)
        {
            return items.count(id) + (items.equipped(equip_slot::HAND).id == id) +
                   (items.equipped(equip_slot::OFF_HAND).id == id);
        };

        auto recipe_of = [](item_id id)
        {
            for(int index = 0; index < recipe_count; ++index)
            {
                if(recipes[index].result == id)
                {
                    return index;
                }
            }

            return -1;
        };

        if(frame == 1840)
        {
            // At night, so the fire's light shows too.
            skip_to_hour(23);
            start = _player.position();
            _zombies.clear();
            static_cast<void>(items.unequip(equip_slot::HAND));

            for(item_id id : { item_id::ALARM_CLOCK, item_id::EMPTY_BOTTLE, item_id::EMPTY_BOTTLE, item_id::MOLOTOV })
            {
                item_stack stack = make_stack(id);
                static_cast<void>(items.add(stack));
            }

            hold(item_id::ALARM_CLOCK);
        }
        else if(frame == 1890)
        {
            hold(item_id::EMPTY_BOTTLE);
        }
        else if(frame == 1915)
        {
            BN_LOG("KXTHROW after the bottle: off=", info(items.equipped(equip_slot::OFF_HAND).id).name,
                   " bottles in bag=", items.count(item_id::EMPTY_BOTTLE));
        }
        else if(frame == 1940)
        {
            constexpr int8_t zombies[4][2] = { { 6, 0 }, { 7, -1 }, { 7, 1 }, { 8, 0 } };

            for(const int8_t* offset : zombies)
            {
                _zombies.spawn(tile_feet(tile_x + offset[0], tile_y + offset[1]), _random);
            }

            hold(item_id::MOLOTOV);
        }
        else if(frame == 2400)
        {
            bn::fixed_point centre;

            if(_throws.fire_centres(&centre, 1))
            {
                _player.teleport(centre + bn::fixed_point(0, 7));
                BN_LOG("KXTHROW into the fire at ", centre.x().integer(), ",", centre.y().integer());
            }
        }
        else if(frame == 2490)
        {
            _player.teleport(start);
            BN_LOG("KXTHROW out of the fire: burns=", _throws.player_burns());
        }
        else if(frame == 2510)
        {
            BN_LOG("KXTHROW tally throws: thrown=", _throws.thrown(), " smashed=", _throws.smashed(), " rings=",
                   _throws.rings(), " fires=", _throws.fires_started(), " burn kills=", _throws.burn_kills(),
                   " zombies left=", _zombies.count(), " player burns=", _throws.player_burns());
            _bench_kills = _kills;
            _zombies.clear();
            item_stack spear = make_stack(item_id::SPEAR);
            static_cast<void>(items.add(spear));
            hold(item_id::SPEAR);
            // Close in front (inside the spear's 28 px), and one more behind it.
            _zombies.spawn(tile_feet(tile_x + 1, tile_y), _random);
            _zombies.spawn(tile_feet(tile_x + 2, tile_y), _random);
            spear_hits = _zombies.total_hits();
        }
        else if(frame == 2760)
        {
            BN_LOG("KXTHROW tally spear: hits=", _zombies.total_hits() - spear_hits, " kills=", _kills - _bench_kills,
                   " zombies left=", _zombies.count(),
                   " spear condition=", int(items.equipped(equip_slot::HAND).amount), "/",
                   int(info(item_id::SPEAR).durability), " two_handed=", two_handed(item_id::SPEAR), " off=",
                   info(items.equipped(equip_slot::OFF_HAND).id).name, " reach=",
                   tuning(weapon::SPEAR, weapon_param::REACH).integer());
        }
        else if(frame == 2780)
        {
            // The Craft tab: a Molotov (a bottle, a rag, a litre from a fuel can), then a spear (2 sticks and the
            // kitchen knife), with no heat about.
            _zombies.clear();
            static_cast<void>(items.unequip(equip_slot::OFF_HAND));

            for(item_id id : { item_id::RAG, item_id::KITCHEN_KNIFE, item_id::FUEL_CAN, item_id::EMPTY_BOTTLE })
            {
                item_stack stack = make_stack(id);
                static_cast<void>(items.add(stack));
            }

            item_stack sticks = make_stack(item_id::STICKS, 2);
            static_cast<void>(items.add(sticks));
            const char* problem_molotov;
            const char* problem_spear;
            bool molotov_ready = recipe_ready(recipe_of(item_id::MOLOTOV), problem_molotov);
            bool spear_ready = recipe_ready(recipe_of(item_id::SPEAR), problem_spear);
            BN_LOG("KXCRAFT ready: molotov=", molotov_ready, " spear=", spear_ready, " heat=", int(heat_source()),
                   " fuel litres=", carries(fuel_litres), " molotovs=", count_held(item_id::MOLOTOV), " spears=",
                   count_held(item_id::SPEAR));
            crafted = 0;
            cook(recipe_of(item_id::MOLOTOV));
        }
        else if(frame > 2780 && crafted == 0 && count_held(item_id::MOLOTOV) > 0)
        {
            crafted = 1;
            BN_LOG("KXCRAFT made a Molotov: fuel litres left=", carries(fuel_litres), " rags=",
                   items.count(item_id::RAG), " bottles=", items.count(item_id::EMPTY_BOTTLE));
            cook(recipe_of(item_id::SPEAR));
        }
        else if(frame > 2780 && crafted == 1 && count_held(item_id::SPEAR) > 1)
        {
            crafted = 2;
            BN_LOG("KXCRAFT made a spear: sticks=", items.count(item_id::STICKS), " knives=",
                   count_held(item_id::KITCHEN_KNIFE), " spears=", count_held(item_id::SPEAR));
        }

        if(_throws.alarms_ringing() > 0)
        {
            alarm_seen = true;
        }
        else if(alarm_seen)
        {
            alarm_seen = false;
            BN_LOG("KXTHROW the alarm clock stopped at f=", frame, " rings=", _throws.rings());
        }

        if(frame > 1960 && frame < 2520 && frame % 60 == 0)
        {
            BN_LOG("KXTHROW f=", frame, " fires=", _throws.fires_burning(), " zombies=", _zombies.count(),
                   " burn kills=", _throws.burn_kills(), " alarms=", _throws.alarms_ringing(), " rings=",
                   _throws.rings());
        }

        if(frame == 3320)
        {
            BN_LOG("KXTHROW end: thrown=", _throws.thrown(), " smashed=", _throws.smashed(), " rings=",
                   _throws.rings(), " fires=", _throws.fires_started(), " burn kills=", _throws.burn_kills(),
                   " player burns=", _throws.player_burns(), " crafted=", crafted, " spear level=",
                   _player.skills().level(skill::SPEAR));
        }
    }

    void game::_bench_guns(int frame)
    {
        // Each phase: the last one's tally, then a gun in hand with ammunition in the bag and zombies down the
        // road to the east (tiles from the player: near for the shotgun, far for the hunting rifle).
        struct phase
        {
            int frame;
            item_id gun_item;
            int rounds;
            int8_t zombies[5][2];
            int count;
        };

        constexpr phase phases[] = {
            { 0, item_id::PISTOL, 30, { { 6, 0 }, { 8, -1 }, { 10, 1 } }, 3 },
            { 560, item_id::SHOTGUN, 12, { { 3, 0 }, { 4, -1 }, { 4, 1 }, { 5, 0 } }, 4 },
            { 920, item_id::HUNTING_RIFLE, 10, { { 9, 0 }, { 11, 0 }, { 13, 1 } }, 3 },
            { 1280, item_id::ASSAULT_RIFLE, 30, { { 5, 0 }, { 6, -1 }, { 6, 1 }, { 7, 0 }, { 8, 0 } }, 5 },
        };

        auto tally = [this](const char* label)
        {
            BN_LOG("KXGUN tally ", label, ": fired=", _player.shots_fired(), " hit=", _player.shots_hit(),
                   " kills=", _kills - _bench_kills, " zombies left=", _zombies.count(), " rounds=",
                   _player.gun_rounds(), "/", _player.gun_rounds_carried(), " aiming xp level=",
                   _player.skills().level(skill::AIMING), " fast reloads=", _player.fast_reloads(), " jams=",
                   _player.jams(), " reloading level=", _player.skills().level(skill::RELOADING));
            _bench_kills = _kills;
        };

        for(const phase& value : phases)
        {
            if(frame != value.frame)
            {
                continue;
            }

            if(frame > 0)
            {
                tally("phase over");
            }

            inventory& items = _player.items();
            _zombies.clear();
            static_cast<void>(items.unequip(equip_slot::HAND));
            item_stack held = make_stack(value.gun_item);
            item_stack ammo = make_stack(ammo_of(gun_of(value.gun_item)), value.rounds);
            static_cast<void>(items.add(held));
            static_cast<void>(items.add(ammo));
            static_cast<void>(_player.use_item(*this, items.find(value.gun_item)));
            int tile_x = _player.position().x().integer() >> world::tile_shift;
            int tile_y = (_player.position().y().integer() - 1) >> world::tile_shift;

            for(int index = 0; index < value.count; ++index)
            {
                _zombies.spawn(tile_feet(tile_x + value.zombies[index][0], tile_y + value.zombies[index][1]),
                               _random);
            }

            BN_LOG("KXGUN phase ", info(value.gun_item).name, " hand=",
                   info(items.equipped(equip_slot::HAND).id).name, " two_handed=", two_handed(value.gun_item),
                   " loaded=", _player.gun_rounds(), " carried=", _player.gun_rounds_carried(), " zombies=",
                   _zombies.count());
        }

        if(frame == 1600)
        {
            // The active reload: a pistol with 5 rounds loaded and a box in the bag, in a Police officer's hands
            // (Reloading 2: the pistol's 48-frame bar becomes 40, the 10-frame sweet spot 14).
            tally("guns over");
            _player.skills().start(occupation::POLICE_OFFICER, 0);
            inventory& items = _player.items();
            static_cast<void>(items.unequip(equip_slot::HAND));
            item_stack pistol = make_stack(item_id::PISTOL);
            set_gun_loaded(pistol, 5);
            item_stack ammo = make_stack(item_id::AMMO_9MM, 30);
            static_cast<void>(items.add(pistol));
            static_cast<void>(items.add(ammo));
            int slot = -1;

            for(int index = 0; index < inventory::slots; ++index)
            {
                if(items.slot(index).id == item_id::PISTOL && gun_loaded(items.slot(index)) == 5)
                {
                    slot = index;
                }
            }

            static_cast<void>(_player.use_item(*this, slot));
            BN_LOG("KXGUN reload test: loaded=", _player.gun_rounds(), " carried=", _player.gun_rounds_carried(),
                   " reloading level=", _player.skills().level(skill::RELOADING));
        }

        // After the jam, the script presses A as the bar reaches the sweet spot.
        if(frame > 1600 && _player.jams() > 0 && _player.reloading() &&
                _player.reload_frame() == _player.sweet_start() + 1)
        {
            _player.reload_press(*this);
        }

        if(frame > 1600 && frame % 10 == 0 && (_player.gun_jammed_now() || _player.clearing_jam()))
        {
            BN_LOG("KXGUN f=", frame, " jammed=", _player.gun_jammed_now(), " clearing=", _player.clearing_jam(),
                   " fired=", _player.shots_fired(), " rounds=", _player.gun_rounds());
        }

        if(frame == 1800)
        {
            tally("reload over");
        }

        _bench_throws(frame);

        if(frame % 60 == 30 && _player.aiming())
        {
            BN_LOG("KXGUN aim f=", frame, " cone=", _player.aim_cone(), " locked=", _player.aim_locked(),
                   " angle=", _player.aim_angle(), " reloading=", _player.reloading(), " speed=",
                   _player.speed());
        }
    }
}
#endif

#if KX_BENCH == 13
namespace kx
{
    void game::_bench_wild(int frame)
    {
        // Phases (the input script walks): the pond with a zombie behind, drowning, the river with and without a
        // heavy load. Every 30 frames a KXSWIM line; the zombie's tiles are checked for deep water every frame.
        static bool zombie_swam = false;
        static int phase = 0;

        auto tile_x_of = [](const bn::fixed_point& position) { return position.x().integer() >> world::tile_shift; };
        auto tile_y_of = [](const bn::fixed_point& position)
        {
            return (position.y().integer() - 1) >> world::tile_shift;
        };

        if(frame == 59)
        {
            phase = 1;
            _player.teleport(tile_feet(14, 37));
            _zombies.spawn(tile_feet(14, 31), _random);
            noise(_player.position(), 20);
            BN_LOG("KXSWIM phase 1: the pond, a zombie behind");
        }
        else if(frame == 819)
        {
            phase = 2;
            // Tiring fast: swimming costs 20 exertion a frame (Exhausted 4 in about 3 in-game minutes).
            survival_tuning().adjust(int(survival_param::SWIM_EXERTION), 17);
            _zombies.clear();
            _player.teleport(tile_feet(19, 37));
            BN_LOG("KXSWIM phase 2: exhausted in the pond, then out; swim exertion=",
                   tuning(survival_param::SWIM_EXERTION));
        }
        else if(frame == 1619)
        {
            phase = 3;
            survival_tuning().adjust(int(survival_param::SWIM_EXERTION), -17);
            _player.teleport(tile_feet(55, 30));
            give_item(item_id::LOGS);
            give_item(item_id::LOGS);
            BN_LOG("KXSWIM phase 3: the river with ", _player.items().count(item_id::LOGS), " logs, weight=",
                   _player.items().weight(), " capacity=", _player.items().capacity());
        }
        else if(frame == 2019)
        {
            phase = 4;
            _player.items().remove(item_id::LOGS, _player.items().count(item_id::LOGS));
            BN_LOG("KXSWIM phase 4: the logs dropped, weight=", _player.items().weight());
        }
        else if(frame == 2649)
        {
            // Stage 4: hunting on the east field.
            phase = 5;
            _player.body() = survival();
            _player.teleport(tile_feet(35, 33));
            item_stack rifle = make_stack(item_id::HUNTING_RIFLE);
            set_gun_loaded(rifle, 5);
            _player.items().equipped(equip_slot::HAND) = rifle;

            // A practised hunter (Aiming 3: a 2-degree cone when still), so the run is the same every time.
            for(int level = 0; level < 3; ++level)
            {
                static_cast<void>(_player.skills().add_xp(skill::AIMING, 1000));
            }

            _animals.clear();
            static_cast<void>(_animals.spawn(*this, animal_kind::DEER, tile_feet(41, 33)));
            BN_LOG("KXWILD phase 5: a deer 6 tiles east, the hunting rifle from a crouch");
        }
        else if(frame == 2869)
        {
            phase = 6;
            int carcass = _animals.carcass_at(_player.position(), 400);
            BN_LOG("KXWILD phase 6: butchering; carcass=", carcass, " animals=", _animals.count(), " aiming xp level=",
                   _player.skills().level(skill::AIMING));

            if(carcass >= 0)
            {
                _player.teleport(_animals.position_of(carcass) - bn::fixed_point(12, 0));
            }

            _player.items().equipped(equip_slot::HAND) = make_stack(item_id::KITCHEN_KNIFE);
        }
        else if(frame == 3169)
        {
            phase = 7;
            _player.teleport(tile_feet(35, 30));
            _animals.spawn_flock(*this, tile_feet(43, 30));
            BN_LOG("KXWILD phase 7: a flock of ", _animals.birds(), " 8 tiles east");
        }
        else if(frame == 3391)
        {
            phase = 8;
            _player.teleport(tile_feet(35, 26));
            item_stack pistol = make_stack(item_id::PISTOL);
            set_gun_loaded(pistol, 7);
            _player.items().equipped(equip_slot::HAND) = pistol;
            _animals.clear();
            static_cast<void>(_animals.spawn(*this, animal_kind::RABBIT, tile_feet(40, 26)));
            BN_LOG("KXWILD phase 8: a rabbit 5 tiles east, the pistol");
        }
        else if(frame == 3493)
        {
            phase = 9;
            _player.teleport(tile_feet(35, 24));
            _player.items().equipped(equip_slot::HAND) = make_stack(item_id::SPEAR);
            _animals.clear();
            BN_LOG("KXWILD phase 9: the spear; meat piles so far: raw meat carried=",
                   _player.items().count(item_id::RAW_MEAT));
        }
        else if(frame == 3505)
        {
            static_cast<void>(_animals.spawn(*this, animal_kind::RABBIT,
                                             _player.position() + bn::fixed_point(22, 0)));
            BN_LOG("KXWILD a rabbit 22 px east of the crouched survivor");
        }

        if(frame == 3619)
        {
            // Stage 5: a snare built in build mode (rope and a plank), a box trap set beside it; nights pass.
            phase = 10;
            _player.teleport(tile_feet(42, 36));
            _player.items().equipped(equip_slot::HAND) = item_stack();
            give_item(item_id::ROPE);
            give_item(item_id::PLANKS);
            const char* problem = _build.place(*this, piece_kind::SNARE, 43, 36);
            static_cast<void>(traps::add(41, 36, day()));
            static_cast<void>(world::set_wall_gfx(41, 36, world_gfx::TRAP));
            int snare = traps::at(43, 36);
            int box = traps::at(41, 36);
            BN_LOG("KXWILD phase 10: snare placed=", problem ? problem : "yes", " snare=", snare, " is snare=",
                   snare >= 0 && traps::snare(snare), " gfx=", int(world::wall_gfx(43, 36)), " box=", box,
                   " solid=", world::solid(43, 36), " traps=", traps::count());
            int foraging = _player.skills().level(skill::FORAGING);

            for(int night = 1; night <= 6; ++night)
            {
                _world_minute += 24 * 60;
                bool snare_caught = snare >= 0 && traps::caught(snare, day(), foraging);
                bool box_caught = box >= 0 && traps::caught(box, day(), foraging);
                BN_LOG("KXWILD   night ", night, " day=", day(), " snare caught=", snare_caught, " box caught=",
                       box_caught, box_caught ? (traps::caught_bird(box) ? " (a bird)" : " (a rabbit)") : "");
            }

            int meat = _player.items().count(item_id::RAW_MEAT);
            _tend_at(43, 36);
            _tend_at(41, 36);
            BN_LOG("KXWILD   catches taken: raw meat ", meat, " -> ", _player.items().count(item_id::RAW_MEAT),
                   "; set again: snare caught=", traps::caught(snare, day(), foraging), " snare still a snare=",
                   traps::snare(snare));
        }
        else if(frame == 3640)
        {
            // A chest freezer: what it rolls, then a piece of meat kept in it through the power shutoff.
            phase = 11;
            static_cast<void>(world::set_wall_gfx(44, 33, world_gfx::CHEST_FREEZER));
            int index = _containers.furniture(44, 33);
            int rolled = index >= 0 ? _containers.open(index, day()) : -1;
            BN_LOG("KXWILD phase 11: freezer container=", index, " table=",
                   index >= 0 ? int(_containers.record(index).table) : -1, " rolled=", rolled, " today=", day(),
                   " power off day=", timeline::power_off_day());

            if(index >= 0)
            {
                container& freezer = _containers.get(index);

                for(const item_stack& stack : freezer.items)
                {
                    if(! stack.empty())
                    {
                        BN_LOG("KXWILD   rolled ", info(stack.id).name, " extra=", stack.extra);
                    }
                }

                freezer.items[container::capacity - 1] = make_stack(item_id::RAW_MEAT);
                freezer.items[container::capacity - 1].extra = uint16_t(day());
                int stored = day();

                for(int passed = 1; passed <= 16; ++passed)
                {
                    _world_minute += 24 * 60;
                    _freeze_day();
                    const item_stack& meat = freezer.items[container::capacity - 1];

                    if(passed % 2 == 0 || day() == timeline::power_off_day())
                    {
                        BN_LOG("KXWILD   day ", day(), " power=", timeline::power_on(day()), " meat stored day ",
                               stored, " freshness day ", meat.extra, " age ", food_age_days(meat, day()), " ",
                               freshness_name(meat, day()));
                    }
                }
            }
        }
        else if(frame == 3660)
        {
            // A well (on the test map there are none: one is put here) under the survivor.
            phase = 12;
            static_cast<void>(world::set_wall_gfx(45, 39, world_gfx::WELL));
            _player.teleport(tile_feet(45, 40));
            BN_LOG("KXWILD phase 12: a well; solid=", world::solid(45, 39), " water=",
                   _player.body().water_percent(), " source=", water_source());
        }
        else if(frame == 3720)
        {
            BN_LOG("KXWILD   after A at the well: water=", _player.body().water_percent(), " context=",
                   _player.context());
        }

        if(frame == 3779)
        {
            // Stage 6: the sandbox's effects (fast zombies, many, rare loot, month 2, 90-minute days).
            phase = 13;
            sandbox::settings chosen;
            chosen.values[0] = 2;
            chosen.values[1] = 2;
            chosen.values[2] = 0;
            chosen.values[3] = 2;
            chosen.values[4] = 2;
            sandbox::set(chosen);
            _zombies.clear();
            int sprinters = 0;
            int fast = 0;

            for(int index = 0; index < 30; ++index)
            {
                _zombies.spawn(tile_feet(2 + (index % 10), 2 + (index / 10)), _random);
            }

            for(int index = 0; index < _zombies.count(); ++index)
            {
                sprinters += _zombies.speed(index) == tuning(zombie_param::SPRINTER_SPEED);
                fast += _zombies.speed(index) == tuning(zombie_param::FAST_SPEED);
            }

            BN_LOG("KXWILD phase 13: sandbox frames/minute=", frames_per_minute(), " water off=",
                   timeline::water_off_day(), " power off=", timeline::power_off_day(), " loot percent=",
                   sandbox::tuning::loot_percent[sandbox::value(sandbox::setting::LOOT)], " population percent=",
                   sandbox::tuning::population_percent[sandbox::value(sandbox::setting::ZOMBIES)], " of ",
                   _zombies.count(), " zombies: sprinters=", sprinters, " fast=", fast);
            _zombies.clear();
        }
        else if(frame == 3780)
        {
            // A Smoker 8 hours and more without a cigarette, then a cigarette lit with the lighter.
            _player.skills().start(occupation::UNEMPLOYED, 1u << int(trait::SMOKER));
            _player.body() = survival();
            survival_input calm;
            calm.smoker = true;

            for(int minute = 0; minute < 8 * 60 + 90; ++minute)
            {
                _player.body().tick_minute(calm, _random);
            }

            const survival& body = _player.body();
            BN_LOG("KXWILD   the Smoker after ", body.minutes_since_smoke(), " min: craving=", body.craving(),
                   " unhappy=", body.level(moodle::UNHAPPY), " panic=", body.level(moodle::PANIC));
            _player.items().remove(item_id::MATCHES, _player.items().count(item_id::MATCHES));
            give_item(item_id::CIGARETTES);
            give_item(item_id::LIGHTER);
            int slot = _player.items().find(item_id::CIGARETTES);
            bool started = slot >= 0 && _player.use_item(*this, slot);
            BN_LOG("KXWILD   lighting up: started=", started, " action=", int(_player.action()));
        }
        else if(frame == 3990)
        {
            const survival& body = _player.body();
            int lighter = _player.items().find(item_id::LIGHTER);
            BN_LOG("KXWILD   after the cigarette: cigarettes=", _player.items().count(item_id::CIGARETTES),
                   " lighter uses=", lighter >= 0 ? int(_player.items().slot(lighter).amount) : -1, " craving=",
                   body.craving(), " unhappy=", body.level(moodle::UNHAPPY), " panic=", body.level(moodle::PANIC));
        }
        else if(frame == 4000)
        {
            // A campfire lit with the lighter (no matches carried).
            give_item(item_id::STONES);
            give_item(item_id::STICKS);
            int x = (_player.position().x().integer() >> world::tile_shift) + 1;
            int y = (_player.position().y().integer() - 1) >> world::tile_shift;
            const char* problem = _build.place(*this, piece_kind::CAMPFIRE, x, y);
            int lighter = _player.items().find(item_id::LIGHTER);
            BN_LOG("KXWILD   campfire with the lighter: ", problem ? problem : "lit", " lighter uses=",
                   lighter >= 0 ? int(_player.items().slot(lighter).amount) : -1, " fire starter=",
                   _player.items().fire_starter());
        }
        else if(frame == 4019)
        {
            // Stage 7: birds scattered by a shot rather than by walking up to them.
            phase = 14;
            _player.teleport(tile_feet(35, 30));
            item_stack pistol = make_stack(item_id::PISTOL);
            set_gun_loaded(pistol, 7);
            _player.items().equipped(equip_slot::HAND) = pistol;
            _animals.spawn_flock(*this, tile_feet(43, 30));
            BN_LOG("KXWILD phase 14: a flock of ", _animals.birds(), " 8 tiles east; a pistol shot");
        }
        else if(frame == 4098)
        {
            // Tired 4: worn out by hours of effort (minutes ticked here), then the game's next minute puts the
            // survivor to sleep where they stand.
            phase = 15;
            survival_input worn;
            worn.exertion = 100000;

            while(_player.body().level(moodle::TIRED) < 4)
            {
                _player.body().tick_minute(worn, _random);
            }

            BN_LOG("KXWILD phase 15: tired=", _player.body().level(moodle::TIRED), " fatigue=",
                   _player.body().fatigue_percent(), " minute=", _world_minute, " must sleep=",
                   _player.body().must_pass_out());
        }
        else if(frame == 4300)
        {
            BN_LOG("KXWILD   after the forced sleep: tired=", _player.body().level(moodle::TIRED), " fatigue=",
                   _player.body().fatigue_percent(), " minute=", _world_minute, " forced=", _forced_sleep,
                   " sleeping=", _sleep_state);
        }
        else if(frame == 4400)
        {
            give_item(item_id::MAP);
            _open_tab(4);
            BN_LOG("KXWILD   the Map tab open with the sandbox settings in its margin");
        }

        if(phase == 15 && frame > 4098 && frame < 4300 && frame % 10 == 0)
        {
            BN_LOG("KXWILD   f=", frame, " sleep state=", _sleep_state, " forced=", _forced_sleep, " tired=",
                   _player.body().level(moodle::TIRED), " minute=", _world_minute);
        }

        if(phase >= 5 && frame % 30 == 0)
        {
            const player& hero = _player;
            BN_LOG("KXWILD f=", frame, " phase=", phase, " survivor=", hero.position().x().integer(), ",",
                   hero.position().y().integer(), " crouched=", hero.crouched(), " aiming=", hero.aiming(),
                   " locked=", hero.aim_locked(), " animals=", _animals.count(), " birds=", _animals.birds(),
                   " flying=", _animals.flock_flying(), " kills listed=", _animals.kills_listed(), " context=",
                   hero.context(), " work=", int(_door_work), " work frame=", _door_work_frame, " hand=",
                   info(hero.items().equipped(equip_slot::HAND).id).name, " action=", int(hero.action()));

            for(int index = 0; index < _animals.count(); ++index)
            {
                BN_LOG("KXWILD   ", _animals.kind_of(index) == animal_kind::DEER ? "deer" : "rabbit", " at ",
                       _animals.position(index).x().integer(), ",", _animals.position(index).y().integer(),
                       " state=", _animals.state_of(index), " health=", _animals.health_of(index));
            }
        }

        if(phase >= 5)
        {
            return;
        }

        if(phase == 0)
        {
            return;
        }

        for(int index = 0; index < _zombies.count(); ++index)
        {
            const bn::fixed_point& at = _zombies.position(index);

            if(world::terrain_at(tile_x_of(at), tile_y_of(at)) == terrain::DEEP_WATER && ! zombie_swam)
            {
                zombie_swam = true;
                BN_LOG("KXSWIM FAIL a zombie in deep water at ", tile_x_of(at), ",", tile_y_of(at));
            }
        }

        if(frame % 30 == 0)
        {
            const survival& body = _player.body();
            const bn::fixed_point& at = _player.position();
            int zombie_x = -1;
            int zombie_y = -1;

            if(_zombies.count())
            {
                zombie_x = tile_x_of(_zombies.position(0));
                zombie_y = tile_y_of(_zombies.position(0));
            }

            BN_LOG("KXSWIM f=", frame, " phase=", phase, " tile=", tile_x_of(at), ",", tile_y_of(at), " swim=",
                   _player.swimming(), " speed=", _player.speed(), " endurance=", body.endurance_percent(), " exh=",
                   body.level(moodle::EXHAUSTED), " wet=", body.level(moodle::WET), " heavy=",
                   body.level(moodle::HEAVY), " health=", body.health(), " dead=", _dead, " zombie=", zombie_x, ",",
                   zombie_y, " zombie_swam=", zombie_swam);
        }
    }
}
#endif

#if KX_BENCH == 16
namespace kx
{
    void game::_bench_animal_census(int frame)
    {
        // Animals plan stage 7. Phases: a wild cell (woods or forest) by day (10:00) and by night (22:00), a farm by
        // day and by night, 1200 frames each, with the game's own spawning and the cells' own zombies; then two worst
        // cases of 600 frames: the farm by day with its animals, two deer, crows on eight corpses and a horde of 40;
        // the wild cell at night with a wolf pack, a boar, two deer and a horde. Each phase logs its counts, the
        // sprites, sprite tiles and palette colours in use, and its frame times (the first 60 frames after the move
        // left out: the world streams in).
        constexpr int natural_frames = 1200;
        constexpr int stress_frames = 600;
        constexpr int settle_frames = 60;
        constexpr const char* phase_names[] = { "wild by day", "wild by night", "farm by day", "farm by night",
                                                "worst case: farm by day", "worst case: wild at night" };
        constexpr const char* kind_names[] = { "deer", "rabbits", "boars", "wolves", "hens", "goats" };

        struct phase_stats
        {
            int frames = 0;
            int ticks = 0;
            int max_ticks = 0;
            int sprites = 0;
            int tiles = 0;
            int colours = 0;
            int zombies = 0;
            int animals = 0;
            int birds = 0;
            int kinds[6] = {};
            bool crows = false;
        };

        static int phase = -1;
        static int phase_start = 0;
        static int wild_x = -1;
        static int wild_y = -1;
        static int farm_x = -1;
        static int farm_y = -1;
        static phase_stats stats;

        auto set_clock = [this](int hour)
        {
            _world_minute = ((_world_minute / (24 * 60)) * (24 * 60)) + (hour * 60);
        };

        if(frame == 1)
        {
            // The nearest wild cell of woods or forest, and the nearest farm with a yard, from the start.
            int start_x = (_player.position().x().integer() >> world::tile_shift) >> macro::cell_shift;
            int start_y = ((_player.position().y().integer() - 1) >> world::tile_shift) >> macro::cell_shift;

            for(int radius = 1; radius < macro::cells && (wild_x < 0 || farm_x < 0); ++radius)
            {
                for(int cell_y = start_y - radius; cell_y <= start_y + radius; ++cell_y)
                {
                    for(int cell_x = start_x - radius; cell_x <= start_x + radius; ++cell_x)
                    {
                        if(cell_x < 1 || cell_y < 1 || cell_x >= macro::cells - 1 || cell_y >= macro::cells - 1 ||
                           (bn::abs(cell_x - start_x) != radius && bn::abs(cell_y - start_y) != radius))
                        {
                            continue;
                        }

                        macro::land land = macro::land_at(cell_x, cell_y);

                        if(wild_x < 0 && macro::zone_at(cell_x, cell_y) == macro::zone::WILDERNESS &&
                           (land == macro::land::FOREST || land == macro::land::WOODS))
                        {
                            wild_x = (cell_x * macro::cell_tiles) + (macro::cell_tiles / 2);
                            wild_y = (cell_y * macro::cell_tiles) + (macro::cell_tiles / 2);
                        }

                        int yard_x;
                        int yard_y;

                        if(farm_x < 0 && world::farm_yard(cell_x, cell_y, yard_x, yard_y))
                        {
                            farm_x = yard_x;
                            farm_y = yard_y + 4;
                        }
                    }
                }
            }

            BN_LOG("KXCENSUS the wild at ", wild_x, ",", wild_y, " (cell ", wild_x >> macro::cell_shift, ",",
                   wild_y >> macro::cell_shift, "), the farm by ", farm_x, ",", farm_y, " (cell ",
                   farm_x >> macro::cell_shift, ",", farm_y >> macro::cell_shift, ")");
        }

        int next = frame < 30 ? -1 : frame < 30 + (4 * natural_frames) ? (frame - 30) / natural_frames :
                   frame < 30 + (4 * natural_frames) + (2 * stress_frames) ?
                   4 + ((frame - 30 - (4 * natural_frames)) / stress_frames) : 6;

        if(next != phase)
        {
            // The phase that ended.
            if(phase >= 0 && stats.frames > 0)
            {
                int per_frame = bn::timers::ticks_per_frame();
                BN_LOG("KXCENSUS ", phase_names[phase], ": frame avg ", (stats.ticks / stats.frames) * 1000 / per_frame,
                       " max ", stats.max_ticks * 1000 / per_frame, " per mille; sprites ", stats.sprites, "/128, tiles ",
                       stats.tiles, ", colours ", stats.colours, "; zombies ", stats.zombies, ", animals ", stats.animals,
                       ", birds ", stats.birds, stats.crows ? " (crows)" : "");
                BN_LOG("KXCENSUS   most at once: ", kind_names[0], " ", stats.kinds[0], ", ", kind_names[1], " ",
                       stats.kinds[1], ", ", kind_names[2], " ", stats.kinds[2], ", ", kind_names[3], " ",
                       stats.kinds[3], ", ", kind_names[4], " ", stats.kinds[4], ", ", kind_names[5], " ",
                       stats.kinds[5]);
            }

            phase = next;
            phase_start = frame;
            stats = phase_stats();

            if(phase >= 0 && phase < 6)
            {
                bool wild = phase == 0 || phase == 1 || phase == 5;
                bool day = phase == 0 || phase == 2 || phase == 4;
                set_clock(day ? 10 : 22);
                _animals.clear();
                teleport_to_tile(wild ? wild_x : farm_x, wild ? wild_y : farm_y);
                _player.body() = survival();

                if(phase < 4)
                {
                    respawn_zombies();
                }
                else
                {
                    spawn_horde();
                    bn::fixed_point feet = _player.position();

                    if(phase == 4)
                    {
                        for(int tick = 0; tick < 4; ++tick)
                        {
                            _animals.spawn_farms_now(*this);
                        }

                        for(int index = 0; index < 8; ++index)
                        {
                            bn::fixed_point at = feet + bn::fixed_point(96 + ((index % 4) * 12), -24 + ((index / 4) * 20));
                            add_decal(at, world_decal::CORPSE);
                            static_cast<void>(add_corpse(at, loot_table::ZOMBIE_CORPSE, index & 3));
                        }

                        static_cast<void>(_animals.spawn_crows(*this, feet + bn::fixed_point(112, -14)));
                        static_cast<void>(_animals.spawn(*this, animal_kind::DEER, feet + bn::fixed_point(-110, 40)));
                        static_cast<void>(_animals.spawn(*this, animal_kind::DEER, feet + bn::fixed_point(-90, 60)));
                    }
                    else
                    {
                        static_cast<void>(_animals.spawn_pack(*this, feet + bn::fixed_point(100, 0), 3));
                        static_cast<void>(_animals.spawn(*this, animal_kind::BOAR, feet + bn::fixed_point(-100, 30)));
                        static_cast<void>(_animals.spawn(*this, animal_kind::DEER, feet + bn::fixed_point(0, 70)));
                        static_cast<void>(_animals.spawn(*this, animal_kind::DEER, feet + bn::fixed_point(30, -70)));
                    }
                }

                BN_LOG("KXCENSUS phase ", phase, ": ", phase_names[phase], " at ",
                       _player.position().x().integer() >> world::tile_shift, ",",
                       (_player.position().y().integer() - 1) >> world::tile_shift, " zombies ", _zombies.count(),
                       " animals ", _animals.count());
            }
        }

        if(phase < 0 || phase >= 6 || frame - phase_start < settle_frames)
        {
            return;
        }

        int ticks = bn::core::last_cpu_ticks();
        ++stats.frames;
        stats.ticks += ticks;
        stats.max_ticks = bn::max(stats.max_ticks, ticks);
        stats.sprites = bn::max(stats.sprites, bn::sprites::used_items_count());
        stats.tiles = bn::max(stats.tiles, bn::sprite_tiles::used_tiles_count());
        stats.colours = bn::max(stats.colours, bn::sprite_palettes::used_colors_count());
        stats.zombies = bn::max(stats.zombies, _zombies.count());
        stats.animals = bn::max(stats.animals, _animals.count());
        stats.birds = bn::max(stats.birds, _animals.birds());
        stats.crows |= _animals.crows();
        int kinds[6] = {};

        for(int index = 0; index < _animals.count(); ++index)
        {
            ++kinds[int(_animals.kind_of(index))];
        }

        for(int kind = 0; kind < 6; ++kind)
        {
            stats.kinds[kind] = bn::max(stats.kinds[kind], kinds[kind]);
        }

        if((frame - phase_start) % 200 == 0)
        {
            BN_LOG("KXCENSUS   f=", frame, " animals ", _animals.count(), " (deer ", kinds[0], " rabbits ", kinds[1],
                   " boars ", kinds[2], " wolves ", kinds[3], " hens ", kinds[4], " goats ", kinds[5], ") birds ",
                   _animals.birds(), " crows ", _animals.crows(), " hunting ", _animals.pack_hunting(), " zombies ",
                   _zombies.count(), " sprites ", bn::sprites::used_items_count(), " frame ",
                   ticks * 1000 / bn::timers::ticks_per_frame());
        }
    }
}
#endif

#if KX_BENCH == 15
namespace kx
{
    void game::_bench_animals(int frame)
    {
        // Phases: a boar 5 tiles off walks away (1); one 24 px off winds up and charges (2); one 6 tiles off is shot
        // once, charges, and the survivor steps aside (3); two more shots (4); butchered with a knife (5).
        static int phase = 0;
        static bool dodged = false;
        bn::fixed_point chest = _player.position() - bn::fixed_point(0, animal_tuning::body_height);

        // Playtest 3: the wolves traced every frame while they circle and dart (from 1500; the survivor stands, then
        // walks east and south from 1750).
        if(frame >= 1500 && frame < 2000)
        {
            _animals.trace_wolves(frame);
        }

        if(frame == 30)
        {
            phase = 1;
            _player.teleport(tile_feet(35, 33));
            _animals.clear();
            static_cast<void>(_animals.spawn(*this, animal_kind::BOAR, tile_feet(40, 33)));
            BN_LOG("KXBOAR phase 1: a boar 5 tiles east (notice ", animal_tuning::boar_notice, " tiles)");
        }
        else if(frame == 200)
        {
            phase = 2;
            _player.body() = survival();
            _animals.clear();
            static_cast<void>(_animals.spawn(*this, animal_kind::BOAR, _player.position() + bn::fixed_point(24, 0)));
            BN_LOG("KXBOAR phase 2: a boar 24 px east");
        }
        else if(frame == 500)
        {
            phase = 3;
            _player.body() = survival();
            _player.teleport(tile_feet(35, 33));
            _animals.clear();
            static_cast<void>(_animals.spawn(*this, animal_kind::BOAR, tile_feet(41, 33)));
            chest = _player.position() - bn::fixed_point(0, animal_tuning::body_height);
            bool hit = _animals.shoot(*this, chest, bn::fixed_point(1, 0), 200, gun::HUNTING_RIFLE);
            BN_LOG("KXBOAR phase 3: a boar 6 tiles east, one rifle shot: hit=", hit);
        }
        else if(frame == 800)
        {
            phase = 4;

            for(int index = 0; index < _animals.count(); ++index)
            {
                _player.teleport(_animals.position(index) - bn::fixed_point(64, 0));
            }

            chest = _player.position() - bn::fixed_point(0, animal_tuning::body_height);
            bool first = _animals.shoot(*this, chest, bn::fixed_point(1, 0), 200, gun::HUNTING_RIFLE);
            bool second = _animals.shoot(*this, chest, bn::fixed_point(1, 0), 200, gun::HUNTING_RIFLE);
            BN_LOG("KXBOAR phase 4: two more rifle shots from 64 px: ", first, " ", second);
        }
        else if(frame == 1000)
        {
            phase = 5;
            int carcass = _animals.carcass_at(_player.position(), 400);
            BN_LOG("KXBOAR phase 5: butchering; carcass=", carcass, " kind boar=",
                   carcass >= 0 && _animals.kind_of_id(carcass) == animal_kind::BOAR);

            if(carcass >= 0)
            {
                _player.teleport(_animals.position_of(carcass) - bn::fixed_point(12, 0));
            }

            _player.items().equipped(equip_slot::HAND) = make_stack(item_id::KITCHEN_KNIFE);
        }
        else if(frame == 1390)
        {
            BN_LOG("KXBOAR result: raw meat=", _player.items().count(item_id::RAW_MEAT), " hide=",
                   _player.items().count(item_id::HIDE), " animals=", _animals.count());
        }
        else if(frame == 1400)
        {
            // Stage 3: night; a pack 8 tiles east, beyond its notice.
            phase = 6;
            _world_minute = ((_world_minute / (24 * 60)) * (24 * 60)) + (22 * 60);
            _player.body() = survival();
            _player.teleport(tile_feet(35, 33));
            _animals.clear();
            static_cast<void>(_animals.spawn_pack(*this, tile_feet(43, 33), 3));
            BN_LOG("KXWOLF phase 6: 22:00, a pack 8 tiles east (notice ", animal_tuning::wolf_notice, " tiles)");
        }
        else if(frame == 1500)
        {
            phase = 7;
            bn::fixed_point centre = tile_feet(43, 33);

            for(int index = 0; index < _animals.count(); ++index)
            {
                centre = _animals.position(index);
            }

            _player.teleport(centre - bn::fixed_point(80, 0));
            BN_LOG("KXWOLF phase 7: the survivor 5 tiles from a wolf");
        }
        else if(frame == 2000)
        {
            phase = 8;
            int target = -1;
            bn::fixed best = 32767;

            for(int index = 0; index < _animals.count(); ++index)
            {
                bn::fixed distance = approx_length(_animals.position(index) - _player.position());

                if(_animals.kind_of(index) == animal_kind::WOLF && _animals.state_of(index) != 4 && distance < best)
                {
                    target = index;
                    best = distance;
                }
            }

            if(target >= 0)
            {
                // Lined up with it, 48 px west, so the shot is straight.
                _player.teleport(_animals.position(target) - bn::fixed_point(48, 0));
                chest = _player.position() - bn::fixed_point(0, animal_tuning::body_height);
                bn::fixed_point direction(1, 0);
                bool first = _animals.shoot(*this, chest, direction, 200, gun::HUNTING_RIFLE);
                bool second = _animals.shoot(*this, chest, direction, 200, gun::HUNTING_RIFLE);
                BN_LOG("KXWOLF phase 8: two rifle shots at the nearest wolf (", best.integer(), " px): ", first, " ",
                       second);
            }
        }
        else if(frame == 2300)
        {
            // A campfire beside the survivor, and a pack 5 tiles off.
            phase = 9;
            _player.body() = survival();
            _player.teleport(tile_feet(35, 36));
            _animals.clear();
            int tile_x = 36;
            int tile_y = 36;
            static_cast<void>(base::add(piece_kind::CAMPFIRE, tile_x, tile_y, covered::DOOR_FACE,
                                        ((_world_minute / 60) + base_tuning::campfire_hours) & 0xFF, _world_minute));
            static_cast<void>(world::set_wall_gfx(tile_x, tile_y, world_gfx::CAMPFIRE));
            refresh_tile(tile_x, tile_y);
            static_cast<void>(_animals.spawn_pack(*this, tile_feet(40, 36), 3));
            BN_LOG("KXWOLF phase 9: a campfire 1 tile east, a pack 5 tiles east; fire near=",
                   base::fire_near(35, 36, animal_tuning::wolf_fire_tiles, _world_minute / 60));
        }
        else if(frame == 2750)
        {
            // A lone wolf killed with an axe, then butchered with the knife.
            phase = 10;
            _player.teleport(tile_feet(35, 30));
            _animals.clear();
            static_cast<void>(_animals.spawn(*this, animal_kind::WOLF, _player.position() + bn::fixed_point(14, 0)));

            for(int hit = 0; hit < 3 && _animals.carcass_at(_player.position(), 40) < 0; ++hit)
            {
                static_cast<void>(_animals.strike(*this, _player.position(), bn::fixed_point(1, 0), 30, 360,
                                                  weapon::AXE));
            }

            int carcass = _animals.carcass_at(_player.position(), 40);
            BN_LOG("KXWOLF phase 10: a wolf killed with an axe; carcass=", carcass);

            if(carcass >= 0)
            {
                _player.teleport(_animals.position_of(carcass) - bn::fixed_point(12, 0));
            }

            _player.items().equipped(equip_slot::HAND) = make_stack(item_id::KITCHEN_KNIFE);
        }
        else if(frame == 3090)
        {
            BN_LOG("KXWOLF result: raw meat=", _player.items().count(item_id::RAW_MEAT), " (6 from the boar) hide=",
                   _player.items().count(item_id::HIDE), " animals=", _animals.count());
        }
        else if(frame == 3110)
        {
            // Stage 4: the next morning, a farm (as if macro cell 0,0 had its barn door at 35,30): 3 hens, 2 goats.
            phase = 11;
            _world_minute = (((_world_minute / (24 * 60)) + 1) * (24 * 60)) + (9 * 60);
            _player.body() = survival();
            _player.teleport(tile_feet(35, 40));
            _animals.clear();
            _animals.bench_farm(*this, 0, 0, 35, 30, 3, 2);
        }
        else if(frame == 3150)
        {
            // Standing 20 px from a hen: it flaps off.
            for(int index = 0; index < _animals.count(); ++index)
            {
                if(_animals.kind_of(index) == animal_kind::HEN)
                {
                    _player.teleport(_animals.position(index) - bn::fixed_point(20, 0));
                    _player.face(1, 0);
                    BN_LOG("KXFARM the survivor stands 20 px from a hen");
                    break;
                }
            }
        }
        else if(frame == 3200 || frame == 3250 || frame == 3300)
        {
            // Beside a hen (3200), then a goat (3250, and 3300 a day later), facing it: the prompt, then A.
            bool hen = frame == 3200;

            if(frame == 3250)
            {
                give_item(item_id::EMPTY_BOTTLE);
                give_item(item_id::EMPTY_BOTTLE);
            }

            if(frame == 3300)
            {
                _world_minute += 24 * 60;
            }

            for(int index = 0; index < _animals.count(); ++index)
            {
                if(_animals.kind_of(index) == (hen ? animal_kind::HEN : animal_kind::GOAT) &&
                   _animals.state_of(index) != 4)
                {
                    _player.teleport(_animals.position(index) - bn::fixed_point(12, 0));
                    _player.face(1, 0);
                    _bench_target = _animals.position(index);
                    break;
                }
            }
        }
        else if(frame == 3201 || frame == 3251 || frame == 3301)
        {
            bool hen = frame == 3201;
            int context = _player.context();
            BN_LOG("KXFARM day ", day(), ": beside a ", hen ? "hen" : "goat", ", context=", context, " (",
                   hen ? player::catch_context : player::milk_context, " expected)");

            if(context == player::catch_context)
            {
                catch_hen();
            }
            else if(context == player::milk_context)
            {
                milk_goat();
            }

            BN_LOG("KXFARM   raw meat=", _player.items().count(item_id::RAW_MEAT), " milk=",
                   _player.items().count(item_id::MILK), " bottles=", _player.items().count(item_id::EMPTY_BOTTLE),
                   " hens lost=", _animals.farm_lost(0, 0, false), " animals=", _animals.count());
        }
        else if(frame == 3252)
        {
            BN_LOG("KXFARM   the same goat again today: ", _animals.goat_to_milk(*this, _bench_target, 24) >= 0 ?
                   "milkable" : "not milkable", " context=", _player.context());
        }
        else if(frame == 3398)
        {
            // A goat killed with an axe, then butchered with the knife (the input holds A).
            phase = 12;

            for(int index = 0; index < _animals.count(); ++index)
            {
                if(_animals.kind_of(index) == animal_kind::GOAT && _animals.state_of(index) != 4)
                {
                    _player.teleport(_animals.position(index) - bn::fixed_point(14, 0));
                    break;
                }
            }

            for(int hit = 0; hit < 3 && _animals.carcass_at(_player.position(), 40) < 0; ++hit)
            {
                static_cast<void>(_animals.strike(*this, _player.position(), bn::fixed_point(1, 0), 30, 360,
                                                  weapon::AXE));
            }

            int carcass = _animals.carcass_at(_player.position(), 40);
            BN_LOG("KXFARM a goat killed with an axe; carcass=", carcass, " goats lost=", _animals.farm_lost(0, 0, true));

            if(carcass >= 0)
            {
                _player.teleport(_animals.position_of(carcass) - bn::fixed_point(12, 0));
            }

            _player.items().equipped(equip_slot::HAND) = make_stack(item_id::KITCHEN_KNIFE);
        }
        else if(frame == 3700)
        {
            // A hen killed with the knife: its meat on the ground.
            for(int index = 0; index < _animals.count(); ++index)
            {
                if(_animals.kind_of(index) == animal_kind::HEN && _animals.state_of(index) != 4)
                {
                    _player.teleport(_animals.position(index) - bn::fixed_point(10, 0));
                    break;
                }
            }

            bool hit = _animals.strike(*this, _player.position(), bn::fixed_point(1, 0), 30, 360, weapon::KNIFE);
            BN_LOG("KXFARM after butchering: raw meat=", _player.items().count(item_id::RAW_MEAT), " hide=",
                   _player.items().count(item_id::HIDE), "; a hen struck: ", hit, " hens lost=",
                   _animals.farm_lost(0, 0, false), " goats lost=", _animals.farm_lost(0, 0, true));
        }
        else if(frame == 3810)
        {
            // Stage 5: the search screen closed; two zombie corpses on the field, a zombie 6 tiles past them.
            phase = 13;
            _search.close(*this);
            _player.teleport(tile_feet(37, 26));
            _animals.clear();
            bn::fixed_point first = tile_feet(43, 26);
            bn::fixed_point second = tile_feet(44, 27);
            add_decal(first, world_decal::CORPSE);
            add_decal(second, world_decal::CORPSE);
            static_cast<void>(add_corpse(first, loot_table::ZOMBIE_CORPSE, 0));
            static_cast<void>(add_corpse(second, loot_table::ZOMBIE_CORPSE, 2));
            _zombies.spawn(tile_feet(49, 26), _random);
            bn::fixed_point found;
            bool near = _animals.carrion_for(*this, found);
            BN_LOG("KXCROW the corpse search from 6 tiles: ", near, " at ", found.x().integer() >> 4, ",",
                   found.y().integer() >> 4);
            _player.teleport(tile_feet(25, 26));
            bool far = _animals.carrion_for(*this, found);
            _player.teleport(tile_feet(40, 26));
            bool close = _animals.carrion_for(*this, found);
            _player.teleport(tile_feet(37, 26));
            BN_LOG("KXCROW   from 18 tiles: ", far, "; from 3 tiles: ", close, " (5 to 14 wanted)");
            int crows = _animals.spawn_crows(*this, bn::fixed_point((43 * world::tile_size) + 8,
                                                                    (26 * world::tile_size) + 10));
            BN_LOG("KXCROW phase 13: corpses at 43,26 and 44,27, a zombie at 49,26; ", crows, " crows");
        }
        else if(frame == 3990)
        {
            // The survivor walks up: 3 tiles from the corpse.
            _player.teleport(tile_feet(40, 26));
            BN_LOG("KXCROW the survivor 3 tiles from the corpse");
        }
        else if(frame == 4190)
        {
            BN_LOG("KXCROW result: birds=", _animals.birds(), " crows=", _animals.crows(), " zombies=", _zombies.count());
        }
        else if(frame == 3720)
        {
            // Foraging by the yard, one hen left of three.
            _player.teleport(tile_feet(35, 33));
            forage();
            BN_LOG("KXFARM foraged by the yard; eggs here=", _animals.eggs_near(35, 33), ", 20 tiles off=",
                   _animals.eggs_near(55, 33));
        }

        // Phase 3: the moment it charges, the survivor is three tiles to the side.
        if(phase == 3 && ! dodged)
        {
            for(int index = 0; index < _animals.count(); ++index)
            {
                if(_animals.state_of(index) == 6)
                {
                    dodged = true;
                    _player.teleport(_player.position() + bn::fixed_point(0, -48));
                    BN_LOG("KXBOAR the survivor steps 3 tiles north at frame ", frame);
                }
            }
        }

        if(phase == 13 && frame % 20 == 0)
        {
            int zombie_x = _zombies.count() ? _zombies.position(0).x().integer() >> 4 : -1;
            int zombie_y = _zombies.count() ? _zombies.position(0).y().integer() >> 4 : -1;
            BN_LOG("KXCROW f=", frame, " birds=", _animals.birds(), " crows=", _animals.crows(), " landing=",
                   _animals.crows_landing(), " flying=", _animals.flock_flying(), " zombie at ", zombie_x, ",",
                   zombie_y);
        }

        if(phase >= 11 && phase <= 12 && frame % 20 == 0)
        {
            for(int index = 0; index < _animals.count(); ++index)
            {
                BN_LOG("KXFARM f=", frame, " ", _animals.kind_of(index) == animal_kind::HEN ? "hen" : "goat", " at ",
                       _animals.position(index).x().integer(), ",", _animals.position(index).y().integer(), " state=",
                       _animals.state_of(index), " distance=",
                       approx_length(_animals.position(index) - _player.position()).integer());
            }
        }

        if(phase >= 6 && phase <= 9 && frame % 15 == 0)
        {
            const survival& body = _player.body();
            int closest = 32767;
            int wolves = 0;

            for(int index = 0; index < _animals.count(); ++index)
            {
                if(_animals.kind_of(index) == animal_kind::WOLF && _animals.state_of(index) != 4)
                {
                    ++wolves;
                    closest = bn::min(closest,
                                      approx_length(_animals.position(index) - _player.position()).integer());
                }
            }

            BN_LOG("KXWOLF f=", frame, " phase=", phase, " wolves=", wolves, " closest=", closest, " hunting=",
                   _animals.pack_hunting(), " kept off=", _animals.kept_off(), " wounds arms=",
                   int(body.wound(body_part::LEFT_ARM).type), ",", int(body.wound(body_part::RIGHT_ARM).type),
                   " legs=", int(body.wound(body_part::LEFT_LEG).type), ",",
                   int(body.wound(body_part::RIGHT_LEG).type));

            for(int index = 0; index < _animals.count(); ++index)
            {
                BN_LOG("KXWOLF   at ", _animals.position(index).x().integer(), ",",
                       _animals.position(index).y().integer(), " state=", _animals.state_of(index), " health=",
                       _animals.health_of(index), " distance=",
                       approx_length(_animals.position(index) - _player.position()).integer());
            }
        }

        if(phase >= 1 && phase <= 4 && frame % 10 == 0)
        {
            const survival& body = _player.body();
            BN_LOG("KXBOAR f=", frame, " phase=", phase, " survivor=", _player.position().x().integer(), ",",
                   _player.position().y().integer(), " speed=", _player.speed(), " legs=",
                   int(body.wound(body_part::LEFT_LEG).type), ",", int(body.wound(body_part::RIGHT_LEG).type));

            for(int index = 0; index < _animals.count(); ++index)
            {
                BN_LOG("KXBOAR   boar at ", _animals.position(index).x().integer(), ",",
                       _animals.position(index).y().integer(), " state=", _animals.state_of(index), " health=",
                       _animals.health_of(index), " distance=",
                       approx_length(_animals.position(index) - _player.position()).integer());
            }
        }
    }
}
#endif

#if KX_BENCH == 14
namespace kx
{

// The loot census (milestone 14 stage 7): every container of every building on the fixed seed is registered, opened
// with the game's own roll (zone tier and all) on day 1, tallied and removed again. One KXCENSUS line per building
// kind and per zone, totals for tools/census_report.py to average and check against the targets in
// docs/milestone-14-plan.md.
namespace
{

struct tally
{
    int buildings = 0;
    int containers = 0;
    int items = 0;
    int food = 0;           // food points (stale food half, rotten none)
    int kitchen_food = 0;   // from kitchen cabinets, fridges and freezers
    int water = 0;
    int guns = 0;
    int safe_guns = 0;      // behind a safe's code
    int ammo = 0;           // rounds
    int ammo_stacks = 0;
    int melee = 0;
    int medical = 0;
    int clothing = 0;
    int weight = 0;         // 10 g
    int expected_food = 0;          // thousandths: what the tables give on average (one roll is a small
    int expected_kitchen = 0;       // sample)
    int expected_guns = 0;
    int expected_ammo_stacks = 0;
};

// A container's expected food, loose guns and ammunition stacks (thousandths), from its table, its zone's loot
// amount and weapon weight and the loot settings, as container_store::open rolls them. Perishables count at 3/4
// (stale half the time).
[[gnu::noinline]] void expect(loot_table table, int tile_x, int tile_y, int& food, int& guns, int& ammo_stacks)
{
    const loot_table_info& value = info(table);
    int cell_x = tile_x >> macro::cell_shift;
    int cell_y = tile_y >> macro::cell_shift;
    int weapon_weight = 1;
    int zone_percent = zone_loot::wild_percent;

    switch(macro::zone_at(cell_x, cell_y))
    {
    case macro::zone::VILLAGE:
        zone_percent = zone_loot::village_percent;
        break;
    case macro::zone::TOWN:
        zone_percent = macro::settlement_quarter(cell_x, cell_y) >= 0 ? zone_loot::city_percent :
                                                                        zone_loot::town_percent;
        break;
    case macro::zone::FARM:
        zone_percent = zone_loot::farm_percent;
        break;
    case macro::zone::MILITARY:
        zone_percent = zone_loot::military_percent;
        weapon_weight = zone_loot::military_weapon_weight;
        break;
    default:
        break;
    }

    int loot_percent = (tuning(inventory_param::LOOT_PERCENT).integer() *
                        sandbox::tuning::loot_percent[sandbox::value(sandbox::setting::LOOT)]) / 100;
    // 32-bit sums only: 64-bit division would link a library routine into IWRAM, which has no room for it.
    int count_milli = ((((value.min_items + value.max_items) * 500 * zone_percent) / 100) * loot_percent) / 100;
    int total = 0;
    int food_sum = 0;           // weight x food x (min + max amount) x percent kept
    int gun_sum = 0;
    int ammo_sum = 0;

    for(const loot_entry& entry : value.entries)
    {
        const item_info& item = info(entry.item);
        int weight = item.category == item_category::WEAPON || is_ammo(entry.item) ? entry.weight * weapon_weight :
                                                                                   int(entry.weight);
        total += weight;

        if(item.flags & item_flag::edible)
        {
            food_sum += weight * item.food * (entry.min_amount + entry.max_amount) * (item.fresh_days ? 75 : 100);
        }

        gun_sum += is_gun(entry.item) ? weight : 0;
        ammo_sum += is_ammo(entry.item) ? weight : 0;
    }

    if(total > 0)
    {
        int food_per_item_milli = (food_sum / total) * 5;   // / 200 for the amount and percent, x 1000
        food = (count_milli * food_per_item_milli) / 1000;
        guns = table == loot_table::GUN_SAFE ? 0 : (count_milli * gun_sum) / total;
        ammo_stacks = (count_milli * ammo_sum) / total;
    }
}

[[gnu::noinline]] void log(const char* label, const char* name, const tally& sum)
{
    if(sum.buildings)
    {
        BN_LOG("KXCENSUS ", label, " ", name, " n=", sum.buildings, " cont=", sum.containers, " items=", sum.items,
               " food=", sum.food, " kitchen=", sum.kitchen_food, " water=", sum.water, " guns=", sum.guns,
               " safeguns=", sum.safe_guns, " ammo=", sum.ammo);
        BN_LOG("KXCENSUS+ ", label, " ", name, " ammostacks=", sum.ammo_stacks, " melee=", sum.melee,
               " med=", sum.medical, " clothes=", sum.clothing, " w=", sum.weight, " xfood=", sum.expected_food,
               " xkitchen=", sum.expected_kitchen, " xguns=", sum.expected_guns,
               " xammo=", sum.expected_ammo_stacks);
    }
}

}

// Kept out of line: inlined into run(), its locals would sit on run()'s IWRAM stack frame from the first frame.
[[gnu::noinline]] void game::_bench_census()
{


    // About 3 KB of tallies: on the heap (EWRAM), not the small IWRAM stack.
    struct tallies
    {
        tally kinds[int(building::COUNT)];
        tally zones[int(macro::zone::COUNT)];
    };

    bn::unique_ptr<tallies> sums(new tallies());
    tally* kinds = sums->kinds;
    tally* zones = sums->zones;
    int today = day();

    for(int cell_y = 0; cell_y < macro::cells; ++cell_y)
    {
        for(int cell_x = 0; cell_x < macro::cells; ++cell_x)
        {
            for(int key = 0; key < 64; ++key)
            {
                world::detail::lot_view lot;

                if(((key >> 2) & 3) >= world_gen::lots_deep || ((key >> 4) & 3) >= world_gen::lots_deep ||
                   ! world::detail::lot_planned(cell_x, cell_y, key, lot))
                {
                    continue;
                }

                // The first of each kind is a stop on the tour (stage 8).
                if(kinds[int(lot.kind)].buildings == 0 && ! _bench_stops.full())
                {
                    _bench_stops.push_back(bench_stop{ int16_t(lot.left), int16_t(lot.top), int16_t(lot.width),
                                                       int16_t(lot.height), uint8_t(lot.kind) });
                }

                world::prepare(((lot.left + (lot.width / 2)) * world::tile_size),
                               ((lot.top + (lot.height / 2)) * world::tile_size));
                tally found;
                found.buildings = 1;

                for(int y = lot.top; y < lot.top + lot.height; ++y)
                {
                    for(int x = lot.left; x < lot.left + lot.width; ++x)
                    {
                        loot_table table = world::container_table(x, y);

                        if(table == loot_table::COUNT)
                        {
                            continue;
                        }

                        int index = _containers.add(container_kind::FURNITURE, table, x, y);

                        if(index < 0)
                        {
                            continue;
                        }

                        static_cast<void>(_containers.open(index, today));
                        ++found.containers;
                        bool kitchen = table == loot_table::KITCHEN || table == loot_table::FRIDGE ||
                                       table == loot_table::FREEZER;
                        int expected_food = 0;
                        int expected_guns = 0;
                        int expected_ammo = 0;
                        expect(table, x, y, expected_food, expected_guns, expected_ammo);
                        found.expected_food += expected_food;
                        found.expected_kitchen += kitchen ? expected_food : 0;
                        found.expected_guns += expected_guns;
                        found.expected_ammo_stacks += expected_ammo;

                        for(const item_stack& stack : _containers.get(index).items)
                        {
                            if(stack.empty())
                            {
                                continue;
                            }

                            const item_info& item = info(stack.id);
                            int quantity = stack.quantity();
                            ++found.items;
                            found.weight += stack.weight();

                            if(item.flags & item_flag::edible)
                            {
                                int food = item.food * quantity;
                                int age = food_age_days(stack, today);

                                if(item.fresh_days > 0 && age > item.fresh_days * 2)
                                {
                                    food = 0;
                                }
                                else if(item.fresh_days > 0 && age > item.fresh_days)
                                {
                                    food /= 2;
                                }

                                found.food += food;
                                found.kitchen_food += kitchen ? food : 0;
                                found.water += bn::max(int(item.water), 0) * quantity;
                            }

                            if(is_gun(stack.id))
                            {
                                ++(table == loot_table::GUN_SAFE ? found.safe_guns : found.guns);
                            }
                            else if(is_ammo(stack.id))
                            {
                                found.ammo += quantity;
                                ++found.ammo_stacks;
                            }
                            else if(item.category == item_category::WEAPON)
                            {
                                ++found.melee;
                            }
                            else if(item.category == item_category::MEDICAL)
                            {
                                ++found.medical;
                            }
                            else if(item.category == item_category::CLOTHING || item.category == item_category::BAG)
                            {
                                ++found.clothing;
                            }
                        }

                        _containers.remove(index);
                    }
                }

                for(tally* sum : { &kinds[int(lot.kind)], &zones[int(macro::zone_at(cell_x, cell_y))] })
                {
                    sum->buildings += found.buildings;
                    sum->containers += found.containers;
                    sum->items += found.items;
                    sum->food += found.food;
                    sum->kitchen_food += found.kitchen_food;
                    sum->water += found.water;
                    sum->guns += found.guns;
                    sum->safe_guns += found.safe_guns;
                    sum->ammo += found.ammo;
                    sum->ammo_stacks += found.ammo_stacks;
                    sum->melee += found.melee;
                    sum->medical += found.medical;
                    sum->clothing += found.clothing;
                    sum->weight += found.weight;
                    sum->expected_food += found.expected_food;
                    sum->expected_kitchen += found.expected_kitchen;
                    sum->expected_guns += found.expected_guns;
                    sum->expected_ammo_stacks += found.expected_ammo_stacks;
                }
            }
        }
    }


    for(int kind = 0; kind < int(building::COUNT); ++kind)
    {
        log("kind", building_names[kind], kinds[kind]);
    }

    constexpr const char* zone_names[] = { "WILDERNESS", "FARM", "VILLAGE", "TOWN", "MILITARY" };

    for(int zone = 0; zone < int(macro::zone::COUNT); ++zone)
    {
        log("zone", zone_names[zone], zones[zone]);
    }

    // The road set pieces on the tour: the first of each.
    bool seen[4] = {};

    for(int cell_y = 0; cell_y < macro::cells; ++cell_y)
    {
        for(int cell_x = 0; cell_x < macro::cells; ++cell_x)
        {
            world::detail::road_piece_view pieces[2];
            int count = world::detail::road_pieces(cell_x, cell_y, pieces);

            for(int index = 0; index < count; ++index)
            {
                int piece = int(pieces[index].kind);

                if(! seen[piece] && ! _bench_stops.full())
                {
                    seen[piece] = true;
                    _bench_stops.push_back(bench_stop{ int16_t(pieces[index].tile_x), int16_t(pieces[index].tile_y), 0,
                                                       0, uint8_t(piece) });
                }
            }
        }
    }

    world::prepare(_player.position().x().integer(), _player.position().y().integer());
    BN_LOG("KXCENSUS done stops=", _bench_stops.size());
}

// The tour (milestone 14 stage 8): to each template's first lot and each set piece in turn, where the chunks are
// generated and drawn; a KXTOUR line says what stands there (doors, containers, beds; the piece's tiles).
[[gnu::noinline]] void game::_bench_tour(int stop)
{
    if(stop >= _bench_stops.size())
    {
        if(stop == _bench_stops.size())
        {
            BN_LOG("KXTOUR done stops=", stop);
        }

        return;
    }

    const bench_stop& value = _bench_stops[stop];
    bool building_stop = value.width > 0;
    int centre_x = building_stop ? value.x + (value.width / 2) : value.x;
    int centre_y = building_stop ? value.y + value.height + 1 : value.y + 3;
    teleport_to_tile(centre_x, centre_y);

    int left = building_stop ? value.x : value.x - 8;
    int top = building_stop ? value.y : value.y - 8;
    int right = building_stop ? value.x + value.width : value.x + 9;
    int bottom = building_stop ? value.y + value.height : value.y + 9;
    int doors = 0;
    int containers = 0;
    int beds = 0;
    int piece_tiles = 0;

    for(int y = top; y < bottom; ++y)
    {
        for(int x = left; x < right; ++x)
        {
            world_gfx wall = world::wall_gfx(x, y);
            doors += wall >= world_gfx::DOOR_FACE_CLOSED && wall <= world_gfx::DOOR_TOP_BROKEN;
            containers += world::container_table(x, y) != loot_table::COUNT;
            beds += world::is_bed(x, y);
            piece_tiles += (wall >= world_gfx::BUS_00 && wall <= world_gfx::BUS_31) || wall == world_gfx::ROADBLOCK ||
                           wall == world_gfx::SANDBAGS || wall == world_gfx::BARRIER ||
                           wall == world_gfx::WATCHTOWER;
        }
    }

    constexpr const char* piece_names[] = { "NONE", "BUS", "BARRICADE", "CHECKPOINT" };
    BN_LOG("KXTOUR ", stop, " ", building_stop ? building_names[value.kind] : piece_names[value.kind], " at ",
           centre_x, ",", centre_y, " zone=", int(macro::zone_at(centre_x >> macro::cell_shift,
                                                                  centre_y >> macro::cell_shift)),
           " doors=", doors, " containers=", containers, " beds=", beds, " piece_tiles=", piece_tiles,
           " loaded=", world::loaded(centre_x, centre_y));

    // Animals plan stage 4: at a farm's buildings, its hens and goats come out.
    if(building_stop && macro::zone_at(centre_x >> macro::cell_shift, centre_y >> macro::cell_shift) ==
                        macro::zone::FARM)
    {
        int yard_x = -1;
        int yard_y = -1;
        bool yard = world::farm_yard(centre_x >> macro::cell_shift, centre_y >> macro::cell_shift, yard_x, yard_y);
        // The lookup takes a tick of its own, then a few come out each tick.
        for(int tick = 0; tick < 4; ++tick)
        {
            _animals.spawn_farms_now(*this);
        }

        int hens = 0;
        int goats = 0;

        for(int index = 0; index < _animals.count(); ++index)
        {
            hens += _animals.kind_of(index) == animal_kind::HEN;
            goats += _animals.kind_of(index) == animal_kind::GOAT;
        }

        BN_LOG("KXFARM tour stop ", stop, ": yard=", yard, " at ", yard_x, ",", yard_y, " minute=", minute_of_day(),
               " hens=", hens, " goats=", goats);
    }
}

}
#endif
