#include "kx_title.h"
#include "kx_music.h"

#include "bn_bgs_mosaic.h"
#include "bn_blending.h"
#include "bn_camera_ptr.h"
#include "bn_core.h"
#include "bn_display.h"
#include "bn_keypad.h"

#include "kx_bench.h"
#include "kx_canvas.h"
#include "kx_hash.h"
#include "kx_sandbox.h"
#include "kx_save.h"
#include "kx_settings.h"
#include "kx_menu_screen.h"
#include "kx_timeline.h"
#include "kx_ui.h"
#include "kx_world.h"
#include "kx_world_renderer.h"

namespace kx
{

namespace
{
    constexpr int panel_left = 52;
    constexpr int panel_top = 52;
    constexpr int panel_width = canvas::width - (panel_left * 2);
    constexpr int panel_height = 58;            // CONTINUE (and the save's day), NEW WORLD, MENU
    constexpr int center_x = canvas::width / 2;
    constexpr const char* game_name = "POCKET OUTBREAK";     // playtest 1: the title (it was KNOX)
    constexpr int footer_y = 147;

    // A footer hint: the button in the text colour, then what it does, quiet.
    struct footer_hint
    {
        const char* button;
        const char* action;
    };

    constexpr int hint_button_gap = 4;
    constexpr int hint_gap = 10;

    [[nodiscard]] int footer_width(const footer_hint* items, int count)
    {
        int total = hint_gap * (count - 1);

        for(int index = 0; index < count; ++index)
        {
            total += canvas::text_width(items[index].button) + hint_button_gap +
                     canvas::text_width(items[index].action);
        }

        return total;
    }

    // Hints along a line, centred on the screen (playtest 4: the title's footers started at the left margin);
    // returns the pixel after the last.
    int footer(canvas& pixels, int y, const footer_hint* items, int count, int extra_width = 0)
    {
        int x = (canvas::width - footer_width(items, count) - extra_width) / 2;

        for(int index = 0; index < count; ++index)
        {
            x = pixels.text(x, y, items[index].button, colour::text) + hint_button_gap;
            x = pixels.text(x, y, items[index].action, colour::quiet) + hint_gap;
        }

        return x;
    }

    // Behind the title: the saved world around where the survivor stood (a fixed village without a save),
    // pixelated and darkened, drifting slowly east. It fades in once the chunks on screen are generated.
    constexpr uint32_t demo_seed = 0x4B4E4F58;
    constexpr int backdrop_mosaic = 5;              // pixels per mosaic block (1-16)
    constexpr int backdrop_darkness_percent = 60;   // of the way to black
    constexpr int backdrop_fade_frames = 40;
    constexpr int backdrop_drift_frames = 4;        // one pixel east every this many frames

    class backdrop
    {

    public:
        explicit backdrop(const save::summary& saved) :
            _camera(bn::camera_ptr::create(0, 0)),
            _renderer(_camera)
        {
            world::init(saved.valid ? saved.seed : demo_seed);

            if(saved.valid && saved.focus_x > 0)
            {
                _x = saved.focus_x;
                _y = saved.focus_y - 8;
            }
            else
            {
                int tile_x;
                int tile_y;
                world::start_tile(tile_x, tile_y);
                _x = (tile_x * world::tile_size) + (world::tile_size / 2);
                _y = tile_y * world::tile_size;
            }

            _renderer.set_visible(false);
            _renderer.set_backdrop(bn::fixed(backdrop_mosaic - 1) / 15);
            bn::blending::set_fade_alpha(1);
        }

        ~backdrop()
        {
            bn::blending::set_fade_alpha(0);
            bn::bgs_mosaic::set_stretch(0);
        }

