#ifndef KX_GAME_H
#define KX_GAME_H

#include "bn_camera_ptr.h"
#include "bn_fixed_point.h"
#include "bn_random.h"

#include "kx_animals.h"
#include "kx_build_mode.h"
#include "kx_events.h"
#include "kx_weather.h"
#include "kx_camera.h"
#include "kx_car_screen.h"
#include "kx_containers.h"
#include "kx_craft_screen.h"
#include "kx_debug_menu.h"
#include "kx_fx.h"
#include "kx_gear_screen.h"
#include "kx_health_screen.h"
#include "kx_horde_edges.h"
#include "kx_hud.h"
#include "kx_input.h"
#include "kx_player.h"
#include "kx_safe_screen.h"
#include "kx_search_screen.h"
#include "kx_map_screen.h"
#include "kx_menu_screen.h"
#include "kx_skills_screen.h"
#include "kx_vehicles.h"
#include "kx_world_gfx_ids.h"
#include "kx_guns.h"
#include "kx_throwables.h"
#include "kx_light.h"
#include "kx_music.h"
#include "kx_world_map.h"
#include "kx_world_renderer.h"
#include "kx_zombies.h"

namespace kx
{

// One life on the test map: owns every system and the services they share. run() returns after death,
// when the player chooses to try again (main then builds a fresh game).
// Why a game ended: quit from the Start menu (back to the title), or the survivor died (a new one starts).
enum class game_exit : uint8_t
{
    QUIT,
    DIED
};

class game
{

public:
    // A new game on the world just built, or the saved one continued (load_save). A new survivor (new world, or
    // continuing after a death) is made from the creation screen's choice; nullptr keeps the default.
    explicit game(bool load_save = false, const character_choice* choice = nullptr);

    [[nodiscard]] game_exit run();

    [[nodiscard]] fx_pool& fx()
    {
        return _fx;
    }

    // Aiming and shot sprites (kx_guns).
    [[nodiscard]] gun_effects& guns()
    {
        return _gun_fx;
    }

    // Thrown items and fires (kx_throwables).
    [[nodiscard]] throw_manager& throws()
    {
        return _throws;
    }

    [[nodiscard]] zombie_manager& zombies()
    {
        return _zombies;
    }

    [[nodiscard]] animal_manager& animals()
    {
        return _animals;
    }

    // A deer carcass in front with a knife or an axe in hand: A held butchers it (milestone 13).
    void butcher();

    // A hen caught, a goat milked (animals plan stage 4: the animal the prompt named).
    void catch_hen();
    void milk_goat();

    [[nodiscard]] player& hero()
    {
        return _player;
    }

    [[nodiscard]] hud& hud_ref()
    {
        return _hud;
    }

    [[nodiscard]] container_store& containers()
    {
        return _containers;
    }

    [[nodiscard]] vehicle_manager& vehicles()
    {
        return _vehicles;
    }

    // How full the world's save lists are (the fullest of containers, item pages, wall changes and base pieces).
    [[nodiscard]] int storage_percent() const;

    [[nodiscard]] bn::random& random()
    {
        return _random;
    }

    // Screen x of a world position relative to the screen centre, for stereo panning.
    [[nodiscard]] int screen_x(const bn::fixed_point& position) const;
    [[nodiscard]] int screen_y(const bn::fixed_point& position) const;

    // A boxed message of up to 3 lines (radio broadcasts).
    void message(const char* line1, const char* line2, const char* line3, int frames)
    {
        _hud.message(line1, line2, line3, frames);
    }

    // Debug: an item into the bag (a full stack of stackables; fresh food; the book volume the skill needs next).
    void give_item(item_id id);

    // Debug: a world event now (0 helicopter, 1 gunshots, 2 alarm, 3 crash site), and time skipped ahead.
    void trigger_event(int which)
    {
        _events.trigger(*this, which);
    }

    void skip_days(int days)
    {
        _world_minute += days * 24 * 60;
    }

