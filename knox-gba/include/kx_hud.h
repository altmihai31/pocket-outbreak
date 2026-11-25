#ifndef KX_HUD_H
#define KX_HUD_H

#include "kx_canvas.h"

namespace kx
{

class player;

// Gameplay HUD on BG0 (no sprites, spec: Controls and UI), laid out like mockup 01: clock top-left, noise meter
// top-right, moodles bottom-left, hotbar and equipped item with its condition bottom-right, the context prompt
// above its target, plus toasts, the eating and grab meters and an optional status / CPU box under the clock.
// Each element redraws only when what it shows changes; elements that overlap are redrawn together.
// Full-screen screens (search, gear, health, death) draw on the same canvas, mostly directly through pixels().
class hud
{

public:
    static constexpr int columns = canvas::columns;
    static constexpr int rows = canvas::rows;

    // Build mode (mockup 06): a line of coloured segments in the bar's info area.
    struct build_line
    {
        static constexpr int max_segments = 4;

        char text[max_segments][24] = {};
        uint8_t colour[max_segments] = {};
        uint8_t count = 0;
    };

    // What build mode shows: the piece bar, the info lines, the hold time and the safehouse count.
    struct build_view
    {
        int selected = 0;
        int seconds = 0;
        bool holding = false;
        int used = -1;              // pieces in the safehouse at the cursor; -1 for a new one
        const char* limit_name = nullptr;   // used counts these (plots, generators, traps) of limit instead
        int limit = 0;
        char title[24] = {};
        build_line lines[2];
        int key = 0;                // changes when anything shown does
    };

    hud();

    // Build mode's bar and boxes replace the clock, noise meter, moodles and hotbar; nullptr gives them back.
    void set_build(const build_view* view)
    {
        _build = view;
    }

    // Two short lines above the moodles while driving ("FUEL 12 L", "CAR 85%"); nullptr hides them.
    void set_drive(const char* line1, const char* line2);

    void update(const player& target, int world_minute);

    void set_perf_visible(bool visible);

    [[nodiscard]] bool perf_visible() const
    {
        return _perf_visible;
    }

    // Full-screen panel behind the paused debug menu (sprite text goes on top).
    void set_menu_backdrop(bool enabled);

    // A notification (wounds, broken weapons, what you ate); bank is a hud_tiles text style. Playtest 4 (mockup 22):
    // all of them show in one place, in the world and over every menu: centred just above the hotbar, up to 3
    // stacked (the newest at the bottom, a fourth pushes out the oldest), each on its own timer; the same text again
    // while it shows only restarts its timer.
    void toast(const char* text, int bank, int frames);

    // A boxed message of up to 3 lines below the toasts (radio broadcasts); line2 and line3 may be nullptr.
    void message(const char* line1, const char* line2, const char* line3, int frames);

    // Context prompt ("[A] SEARCH") centred above a screen pixel; center_x < 0 hides it.
    void set_prompt(int center_x, int target_top_y, const char* text);

    // The reload bar or the JAMMED box over the player's head (mockup 11), at screen pixels; center_x < 0 hides it.
    void set_gun_box(int center_x, int head_y);

    // An action's progress bar in the same place (milestone 12.7: forcing or picking a lock): filled of length;
    // length 0 hides it. It wins over the gun box.
    void set_work_bar(int center_x, int head_y, int filled, int length);

    void death_screen(const char* cause, int kills, int minutes_survived, int distance_tiles);

    // ----- full-screen screens -----

    // Takes the layer over (gameplay HUD hidden, background filled) or gives it back.
    void begin_screen();

    // A menu screen (or the debug menu) covers the HUD.
    [[nodiscard]] bool screen_open() const
    {
        return _backdrop;
    }
    void end_screen();

    [[nodiscard]] canvas& pixels()
    {
        return _canvas;
    }

    // Cell-based helpers (8 px cells; bank is a hud_tiles text style). A box's 1 px border runs through the
    // middle of its edge cells; text is the small font, `width` cells are cleared first.
    void box(int column0, int row0, int column1, int row1, int bank);
    void text(int column, int row, const char* value, int width, int bank);