        // Once per frame, before bn::core::update().
        void frame()
        {
            ++_frames;

            if(_shown && _frames % backdrop_drift_frames == 0)
            {
                ++_x;
            }

            int half_width = bn::display::width() / 2;
            int half_height = bn::display::height() / 2;
            _x = bn::clamp(_x, half_width, world::width_px() - half_width - 1);
            _y = bn::clamp(_y, half_height, world::height_px() - half_height - 1);
            int left = _x - half_width;
            int top = _y - half_height;
            _camera.set_position(_x, _y);
            world::update(_x, _y, left, top);
            _renderer.update(left, top);

            int right_tile = (left + bn::display::width() - 1) >> world::tile_shift;
            int bottom_tile = (top + bn::display::height() - 1) >> world::tile_shift;

            if(! _shown && world::loaded(left >> world::tile_shift, top >> world::tile_shift) &&
               world::loaded(right_tile, bottom_tile))
            {
                _shown = true;
                _renderer.set_visible(true);
            }

            if(_shown && _fade < backdrop_fade_frames)
            {
                ++_fade;
                int percent = 100 - (((100 - backdrop_darkness_percent) * _fade) / backdrop_fade_frames);
                bn::blending::set_fade_alpha(bn::fixed(percent) / 100);
            }
        }

    private:
        bn::camera_ptr _camera;
        world_renderer _renderer;
        int _x = 0;
        int _y = 0;
        int _frames = 0;
        int _fade = 0;
        bool _shown = false;
    };

    // The tester build says so under the title.
    void title_tag(canvas& pixels)
    {
        #if KX_DEBUG
            pixels.text_centered(center_x, 43, "DEBUG BUILD", colour::ui_red);
        #else
            static_cast<void>(pixels);
        #endif
    }

    void seed_text(char* output, uint32_t seed, bool rolled)
    {
        const char* prefix = "SEED  ";
        int length = 0;

        while(*prefix)
        {
            output[length++] = *prefix++;
        }

        if(! rolled)
        {
            for(const char* word = "RANDOM"; *word; ++word)
            {
                output[length++] = *word;
            }
        }
        else
        {
            for(int shift = 28; shift >= 0; shift -= 4)
            {
                int digit = int((seed >> shift) & 15);
                output[length++] = char(digit < 10 ? '0' + digit : 'A' + digit - 10);
            }
        }

        output[length] = 0;
    }
}

namespace
{
    // The new world screen (mockup 13, panel 6): the seed, then the sandbox settings. Up and down pick a line,
    // left and right roll a new seed or change a setting, A or Start begins. With a save to replace it warns, and B
    // goes back (returns false).
    constexpr int world_panel_left = 40;
    constexpr int world_panel_top = 22;
    constexpr int world_panel_width = canvas::width - (world_panel_left * 2);
    constexpr int world_panel_height = 110;
    constexpr int world_seed_y = world_panel_top + 16;
    constexpr int settings_top = world_panel_top + 39;
    constexpr int setting_step = 11;
    constexpr int detail_y = world_panel_top + 97;
    constexpr int row_left = world_panel_left + 8;
    constexpr int row_right = world_panel_left + world_panel_width - 8;

    void draw_world_rows(canvas& pixels, uint32_t seed, bool rolled, const sandbox::settings& chosen, int selected)
    {
        // The seed line.
        char text[24];
        seed_text(text, seed, rolled);
        pixels.rect(world_panel_left + 2, world_seed_y - 2, world_panel_width - 4, 9,
                    selected == 0 ? colour::border : colour::fill);
        pixels.text_centered(center_x, world_seed_y, text, selected == 0 ? colour::accent : colour::text);

        // The settings, the picked one with arrows round its value.
        for(int index = 0; index < sandbox::setting_count; ++index)
        {
            sandbox::setting which = sandbox::setting(index);
            int y = settings_top + (index * setting_step);
            bool picked = selected == index + 1;
            const char* value = sandbox::value_name(which, chosen.values[index]);
            pixels.rect(world_panel_left + 2, y - 2, world_panel_width - 4, 9, picked ? colour::border : colour::fill);
            pixels.text(row_left, y, sandbox::name(which), picked ? colour::text : colour::quiet);

            if(picked)
            {
                int length = ui::append(text, 0, "< ");
                length = ui::append(text, length, value);
                ui::append(text, length, " >");
                pixels.text_right(row_right, y, text, colour::accent);
            }
            else
            {
                pixels.text_right(row_right, y, value, colour::text);
            }
        }

        // What the picked setting's value means.
        pixels.rect(world_panel_left + 2, detail_y, world_panel_width - 4, 7, colour::fill);

        if(selected > 0)
        {
            sandbox::setting which = sandbox::setting(selected - 1);
            pixels.text_centered(center_x, detail_y, sandbox::detail(which, chosen.values[selected - 1]),
                                 colour::quiet);
        }
        else
        {
            pixels.text_centered(center_x, detail_y, "<> ROLLS A NEW WORLD", colour::quiet);
        }
    }