    // A zombie at the car the player sits in hits it (milestone 12.5): noise, a thump, condition off the car. True
    // when the car is battered enough that the zombie drags the driver out (the caller then grabs them).
    bool zombie_hits_car(const bn::fixed_point& from);

    // Night (plan 12.5): the player is lit (a flashlight on, driving with headlights, by a burning campfire, in a lit
    // building), so zombies see them from farther. Worked out once a frame; false by day.
    [[nodiscard]] bool player_lit() const
    {
        return _player_lit;
    }

    // Debug: the clock forward to the next time it reads this hour (never back, so nothing lazy runs backwards).
    void skip_to_hour(int hour)
    {
        int minutes = ((hour * 60) - minute_of_day() + (24 * 60)) % (24 * 60);
        _world_minute += minutes;
    }


    // The radio (spec: broadcasts through the first week).
    void listen_radio()
    {
        _events.listen(*this);
    }

    // Game feel: hitstop pauses the simulation (rendering keeps running); shake offsets the world layers.
    void request_hitstop(int frames);
    void shake(int pixels, int frames);

    // Writes a decal (blood, corpse) into the ground layer under a world position.
    void add_decal(const bn::fixed_point& position, world_decal decal);

    // Noise event: zombies within the radius (tiles) turn towards its source.
    void noise(const bn::fixed_point& position, int radius_tiles);

    void toast(const char* text, int bank, int frames = 90);

    void add_kill()
    {
        ++_kills;
    }

    // A killed zombie leaves a searchable corpse.
    // True if it made one (a tile already holding a container keeps it).
    bool add_corpse(const bn::fixed_point& position, loot_table table = loot_table::ZOMBIE_CORPSE, int body = 0);

    // Drops a stack at the player's feet (into the floor pile or corpse there). False if there is no room.
    bool drop_item(const item_stack& stack);

    // The same on a tile (a thrown alarm clock once it stops ringing).
    bool drop_item_at(const item_stack& stack, int tile_x, int tile_y);

    // Removes a floor pile once it is empty, with its ground graphic.
    void remove_empty_pile(int index);

    // True if a standing zombie is within the distance of the player's feet (A then attacks, not searches).
    [[nodiscard]] bool zombie_near(int distance) const;

    // Searches a container (extra_items: more finds when it is rolled, for foraging skill).
    // A gun safe not yet opened shows its dial instead, unless unlocked (its code was just entered).
    void begin_search(int container_index, int extra_items = 0, bool unlocked = false);

    // A car's TRUNK or GLOVE BOX from the car screen (playtest 4): searched, and closing the search (B or Start)
    // goes back to the car screen on the same car.
    void search_car_container(int container_index, int vehicle);

    // In-game clock (spec: needs update once per in-game minute). Day 1 starts at 09:00.
    [[nodiscard]] int day() const
    {
        return (_world_minute / (24 * 60)) + 1;
    }

    [[nodiscard]] int minute_of_day() const
    {
        return _world_minute % (24 * 60);
    }

    // Sleep in a bed: choose the hours (spec: sleep screen, hours to sleep).
    void begin_sleep();

    // Reads a skill book (spec: Books): takes in-game time, needs light (daylight or a flashlight carried) and no
    // zombies close. False (with a message) if it can't be read now.
    bool begin_reading(skill taught, int volume);
    bool begin_reading_magazine(magazine value);

    // ----- cooking (spec: Cooking; the Craft tab) -----

    // What would cook here: a lit campfire or a working stove close by, else a gas stove with propane carried.
    [[nodiscard]] heat_kind heat_source();

    // A water source close by to boil: 0 none, 1 a river, pond or working tap, 2 a rain collector (one drink used).
    [[nodiscard]] int water_source();

    // Units of an item carried (any_produce: berries, mushrooms, herbs, apples and crops; tools: one if carried).
    [[nodiscard]] int carries(item_id id) const;