    // Start menu tab bar (mockup 04; 0 Gear, 1 Health, ...).
    void tab_bar(int selected);

    // Footer button hints at pixel row y: button in the text colour, action quiet (mockup 04).
    struct hint
    {
        const char* button;
        const char* action;
    };

    void hints(int y, const hint* items, int count, bool centered);

    // Menu screens cover the HUD, so their notifications are drawn over the screen in the same place as in the
    // world, counting down only here; call once a frame after the screen's update. True when the screen must
    // redraw under them (one has gone, or a new one moved the others).
    [[nodiscard]] bool screen_toast();

    // A 16x16 item slot at pixel (x, y), framed in the accent colour when selected, and its labels: the stack
    // count bottom-right ("x2", "14"), a hotbar number top-left.
    void slot(int x, int y, item_icon icon, bool selected);
    void slot_count(int x, int y, int value);
    void slot_corner(int x, int y, const char* text, bool accent);

    [[nodiscard]] static int style_colour(int bank);

private:
    enum element
    {
        CLOCK,
        NOISE,
        STATUS,
        MOODLES,
        HOTBAR,
        TOAST,
        EATING,
        GRAB,
        PROMPT,
        MESSAGE,
        BUILD_HOLD,
        BUILD_SAFEHOUSE,
        BUILD_BAR,
        DRIVE,
        GUN,
        ELEMENTS
    };

    const build_view* _build = nullptr;
    static constexpr int max_message_length = 40;
    char _message[3][max_message_length + 1] = {};
    int _message_lines = 0;
    int _message_timer = 0;
    int _message_serial = 0;

    canvas _canvas;
    area _drawn[ELEMENTS];
    area _next[ELEMENTS];
    int _keys[ELEMENTS] = {};
    bool _dirty[ELEMENTS] = {};
    const player* _target = nullptr;
    int _minute = 0;
    static constexpr int max_toast_length = 32;
    static constexpr int max_toasts = 3;
    static constexpr int max_prompt_length = 14;

    struct toast_line
    {
        char text[max_toast_length + 1] = {};
        int bank = 0;
        int timer = 0;
        int serial = 0;
    };

    toast_line _toasts[max_toasts];         // oldest first
    int _toast_count = 0;
    char _prompt[max_prompt_length + 1] = {};
    char _drive[2][16] = {};
    bool _driving = false;
    int _toast_serial = 0;
    int _screen_toast_key = -1;             // the notifications drawn over a menu screen, -1 for none
    area _screen_toast_area;                // and where
    int _prompt_x = -1;
    int _prompt_y = 0;
    int _gun_x = -1;
    int _gun_y = 0;
    int _work_x = -1;
    int _work_y = 0;
    int _work_fill = 0;             // of reload_track pixels
    int _cpu = 0;
    int _peak_cpu = 0;
    int _shown_peak = 0;
    int _peak_window = 0;
    int _perf_counter = 0;
    bool _perf_visible = KX_DEBUG;          // the tester build starts with the CPU readout on
    int _warmup = 60;                       // frames before the CPU readout counts peaks
    bool _backdrop = false;

    void _reset();
    [[nodiscard]] int _key(element value) const;
    [[nodiscard]] area _layout(element value) const;
    void _draw(element value);
    void _draw_clock();
    void _draw_noise();
    void _draw_status();
    void _draw_moodles();
    void _draw_hotbar();
    void _draw_gun();
    void _draw_toast();
    void _draw_toasts();
    void _tick_toasts();
    [[nodiscard]] int _toast_key() const;
    [[nodiscard]] area _toast_area() const;
    void _draw_eating();
    void _draw_grab();
    void _draw_prompt();
    void _draw_message();
    void _draw_build_hold();
    void _draw_build_safehouse();
    void _draw_build_bar();
    void _draw_drive();
};

}

#endif