    // The game's settings from the title's MENU (playtest 2, mockup 18): the rows in a panel, the chosen one's help
    // under it; B (or Start) goes back.
    void title_settings(canvas& pixels, backdrop& scene)
    {
        constexpr int left = 40;
        constexpr int top = 22;
        constexpr int width = canvas::width - (left * 2);
        constexpr int rows_top = top + 22;
        constexpr int height = 22 + (settings::count * menu_screen::row_step) + 8;
        int selected = 0;

        auto draw = [&]()
        {
            pixels.clear();
            pixels.text_centered(center_x, 10, game_name, colour::accent);
            pixels.panel(left, top, width, height, colour::fill, colour::border);
            pixels.text_centered(center_x, top + 6, "SETTINGS", colour::accent);
            pixels.rect(left + 4, top + 14, width - 8, 1, colour::border);
            menu_screen::draw_settings(pixels, left, rows_top, width, selected);

            int y = top + height + 6;

            for(int line = 0; line < settings::help_lines; ++line)
            {
                if(const char* text = settings::help(settings::setting(selected))[line])
                {
                    pixels.text_centered(center_x, y, text, colour::quiet);
                    y += 8;
                }
            }

            constexpr footer_hint hints[] = { { "UP/DOWN", "PICK" }, { "<>", "CHANGE" }, { "B", "BACK" } };
            footer(pixels, footer_y, hints, 3);
        };

        draw();

        // The press that opened them is still down this frame (Start opens them from the menu and also closes
        // them): wait a frame before reading the buttons (playtest 2).
        scene.frame();
        bn::core::update();

        #if KX_DUMP
            int shown = 0;
        #endif

        while(true)
        {
            int dx = int(bn::keypad::right_pressed()) - int(bn::keypad::left_pressed());
            int dy = int(bn::keypad::down_pressed()) - int(bn::keypad::up_pressed());

            #if KX_DUMP
                // DUMP=1: down to SOUND EFFECTS, one step quieter, the screen logged (dump 9), then back.
                ++shown;
                dy = shown == 4 ? 1 : dy;
                dx = shown == 8 ? -1 : dx;

                if(shown == 12)
                {
                    pixels.dump(9);
                    static_cast<void>(settings::step(settings::setting::SOUND, 1));
                    return;
                }
            #endif

            if(menu_screen::handle_settings(selected, dx, dy))
            {
                draw();
            }

            if(bn::keypad::b_pressed() || bn::keypad::start_pressed())
            {
                return;
            }

            scene.frame();
            bn::core::update();
        }
    }

    // The guide from the title's MENU (playtest 3, mockup 20): the same pages as the Start menu's, under the title;
    // B (or Start) goes back.
    void title_guide(canvas& pixels, backdrop& scene)
    {
        int topic = 0;
        int page = 0;

        auto draw = [&]()
        {
            pixels.clear();
            pixels.text_centered(center_x, 9, game_name, colour::accent);
            menu_screen::draw_guide(pixels, 22, topic, page);

            constexpr footer_hint hints[] = { { "UP/DOWN", "TOPIC" }, { "<>", "PAGE" }, { "B", "BACK" } };
            footer(pixels, footer_y, hints, 3);
        };

        draw();

        // The press that opened it is still down this frame.
        scene.frame();
        bn::core::update();

        #if KX_DUMP
            int shown = 0;
        #endif

        while(true)
        {
            int dx = int(bn::keypad::right_pressed()) - int(bn::keypad::left_pressed());
            int dy = int(bn::keypad::down_pressed()) - int(bn::keypad::up_pressed());

            #if KX_DUMP
                // DUMP=1: down to GUNS, its second page, the screen logged (dump 11), then back.
                ++shown;
                dy = shown >= 2 && shown <= 5 ? 1 : dy;
                dx = shown == 7 ? 1 : dx;

                if(shown == 9)
                {
                    pixels.dump(11);
                    return;
                }
            #endif

            if(menu_screen::handle_guide(topic, page, dx, dy))
            {
                draw();
            }

            if(bn::keypad::b_pressed() || bn::keypad::start_pressed())
            {
                return;
            }

            scene.frame();
            bn::core::update();
        }
    }