    // Whether a recipe can be cooked now (else why not).
    [[nodiscard]] bool recipe_ready(int index, const char*& problem);
    [[nodiscard]] int cook_minutes(int index) const;
    void cook(int index);

    // Closes the Start menu (eating continues in the world).
    void close_menus();

    void player_bitten();

    [[nodiscard]] bool god_mode() const
    {
        return _god_mode;
    }

    void set_god_mode(bool enabled)
    {
        _god_mode = enabled;
    }

    void respawn_zombies();
    void spawn_horde();
    void give_test_kit();

    // Debug: to the crossroads of a settlement (macro map index).
    void teleport_to_settlement(int index);

    // A drink from the water or rain collector in front of the player (unboiled: it may make them sick).
    void drink();

    // Forages the ground around the player (spec: search mode in forests and fields): once a day per area.
    void forage();

    // What is in front (or a farm plot underfoot): a plot is planted with the first seeds carried, watered from a
    // watering can, harvested when ripe or cleared when it rotted; a generator is fuelled or switched on and off;
    // a trap gives up its catch; a fuel pump fills fuel cans while the power is on.
    void tend();

    // Doors (milestone 12.7): A opens or closes the door the context found; a locked one is unlocked with its key
    // (or from inside), forced with a crowbar in hand or picked with a screwdriver (A held), or rattles.
    void use_door();

    // Starts chopping the tree the context found (milestone 12.7 stage 5).
    void chop();

    // An item taken into the bag from a search (milestone 12.7: a key marks its house).
    void item_taken(const item_stack& stack);

    // Reads a note (milestone 12.7 stage 4): its text in the message box; a safe's or a stash's place goes on the
    // Map tab, and a stash is put out in the woods.
    void read_note(const item_stack& stack);

    // The bicycle, car or wreck in front of the player: rides the bicycle, or opens the car screen (mockup 10).
    void use_vehicle();

    // Work on a car takes in-game time (fitting a part, siphoning, hotwiring); the vehicles finish it afterwards.
    // False (with a message) with zombies close.
    bool begin_car_work(int minutes);

    // Debug: a car that runs (0) or a bicycle (1) beside the player.
    void spawn_vehicle(int which);

    // Build mode (mockup 06): opened with the hammer on the hotbar.
    void begin_build(bool farming = false);

    [[nodiscard]] bool building() const
    {
        return _build.active();
    }

    [[nodiscard]] int world_minute() const
    {
        return _world_minute;
    }

    // The weather now (worked out once per in-game minute).
    [[nodiscard]] weather_kind weather() const
    {
        return _weather;
    }

    [[nodiscard]] const bn::camera_ptr& camera() const
    {
        return _camera;
    }

    // Redraws a tile whose graphic changed.
    void refresh_tile(int tile_x, int tile_y);

    // A zombie hits the window or barricade on a tile (spec: attack barricades, smash closed windows, loud).
    void bash(int tile_x, int tile_y, const bn::fixed_point& from);

    // Debug: to a tile, with the chunks around it generated at once.
    void teleport_to_tile(int tile_x, int tile_y);

private:
    bn::camera_ptr _camera;
    world_renderer _renderer;
    fx_pool _fx;
    container_store _containers;
    zombie_manager _zombies;
    vehicle_manager _vehicles;
    horde_edges _edges;
    player _player;
    camera_controller _camera_controller;
    hud _hud;
    debug_menu _menu;
    search_screen _search;
    gear_screen _gear;
    health_screen _health;
    skills_screen _skills;
    menu_repeat _menu_repeat;   // held directions in menu lists (playtest 5)
    menu_screen _menu_tab;   // the Start menu's MENU tab (the debug menu is _menu)
    map_screen _map;
    craft_screen _craft;
    car_screen _car;
    safe_screen _safe;
    build_mode _build;

    // Windows and closed doors being smashed (their health; one is only remembered once it breaks).
    struct dent
    {
        int16_t tile_x = 0;
        int16_t tile_y = 0;
        int16_t health = 0;
    };

