#ifndef KX_MENU_SCREEN_H
#define KX_MENU_SCREEN_H

#include <cstdint>

#include "kx_bench.h"
#include "kx_sandbox.h"

namespace kx
{

class canvas;
class game;
struct input_frame;

// Start menu, MENU tab (playtest 3, mockup 20; it was SETTINGS, playtest 2, mockup 18; the game is paused): four
// entries, SETTINGS, GUIDE, WORLD and SAVE AND QUIT (playtest 4); A opens one, B goes back to them (and closes the
// menu from them). SAVE AND QUIT is held: A held for a second on it (a bar fills under the row) asks the game to save
// and quit to the title (it was Select held on any tab).
// - SETTINGS: the game's settings (kx_settings.h) on the left, the chosen one's values and what it does on the right.
//   Up/Down choose, Left/Right change, at once.
// - GUIDE: how to play (tools/assets/guide.py), the topics on the left, the chosen topic's page on the right.
//   Up/Down choose a topic, Left/Right turn its pages.
// - WORLD (playtest 4, from the Map tab's margin): the world's seed and sandbox settings, read-only, on the left;
//   the chosen one's values and what its value means on the right. Up/Down choose.
// The title screen shows the same entries, rows and pages (the static draw and handle functions).
class menu_screen
{

public:
    enum class page : uint8_t
    {
        ENTRIES,
        SETTINGS,
        GUIDE,
        WORLD
    };

    // SETTINGS, GUIDE, WORLD, SAVE AND QUIT; the title shows the first three (two when there is no world yet).
    static constexpr int entry_count = 4;
    static constexpr int title_entry_count = 3;

    [[nodiscard]] bool active() const
    {
        return _active;
    }

    // Redraws the whole screen on the next update (a popup drawn over it has gone).
    void refresh()
    {
        _dirty = true;
    }

    void open(game& owner);

    // SAVE AND QUIT was held long enough (save::quit_hold_frames; the game saves and goes back to the title).
    [[nodiscard]] bool quit_requested() const;
    void update(game& owner, const input_frame& input);
    void close(game& owner);

    #if KX_DUMP
        // For the dumps: a page shown with a topic or setting chosen.
        void show(page value, int choice, int guide_page = 0);
    #endif

    // Rows 12 px apart in a panel `width` wide from `left`, the first row's text at `top`.
    static constexpr int row_step = 12;

    // The first `count` entries, each with an arrow.
    static void draw_entries(canvas& pixels, int left, int top, int width, int selected, int count = entry_count);

    // What an entry opens, in lines of at most 12 characters (null-terminated list of 4).
    static constexpr int entry_help_lines = 4;
    [[nodiscard]] static const char* const* entry_help(int entry);

    [[nodiscard]] static const char* entry_name(int entry);

    // The settings as rows.
    static void draw_settings(canvas& pixels, int left, int top, int width, int selected);

    // Up/Down (dy) choose a setting, Left/Right (dx) change it; true if anything changed.
    static bool handle_settings(int& selected, int dx, int dy);

    // The guide's two panels (topics, then the page) from `top`, 120 px tall, across the screen.
    static void draw_guide(canvas& pixels, int top, int topic, int guide_page);

    // Up/Down (dy) choose a topic (on its first page), Left/Right (dx) turn its pages; true if anything changed.
    static bool handle_guide(int& topic, int& guide_page, int dx, int dy);

    // The WORLD page's two panels from `top`, 120 px tall: the seed and a world's settings, one chosen.
    static void draw_world(canvas& pixels, int top, uint32_t seed, const sandbox::settings& world, int selected);

    // Up/Down (dy) choose a setting; true if it changed.
    static bool handle_world(int& selected, int dy);

private:
    page _page = page::ENTRIES;
    int _entry = 0;
    int _setting = 0;
    int _topic = 0;
    int _guide_page = 0;
    int _world_row = 0;
    int _quit_hold = 0;
    bool _active = false;
    bool _dirty = false;

    void _draw(game& owner);
};

}

#endif