    // The saved world's seed and sandbox settings from the title's MENU (playtest 4): the WORLD page under the
    // title; B (or Start) goes back.
    void title_world(canvas& pixels, backdrop& scene, const save::summary& saved)
    {
        int selected = 0;

        auto draw = [&]()
        {
            pixels.clear();
            pixels.text_centered(center_x, 9, game_name, colour::accent);
            menu_screen::draw_world(pixels, 22, saved.seed, saved.sandbox, selected);

            constexpr footer_hint hints[] = { { "UP/DOWN", "CHOOSE" }, { "B", "BACK" } };
            footer(pixels, footer_y, hints, 2);
        };

        draw();

        // The press that opened it is still down this frame.
        scene.frame();
        bn::core::update();

        while(true)
        {
            int dy = int(bn::keypad::down_pressed()) - int(bn::keypad::up_pressed());

            if(menu_screen::handle_world(selected, dy))
            {
                draw();
            }

            if(bn::keypad::b_pressed() || bn::keypad::start_pressed())
            {
                return;
            }

            scene.frame();
            bn::core::update();
        }
    }

    // The title's MENU (playtest 3, mockup 20): SETTINGS and GUIDE, as in the Start menu's MENU tab, and with a saved
    // world its WORLD (playtest 4; not from the new world screen, where it would show the world being replaced);
    // A opens one, B (or Start) goes back.
    void title_menu(canvas& pixels, backdrop& scene, const save::summary* saved = nullptr)
    {
        constexpr int left = 60;
        constexpr int top = 40;
        constexpr int width = canvas::width - (left * 2);
        int count = saved ? menu_screen::title_entry_count : menu_screen::title_entry_count - 1;
        int height = 22 + (count * menu_screen::row_step) + 6;
        int selected = 0;

        auto draw = [&]()
        {
            pixels.clear();
            pixels.text_centered(center_x, 10, game_name, colour::accent);
            pixels.panel(left, top, width, height, colour::fill, colour::border);
            pixels.text_centered(center_x, top + 6, "MENU", colour::accent);
            pixels.rect(left + 4, top + 14, width - 8, 1, colour::border);
            menu_screen::draw_entries(pixels, left, top + 22, width, selected, count);

            int y = top + height + 6;

            for(int line = 0; line < menu_screen::entry_help_lines; ++line)
            {
                pixels.text_centered(center_x, y, menu_screen::entry_help(selected)[line], colour::quiet);
                y += 8;
            }

            constexpr footer_hint hints[] = { { "A", "OPEN" }, { "D-PAD", "CHOOSE" }, { "B", "BACK" } };
            footer(pixels, footer_y, hints, 3);
        };

        draw();

        // The press that opened it is still down this frame.
        scene.frame();
        bn::core::update();

        #if KX_DUMP
            int shown = 0;
        #endif

        while(true)
        {
            int dy = int(bn::keypad::down_pressed()) - int(bn::keypad::up_pressed());
            bool open = bn::keypad::a_pressed();

            #if KX_DUMP
                // DUMP=1: the menu logged (dump 10) with GUIDE chosen, the guide (dump 11), then the settings
                // (dump 9), then back.
                ++shown;
                dy = shown == 2 ? 1 : shown == 6 ? -1 : 0;
                open = shown == 4 || shown == 7;

                if(shown == 3)
                {
                    pixels.dump(10);
                }
                else if(shown == 8)
                {
                    return;
                }
            #endif

            if(dy)
            {
                selected = (selected + dy + count) % count;
                draw();
            }

            if(open)
            {
                if(selected == 0)
                {
                    title_settings(pixels, scene);
                }
                else if(selected == 1)
                {
                    title_guide(pixels, scene);
                }
                else
                {
                    title_world(pixels, scene, *saved);
                }

                draw();
                scene.frame();
                bn::core::update();
                continue;
            }

            if(bn::keypad::b_pressed() || bn::keypad::start_pressed())
            {
                return;
            }

            scene.frame();
            bn::core::update();
        }
    }