    bn::array<dent, 8> _dents = {};
    int _next_dent = 0;
    bn::optional<bn::sprite_ptr> _flames[3];
    light_sprites _lights;
    mark_arrow _mark_arrow;
    gun_effects _gun_fx;
    throw_manager _throws;
    animal_manager _animals;
    int _butcher_id = -1;               // the carcass being butchered or offered (milestone 13)
    int _farm_animal_id = -1;       // the hen or goat a [A] CATCH or [A] MILK prompt names
    bool _player_lit = false;
    bool _reading_dark = false;     // reading stopped because the light went
    weather_effects _weather_effects;
    world_events _events;
    weather_kind _weather = weather_kind::CLEAR;

    void _update_flames();

    // Something stands in a doorway (the survivor or a zombie), so its door can't close.
    [[nodiscard]] bool _doorway_occupied(int tile_x, int tile_y) const;

    // A locked door: how A gets in (milestone 12.7); and chopping a tree (stage 5), held the same way.
    enum class work_kind : uint8_t
    {
        NONE,           // rattles
        UNLOCK,         // its key carried, or from inside
        FORCE,          // a crowbar in hand
        PICK,           // a screwdriver carried
        CHOP,           // an axe in hand, facing a tree
        BUTCHER         // a knife or an axe in hand, facing a deer carcass (milestone 13)
    };

    [[nodiscard]] work_kind _lock_way(int tile_x, int tile_y);
    void _unlocked(int tile_x, int tile_y, world::door_state state, const char* message);

    // Forcing or picking a lock, or chopping a tree: A held, standing still, until it is done.
    work_kind _door_work = work_kind::NONE;
    int _door_work_frame = 0;
    int _door_work_length = 0;
    void _update_door_work(const input_frame& input);

    // Night lights (kx_light): the flashlight's battery each minute, the light windows each frame, and whether
    // the building the player is in has its lights on.
    [[nodiscard]] bool _building_lit() const;
    [[nodiscard]] item_stack* _held_flashlight();
    void _tick_flashlight();
    void _update_lights(int darkness);
    void _campfires_call(int hour);

    #if KX_BENCH == 12
        // Doors bench script and log (see kx_game.cpp).
        void _bench_doors(int frame);
        void _bench_notes(int frame);
        void _bench_wood(int frame, uint16_t broken_house);
    #endif

    #if KX_BENCH == 13
        // The wild bench script and log (see kx_game.cpp).
        void _bench_wild(int frame);
    #endif

    #if KX_BENCH == 16
        // The animal census script and log (see kx_game.cpp).
        void _bench_animal_census(int frame);
    #endif

    #if KX_BENCH == 15
        // The animals bench script and log (see kx_game.cpp).
        void _bench_animals(int frame);
        bn::fixed_point _bench_target;
    #endif

    #if KX_BENCH == 14
        // The loot census (milestone 14 stage 7) and the tour of every template and set piece (stage 8; see
        // kx_game.cpp).
        struct bench_stop
        {
            int16_t x;          // a building's rectangle in tiles, or a set piece's middle (width 0)
            int16_t y;
            int16_t width;
            int16_t height;
            uint8_t kind;       // kx::building, or the road piece
        };

        bn::vector<bench_stop, 48> _bench_stops;

        void _bench_census();
        void _bench_tour(int stop);
    #endif

    #if KX_BENCH == 11
        // Guns bench script and log (see kx_game.cpp).
        void _bench_guns(int frame);
        void _bench_throws(int frame);
        int _bench_kills = 0;
    #endif

    #if KX_BENCH == 10
        // Night bench script and log (see kx_game.cpp).
        void _bench_night(int frame);
        int _bench_tile_x = 0;
        int _bench_tile_y = 0;
        int _bench_building_x = 0;
        int _bench_building_y = 0;
    #endif
    input_source _input;
    bn::random _random;
    int _hitstop = 0;
    int _kills = 0;
    int _frames = 0;
    int _death_timer = 0;
    int _world_minute = 9 * 60;
    int _minute_frames = 0;
    int _start_minute = 9 * 60;
    int _sleep_state = 0;           // see kx_game.cpp