    [[nodiscard]] bool new_world(canvas& pixels, backdrop& scene, bool replaces, uint32_t& frames, uint32_t& result,
                                 sandbox::settings& chosen)
    {
        auto draw_fixed = [&]()
        {
        pixels.clear();
        pixels.text_centered(center_x, 10, game_name, colour::accent);
        pixels.panel(world_panel_left, world_panel_top, world_panel_width, world_panel_height, colour::fill,
                     colour::border);
        pixels.text_centered(center_x, world_panel_top + 6, "NEW WORLD", colour::accent);
        pixels.rect(world_panel_left + 4, world_panel_top + 25, world_panel_width - 8, 1, colour::border);
        pixels.text_centered(center_x, world_panel_top + 29, "SANDBOX", colour::quiet);

        // Under the panel: Select for the MENU (playtest 2, the settings; playtest 3, with the guide), and with a
        // save the warning that the new world replaces it after it, the line centred; then the footer hints, centred
        // too (playtest 4: with a save the two sat at the line's ends, and the hints at the left margin).
        constexpr int line_y = world_panel_top + world_panel_height + 3;
        constexpr const char* warning = "REPLACES THE SAVED WORLD";
        constexpr footer_hint select_hint[] = { { "SELECT", "MENU" } };
        int warning_width = replaces ? hint_gap + canvas::text_width(warning) : 0;
        int x = footer(pixels, line_y, select_hint, 1, warning_width);

        if(replaces)
        {
            pixels.text(x, line_y, warning, colour::ui_red);
        }

        constexpr footer_hint hints[] = { { "A", "START" }, { "UP/DOWN", "PICK" }, { "<>", "CHANGE" },
                                          { "B", "BACK" } };
        footer(pixels, footer_y, hints, replaces ? 4 : 3);
        };

        draw_fixed();
        uint32_t seed = 0;
        bool rolled = false;
        int selected = 0;
        chosen = sandbox::settings();
        draw_world_rows(pixels, seed, rolled, chosen, selected);

        #if KX_DUMP
            int shown = 0;
        #endif

        while(true)
        {
            ++frames;
            int dx = int(bn::keypad::right_pressed()) - int(bn::keypad::left_pressed());
            int dy = int(bn::keypad::down_pressed()) - int(bn::keypad::up_pressed());

            if(dy)
            {
                selected = (selected + dy + sandbox::setting_count + 1) % (sandbox::setting_count + 1);
                draw_world_rows(pixels, seed, rolled, chosen, selected);
            }
            else if(dx && selected == 0)
            {
                seed = hash32((frames * 0x9e3779b9u) ^ seed);
                rolled = true;
                draw_world_rows(pixels, seed, rolled, chosen, selected);
            }
            else if(dx)
            {
                sandbox::setting which = sandbox::setting(selected - 1);
                int count = sandbox::value_count(which);
                uint8_t& value = chosen.values[selected - 1];
                value = uint8_t((value + dx + count) % count);
                draw_world_rows(pixels, seed, rolled, chosen, selected);
            }

            if(bn::keypad::a_pressed() || bn::keypad::start_pressed())
            {
                result = rolled ? seed : hash32(frames * 0x9e3779b9u);
                return true;
            }

            if(replaces && bn::keypad::b_pressed())
            {
                return false;
            }

            if(bn::keypad::select_pressed())
            {
                title_menu(pixels, scene);
                draw_fixed();
                draw_world_rows(pixels, seed, rolled, chosen, selected);
            }

            scene.frame();

            #if KX_DUMP
                // DUMP=1 without a bench: the MENU (Select: its entries, the guide, the settings), then log the
                // screen (a setting picked and changed), then start a world.
                ++shown;

                if(shown == 5 && ! replaces)
                {
                    title_menu(pixels, scene);
                    draw_fixed();
                    draw_world_rows(pixels, seed, rolled, chosen, selected);
                }

                if(shown == 10)
                {
                    selected = 3;
                    chosen.values[2] = 0;
                    draw_world_rows(pixels, seed, rolled, chosen, selected);
                }
                else if(shown == 30)
                {
                    pixels.dump(replaces ? 2 : 0);
                    chosen = sandbox::settings();
                    result = hash32(frames * 0x9e3779b9u);
                    return true;
                }
            #endif

            bn::core::update();
        }
    }

    // "DAY 3 ` 14:20" (the dot glyph), or "NEW SURVIVOR ` DAY 3" after a death.
    void saved_text(char* output, const save::summary& saved)
    {
        int day = (saved.world_minute / (24 * 60)) + 1;
        int minute_of_day = saved.world_minute % (24 * 60);
        int length = 0;

        if(! saved.has_survivor)
        {
            length = ui::append(output, length, "NEW SURVIVOR ` ");
        }

        timeline::date_text(output + length, day);

        while(output[length])
        {
            ++length;
        }

        if(saved.has_survivor)
        {
            length = ui::append(output, length, " ` ");
            output[length++] = char('0' + (minute_of_day / 600));
            output[length++] = char('0' + ((minute_of_day / 60) % 10));
            output[length++] = ':';
            output[length++] = char('0' + ((minute_of_day % 60) / 10));
            output[length++] = char('0' + (minute_of_day % 10));
        }

        output[length] = 0;
    }

    void draw_menu(canvas& pixels, const save::summary& saved, int selected)
    {
        pixels.clear();
        pixels.text_centered(center_x, 34, game_name, colour::accent);
        title_tag(pixels);
        pixels.panel(panel_left, panel_top, panel_width, panel_height, colour::fill, colour::border);

        char text[32];
        saved_text(text, saved);
        pixels.text_centered(center_x, panel_top + 8, "CONTINUE", selected == 0 ? colour::accent : colour::text);
        pixels.text_centered(center_x, panel_top + 17, text, colour::quiet);
        pixels.text_centered(center_x, panel_top + 31, "NEW WORLD", selected == 1 ? colour::accent : colour::text);
        pixels.text_centered(center_x, panel_top + 45, "MENU", selected == 2 ? colour::accent : colour::text);

        constexpr footer_hint hints[] = { { "A", "OK" }, { "D-PAD", "CHOOSE" } };
        footer(pixels, footer_y, hints, 2);
    }
}

title_choice title_screen()
{
    music::title();

    canvas pixels;
    title_choice result;
    uint32_t frames = 0;

    save::summary saved = save::read_summary();

    // The title shows at once; the world behind it is built (well under a second) and fades in.
    pixels.text_centered(center_x, 34, game_name, colour::accent);
    title_tag(pixels);
    bn::core::update();
    backdrop scene(saved);

    if(! saved.valid)
    {
        while(! new_world(pixels, scene, false, frames, result.seed, result.sandbox))
        {
        }

        return result;
    }

    int selected = 0;
    draw_menu(pixels, saved, selected);

    while(true)
    {
        ++frames;
        int dy = int(bn::keypad::down_pressed()) - int(bn::keypad::up_pressed());
        bool confirm = bn::keypad::a_pressed() || bn::keypad::start_pressed();

        #if KX_DUMP
            // DUMP=1 with a save: log the menu, then NEW WORLD chosen, then its screen.
            if(frames == 30)
            {
                pixels.dump(0);
                dy = 1;
            }
            else if(frames == 40)
            {
                pixels.dump(1);
                confirm = true;
            }
        #endif

        if(dy)
        {
            constexpr int items = 3;    // CONTINUE, NEW WORLD, MENU
            selected = (selected + dy + items) % items;
            draw_menu(pixels, saved, selected);
        }

        if(confirm)
        {
            if(selected == 0)
            {
                result.seed = saved.seed;
                result.load = true;
                result.new_survivor = ! saved.has_survivor;
                return result;
            }

            if(selected == 2)
            {
                title_menu(pixels, scene, &saved);
                draw_menu(pixels, saved, selected);
                scene.frame();
                bn::core::update();
                continue;
            }

            scene.frame();
            bn::core::update();

            // The old save stays until the new world's first save replaces it (backing out of character
            // creation keeps it).
            if(new_world(pixels, scene, true, frames, result.seed, result.sandbox))
            {
                return result;
            }

            draw_menu(pixels, saved, selected);
        }

        scene.frame();
        bn::core::update();
    }
}

}