    int _music_frames = 0;          // since the music director last looked

    #if KX_BENCH
        music::track _bench_track = music::track::NONE;
    #endif
    int _sleep_hours = 8;
    int _sleep_minutes_left = 0;
    int _sleep_fade = 0;
    bool _forced_sleep = false;
    // What the sleep fast-forward is for: sleeping, or (awake) reading a book or magazine, or cooking.
    enum class activity : uint8_t
    {
        SLEEP,
        READ_BOOK,
        READ_MAGAZINE,
        COOK,
        MECHANIC        // work on a car
    };

    activity _activity = activity::SLEEP;
    int8_t _activity_value = 0;     // the skill, magazine or recipe
    int8_t _reading_volume = 0;
    int _roof_tile_x = -1;          // the feet tile the building below was looked up for
    int _roof_tile_y = -1;
    int _building_left = 0;         // the building the player is in, in tiles
    int _building_top = 0;
    int _building_width = 0;
    int _building_height = 0;
    bool _inside = false;
    bool _god_mode = false;
    bool _dead = false;
    bool _death_shown = false;
    bool _menu_dimmed = false;
    int _search_car = -1;               // the car whose trunk or glove box is being searched (playtest 4)

    // The search screen's frame; closing a car's trunk or glove box goes back to the car screen.
    void _update_search(const input_frame& input);
    int _drown_frames = 0;              // towards the next point of health lost drowning (milestone 13)
    bool _storage_warned = false;       // the world-storage toast has shown (milestone 13)
    int _minute_distance = 0;
    // Areas foraged lately (8x8 tiles each, with the day), so each is foraged once a day.
    struct foraged_area
    {
        int16_t x = -1;
        int16_t y = -1;
        int16_t day = 0;
    };

    bn::array<foraged_area, 16> _foraged = {};
    int _next_foraged = 0;
    int _vehicle_context = -1;      // the vehicle A uses
    int _door_x = 0;                // the door A opens or closes
    int _door_y = 0;
    int _tend_x = 0;                // the farm plot A tends
    int _tend_y = 0;
    char _tend_text[24] = {};
    int _can_rattle_frame = -1000;  // the last time a can alarm line rattled
    bool _on_can_line = false;      // the player stands on one (it rattles once, stepping on)
    int _drink_tile_x = 0;          // the water or rain collector A drinks from
    int _drink_tile_y = 0;       // distance walked when the last in-game minute ended (skill XP)
    bool _loaded = false;           // continued from the save
    bool _new_survivor = false;     // continued after a death

    // Saves the character and the world changes, at once or over the next frames.
    void _save(bool at_once);

    // Reads the save into the new game; false if there is none.
    [[nodiscard]] bool _load();

    void _update_play();
    void _update_context();

    // What A does at a farm plot ("[A] WATER"), or nullptr for nothing; a watering can with water in it.
    [[nodiscard]] const char* _plot_prompt(int plot);
    [[nodiscard]] const char* _device_prompt(int tile_x, int tile_y);
    [[nodiscard]] item_stack* _watering_can();
    [[nodiscard]] item_stack* _fuel_can(bool full_ok);
    void _tend_at(int tile_x, int tile_y);
    void _update_can_lines();
    [[nodiscard]] bool _reading_light();
    void _update_population();
    void _add_decal(int tile_x, int tile_y, world_decal decal);
    void _tick_minute(bool sleeping);
    void _freeze_day();
    void _update_drowning();
    void _update_cues();
    void _update_sleep(const input_frame& input);
    void _start_sleeping(int hours, bool forced);
    void _open_tab(int tab);
    void _screen_toast();
    void _update_menu_dim();
    [[nodiscard]] bool _menu_open() const;
    bool _update_death();
};

}

#endif
