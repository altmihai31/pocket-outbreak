#include "kx_hud.h"

#include "bn_algorithm.h"
#include "bn_core.h"

#include "kx_light.h"
#include "kx_base.h"
#include "kx_hud_tiles.h"
#include "kx_inventory.h"
#include "kx_survival.h"
#include "kx_timeline.h"
#include "kx_player.h"
#include "kx_save.h"
#include "kx_ui.h"

namespace kx
{

namespace
{
    // Mockup 01 layout in screen pixels.
    constexpr int margin = 4;
    constexpr int small_box_height = 11;            // 1 px border, 2 px padding, 5 px text
    constexpr int small_box_padding = 3;
    constexpr int status_top = margin + small_box_height + 3;
    constexpr int status_height = 3 + (canvas::line_height * 2) + 1;
    // Notifications (playtest 4, mockup 22): the newest box's bottom, stacked upwards 2 px apart.
    constexpr int toast_bottom = 116;
    constexpr int toast_gap = 2;
    constexpr int drive_top = canvas::height - margin - 14 - 6 - status_height;     // above the moodle row
    constexpr int message_top = 34;
    constexpr int meter_box_top = 40;
    constexpr int meter_width = 32;
    constexpr int meter_height = 4;
    constexpr int prompt_height = 9;
    constexpr int prompt_gap = 3;
    constexpr int prompt_min_top = status_top;
    constexpr int prompt_max_top = 120;

    // Build mode (mockup 06).
    constexpr int build_bar_top = 128;
    constexpr int build_slots_left = 4;
    constexpr int build_slot_step = 24;
    constexpr int build_slots_top = 133;
    constexpr int build_labels_top = 152;
    constexpr int build_info_left = 126;
    constexpr int build_visible = 5;

    constexpr int slot_size = 14;
    constexpr int slot_step = 16;
    constexpr int slots_top = canvas::height - margin - slot_size;
    constexpr int max_moodles = 6;                  // spec: at most 6 moodles show at once
    constexpr int moodle_pip_height = 2;
    constexpr int moodle_pip_step = (slot_size - 2) / 3;     // 4 pips, 2 px wide, spanning the slot width
    constexpr int hotbar_left = 152;
    constexpr int equipped_left = hotbar_left + (inventory::hotbar_slots * slot_step) + 2;
    constexpr int equipped_size = 18;
    constexpr int equipped_top = canvas::height - margin - equipped_size - 3;
    constexpr int condition_top = equipped_top + equipped_size + 1;
    constexpr int condition_height = 2;
    constexpr int low_condition_eighths = 2;
    constexpr int ammo_box_height = 9;             // a gun's rounds over the equipped slot (mockup 11)

    // Mockup 11: the reload bar over the player's head (a track, what has filled, the green sweet spot, the
    // moving mark), or the JAMMED box.
    constexpr int reload_box_width = 44;
    constexpr int gun_box_height = 9;
    constexpr int gun_box_gap = 3;              // above the head
    constexpr int reload_track = reload_box_width - 6;
    constexpr int charge_height = 2;               // a flashlight's battery under its hotbar slot

    // Noise meter (mockups 01, 02): lit bars for radii at or above each threshold (tiles: crouch 1, walk 2,
    // jog 4, sprint 6, then loud noises), bar heights in pixels, lit colours.
    constexpr int noise_bar_thresholds[] = { 1, 2, 4, 6, 10 };
    constexpr int noise_bar_heights[] = { 2, 3, 4, 5, 6 };
    constexpr int noise_bar_colours[] = { colour::accent, colour::accent, colour::accent, colour::ui_red,
                                          colour::ui_red };
    constexpr int noise_bars = 5;
    constexpr int noise_bars_width = (noise_bars * 3) - 1;

    // The flashlight the hotbar shows: the one held in the off hand, else the first in the bag.
    [[nodiscard]] const item_stack* flashlight_stack(const inventory& items)
    {
        const item_stack& held = items.equipped(equip_slot::OFF_HAND);

        if(held.id == item_id::FLASHLIGHT)
        {
            return &held;
        }

        int slot = items.find(item_id::FLASHLIGHT);
        return slot >= 0 ? &items.slot(slot) : nullptr;
    }

    constexpr int perf_refresh_frames = 30;
    constexpr int perf_peak_frames = 60;
    constexpr int unset_key = -0x7FFFFFFF;

    // Moodle level 1-4 colours (docs/palette.md).
    constexpr int moodle_level_colours[] = { colour::accent, colour::moodle_orange, colour::moodle_red,
                                             colour::moodle_dark_red };

    [[nodiscard]] int lit_bars(int noise_radius)
    {
        int result = 0;

        for(int threshold : noise_bar_thresholds)
        {
            if(noise_radius >= threshold)
            {
                ++result;
            }
        }

        return result;
    }

    [[nodiscard]] int border_colour(int bank)
    {
        return bank == hud_tiles::bank_accent ? colour::accent :
               bank == hud_tiles::bank_danger ? colour::ui_red : colour::border;
    }

    void clock_text(char* output, int world_minute)
    {
        // "DAY 3 18:40", then "NIGHT 3 23:10"; the hours after midnight still belong to that night.
        int minute_of_day = world_minute % (24 * 60);
        int day = (world_minute / (24 * 60)) + 1;
        bool night = is_night(minute_of_day);

        if(night && minute_of_day < day_start_hour * 60)
        {
            day = bn::max(day - 1, 1);
        }

        // The date (spec: the world starts on 1 July): "3 JUL ` 18:40", "NIGHT 3 JUL ` 23:10".
        int length = ui::append(output, 0, night ? "NIGHT " : "");
        timeline::date_text(output + length, day);

        while(output[length])
        {
            ++length;
        }

        length = ui::append(output, length, " ` ");
        output[length++] = char('0' + (minute_of_day / 600));
        output[length++] = char('0' + ((minute_of_day / 60) % 10));
        output[length++] = ':';
        output[length++] = char('0' + ((minute_of_day % 60) / 10));
        output[length++] = char('0' + (minute_of_day % 10));
        output[length] = 0;
    }

    // Second line of the status box: CPU now and the peak over the last second.
    void status_line(char* output, int cpu, int peak)
    {
        int length = ui::append(output, 0, "CPU ");
        length = ui::append_int(output, length, cpu);
        length = ui::append(output, length, "% PEAK ");
        length = ui::append_int(output, length, bn::max(peak, cpu));
        ui::append(output, length, "%");
    }

    // Small box: 1 px border around 2 px padding and one text line; returns the text's left x.
    int small_box(canvas& pixels, const area& region, int border)
    {
        pixels.panel(region.x, region.y, region.width, region.height, colour::fill, border);
        return region.x + 1 + small_box_padding;
    }

    // Build mode's top boxes: "HOLD A ` 6 S", and "SAFEHOUSE ` 14 / 64" or "NEW SAFEHOUSE" (each box is as wide as
    // its text).
    void build_hold_text(char* output, int seconds)
    {
        int length = ui::append(output, 0, "HOLD A ` ");
        length = ui::append_int(output, length, seconds);
        ui::append(output, length, " S");
    }

    void build_safehouse_text(char* output, int used, const char* limit_name, int limit)
    {
        if(limit_name)
        {
            int length = ui::append(output, 0, limit_name);
            length = ui::append(output, length, " ` ");
            length = ui::append_int(output, length, used);
            length = ui::append(output, length, " / ");
            ui::append_int(output, length, limit);
            return;
        }

        if(used < 0)
        {
            ui::append(output, 0, "NEW SAFEHOUSE");
            return;
        }

        int length = ui::append(output, 0, "SAFEHOUSE ` ");
        length = ui::append_int(output, length, used);
        length = ui::append(output, length, " / ");
        ui::append_int(output, length, base_tuning::max_pieces);
    }

    void meter(canvas& pixels, int x, int y, int eighths, int fill_colour)
    {
        pixels.rect(x, y, meter_width, meter_height, colour::empty);
        pixels.rect(x, y, (bn::clamp(eighths, 0, 8) * meter_width) / 8, meter_height, fill_colour);
    }
}

int hud::style_colour(int bank)
{
    return bank == hud_tiles::bank_accent ? colour::accent :
           bank == hud_tiles::bank_quiet ? colour::quiet :
           bank == hud_tiles::bank_danger ? colour::ui_red : colour::text;
}

hud::hud()
{
    _reset();
}

void hud::_reset()
{
    for(int index = 0; index < ELEMENTS; ++index)
    {
        _drawn[index] = area();
        _keys[index] = unset_key;
        _dirty[index] = false;
    }
}

void hud::update(const player& target, int world_minute)
{
    _target = &target;
    _minute = world_minute;

    // CPU readout: last frame and the peak over the last second.
    int cpu = (bn::core::last_cpu_usage() * 100).round_integer();

    // The first second after a game starts includes building the world: not a peak worth showing.
    if(_warmup > 0)
    {
        --_warmup;
        cpu = 0;
    }

    _peak_cpu = bn::max(_peak_cpu, cpu);

    if(++_peak_window >= perf_peak_frames)
    {
        _peak_window = 0;
        _shown_peak = _peak_cpu;
        _peak_cpu = 0;
    }

    if(++_perf_counter >= perf_refresh_frames)
    {
        _perf_counter = 0;
        _cpu = cpu;
    }

    if(_message_timer > 0)
    {
        --_message_timer;
    }

    // Behind a menu screen the notifications count down in screen_toast(), where they can be seen (playtest 1).
    if(! _backdrop)
    {
        _tick_toasts();
    }

    if(_backdrop)
    {
        return;
    }

    bool any = false;

    for(int index = 0; index < ELEMENTS; ++index)
    {
        if(_key(element(index)) != _keys[index])
        {
            _dirty[index] = true;
            any = true;
        }
    }

    if(! any)
    {
        return;
    }

    for(int index = 0; index < ELEMENTS; ++index)
    {
        _next[index] = _dirty[index] ? _layout(element(index)) : _drawn[index];
    }

    // An element that changes is erased and redrawn; anything it touches (before or after) is redrawn too.
    for(bool changed = true; changed;)
    {
        changed = false;

        for(int index = 0; index < ELEMENTS; ++index)
        {
            if(! _dirty[index])
            {
                continue;
            }

            for(int other = 0; other < ELEMENTS; ++other)
            {
                if(! _dirty[other] && (_drawn[other].intersects(_drawn[index]) ||
                                       _drawn[other].intersects(_next[index])))
                {
                    _dirty[other] = true;
                    changed = true;
                }
            }
        }
    }

    for(int index = 0; index < ELEMENTS; ++index)
    {
        if(_dirty[index])
        {
            _canvas.erase(_drawn[index]);
        }
    }

    for(int index = 0; index < ELEMENTS; ++index)
    {
        if(_dirty[index])
        {
            _dirty[index] = false;
            _keys[index] = _key(element(index));
            _drawn[index] = _next[index];

            if(! _drawn[index].empty())
            {
                _draw(element(index));
            }
        }
    }
}

int hud::_key(element value) const
{
    const player& target = *_target;

    // Build mode hides the gameplay HUD's corners and shows its own.
    if(_build && (value == CLOCK || value == NOISE || value == MOODLES || value == HOTBAR || value == PROMPT ||
                  value == STATUS))
    {
        return -3;
    }

    switch(value)
    {

    case BUILD_HOLD:
        return _build ? (_build->seconds * 2) + int(_build->holding) : -1;

    case BUILD_SAFEHOUSE:
        return _build ? _build->used + (_build->limit * 1000) : -2;

    case BUILD_BAR:
        return _build ? _build->key : -1;

    case CLOCK:
        return _minute;

    case NOISE:
        return lit_bars(target.noise_radius());

    case STATUS:
        return _perf_visible ? int(target.mode()) + (int(target.ground()) * 8) + (_cpu * 256) +
                               (bn::max(_shown_peak, _cpu) * 65536) : -1;

    case MOODLES:
    {
        const survival& body = target.body();
        int key = 0;

        for(int index = 0; index < int(moodle::COUNT); ++index)
        {
            key = (key * 5) + body.level(moodle(index));
            key &= 0x3FFFFFFF;
        }

        return key;
    }

    case HOTBAR:
    {
        // Selection, hand item and condition, and the hotbar contents (and whether each is carried).
        const inventory& items = target.items();
        int key = target.selected_slot() + (int(items.equipped(equip_slot::HAND).id) * 4) +
                  (target.condition_eighths() * 1024);

        // A gun: its rounds, the rounds in the bag, and whether it is being reloaded.
        if(target.gun_rounds() >= 0)
        {
            key = (key * 37) + target.gun_rounds() + (target.gun_rounds_carried() * 64) +
                  (target.reloading() ? 1 << 20 : 0) + (target.gun_jammed_now() ? 1 << 21 : 0);
        }

        for(int slot = 0; slot < inventory::hotbar_slots; ++slot)
        {
            item_id id = items.hotbar(slot);
            key = (key * 31) + int(id) + (items.available(id) ? 0 : 7);

            if(const item_stack* found = id == item_id::FLASHLIGHT ? flashlight_stack(items) : nullptr)
            {
                const item_stack& torch = *found;
                key = (key * 19) + (light::flashlight_eighths(torch) * 2) + light::flashlight_on(torch);
            }

            key &= 0x3FFFFFFF;
        }

        return key;
    }

    case TOAST:
        return _toast_key();

    case DRIVE:
    {
        if(! _driving || _build)
        {
            return -1;
        }

        int key = 7;

        for(const char* line : _drive)
        {
            for(const char* c = line; *c; ++c)
            {
                key = (key * 31) + *c;
            }
        }

        return key & 0x7FFFFFFF;
    }

    case MESSAGE:
        return _message_timer > 0 ? _message_serial : -1;

    case EATING:
        return target.eat_progress_eighths();

    case GRAB:
        return target.grabbed() ? (target.grab_meter_eighths() * 16) + target.grab_time_eighths() : -2;

    case GUN:
    {
        if(_work_x >= 0)
        {
            return 1000000000 + (((_work_x * 256) + _work_y) * 64) + _work_fill;
        }

        if(_gun_x < 0)
        {
            return -1;
        }

        int state = target.reloading() ? 1 + ((target.reload_frame() * reload_track) /
                                              bn::max(target.reload_length(), 1)) :
                    target.clearing_jam() ? 100 : 200;
        return (((_gun_x * 256) + _gun_y) * 512) + state;
    }

    case PROMPT:
    {
        if(_prompt_x < 0)
        {
            return -1;
        }

        int key = (_prompt_x * 256) + _prompt_y;

        for(const char* c = _prompt; *c; ++c)
        {
            key = (key * 31) + *c;
        }

        return key & 0x7FFFFFFF;
    }

    default:
        return 0;
    }
}

area hud::_layout(element value) const
{
    const player& target = *_target;

    if(_build && (value == CLOCK || value == NOISE || value == MOODLES || value == HOTBAR || value == PROMPT ||
                  value == STATUS))
    {
        return area();
    }

    switch(value)
    {

    case BUILD_HOLD:
    {
        if(! _build)
        {
            return area();
        }

        char text[24];
        build_hold_text(text, _build->seconds);
        int width = canvas::text_width(text) + ((1 + small_box_padding) * 2);
        return { margin, margin, width, small_box_height };
    }

    case BUILD_SAFEHOUSE:
    {
        if(! _build)
        {
            return area();
        }

        char text[32];
        build_safehouse_text(text, _build->used, _build->limit_name, _build->limit);
        int width = canvas::text_width(text) + ((1 + small_box_padding) * 2);
        return { canvas::width - margin - width, margin, width, small_box_height };
    }

    case BUILD_BAR:
        return _build ? area{ 0, build_bar_top, canvas::width, canvas::height - build_bar_top } : area();

    case CLOCK:
    {
        char text[24];
        clock_text(text, _minute);
        return { margin, margin, canvas::text_width(text) + ((1 + small_box_padding) * 2), small_box_height };
    }

    case NOISE:
    {
        int width = canvas::text_width("NOISE") + 3 + noise_bars_width + ((1 + small_box_padding) * 2);
        return { canvas::width - margin - width, margin, width, small_box_height };
    }

    case STATUS:
    {
        if(! _perf_visible)
        {
            return area();
        }

        char text[32];
        status_line(text, _cpu, _shown_peak);
        int first = canvas::text_width(mode_name(target.mode())) + 4 + canvas::text_width(info(target.ground()).name);
        int width = bn::max(first, canvas::text_width(text)) + ((1 + small_box_padding) * 2);
        return { margin, status_top, width, status_height };
    }

    case MOODLES:
    {
        const survival& body = target.body();
        int count = 0;

        for(int index = 0; index < int(moodle::COUNT); ++index)
        {
            if(body.level(moodle(index)) > 0)
            {
                ++count;
            }
        }

        count = bn::min(count, max_moodles);
        return count ? area{ margin, slots_top, (count * slot_step) - 2, slot_size + 1 + moodle_pip_height } :
                       area();
    }

    case HOTBAR:
    {
        // With a gun in hand the rounds box sits over the equipped slot.
        int top = target.gun_rounds() >= 0 ? equipped_top - ammo_box_height - 1 : equipped_top;
        return { hotbar_left, top, canvas::width - margin - hotbar_left, condition_top + condition_height - top };
    }

    case DRIVE:
    {
        if(! _driving || _build)
        {
            return area();
        }

        int width = bn::max(canvas::text_width(_drive[0]), canvas::text_width(_drive[1])) +
                    ((1 + small_box_padding) * 2);
        return { margin, drive_top, width, status_height };
    }

    case TOAST:
        return _toast_area();

    case MESSAGE:
    {
        if(_message_timer <= 0)
        {
            return area();
        }

        int width = 0;

        for(int line = 0; line < _message_lines; ++line)
        {
            width = bn::max(width, canvas::text_width(_message[line]));
        }

        width += (1 + small_box_padding) * 2;
        return { (canvas::width - width) / 2, message_top, width, 3 + (_message_lines * canvas::line_height) + 1 };
    }

    case EATING:
    {
        if(target.eat_progress_eighths() < 0)
        {
            return area();
        }

        int width = canvas::text_width("DRINKING") + 4 + meter_width + ((1 + small_box_padding) * 2);
        return { (canvas::width - width) / 2, meter_box_top, width, small_box_height };
    }

    case GRAB:
    {
        if(! target.grabbed())
        {
            return area();
        }

        int width = canvas::text_width("MASH A") + 4 + meter_width + ((1 + small_box_padding) * 2);
        return { (canvas::width - width) / 2, meter_box_top, width, 3 + (canvas::line_height * 2) + 1 };
    }

    case GUN:
    {
        if(_work_x >= 0)
        {
            int top = bn::clamp(_work_y - gun_box_gap - gun_box_height, margin, canvas::height - gun_box_height);
            return { bn::clamp(_work_x - (reload_box_width / 2), margin, canvas::width - margin - reload_box_width),
                     top, reload_box_width, gun_box_height };
        }

        if(_gun_x < 0)
        {
            return area();
        }

        int width = target.reloading() ? reload_box_width :
                    target.clearing_jam() ? canvas::text_width("CLEARING") + 6 :
                    canvas::text_width("JAMMED B CLEAR") + 10;
        int top = bn::clamp(_gun_y - gun_box_gap - gun_box_height, margin, canvas::height - gun_box_height);
        return { bn::clamp(_gun_x - (width / 2), margin, canvas::width - margin - width), top, width,
                 gun_box_height };
    }

    case PROMPT:
    {
        if(_prompt_x < 0)
        {
            return area();
        }

        int width = canvas::text_width(_prompt) + 6;
        return { bn::clamp(_prompt_x - (width / 2), margin, canvas::width - margin - width), _prompt_y, width,
                 prompt_height };
    }

    default:
        return area();
    }
}

void hud::_draw(element value)
{
    switch(value)
    {

    case CLOCK:
        _draw_clock();
        break;

    case NOISE:
        _draw_noise();
        break;

    case STATUS:
        _draw_status();
        break;

    case MOODLES:
        _draw_moodles();
        break;

    case HOTBAR:
        _draw_hotbar();
        break;

    case TOAST:
        _draw_toast();
        break;

    case DRIVE:
        _draw_drive();
        break;

    case MESSAGE:
        _draw_message();
        break;

    case EATING:
        _draw_eating();
        break;

    case GRAB:
        _draw_grab();
        break;

    case PROMPT:
        _draw_prompt();
        break;

    case GUN:
        _draw_gun();
        break;

    case BUILD_HOLD:
        _draw_build_hold();
        break;

    case BUILD_SAFEHOUSE:
        _draw_build_safehouse();
        break;

    case BUILD_BAR:
        _draw_build_bar();
        break;

    default:
        break;
    }
}

void hud::_draw_build_hold()
{
    // "HOLD A ` 6 S": the time left to build, in the accent colour while A is held.
    const area& region = _drawn[BUILD_HOLD];
    char text[24];
    build_hold_text(text, _build->seconds);
    int x = small_box(_canvas, region, _build->holding ? colour::accent : colour::border);
    _canvas.text(x, region.y + 3, text, _build->holding ? colour::accent : colour::text);
}

void hud::_draw_build_safehouse()
{
    const area& region = _drawn[BUILD_SAFEHOUSE];
    char text[32];
    build_safehouse_text(text, _build->used, _build->limit_name, _build->limit);
    int x = small_box(_canvas, region, colour::border);
    _canvas.text(x, region.y + 3, text, colour::text);
}

void hud::_draw_build_bar()
{
    // Mockup 06: the pieces with their labels on the left, then what the selected one needs.
    const area& region = _drawn[BUILD_BAR];
    _canvas.rect(region.x, region.y, region.width, region.height, colour::fill);
    _canvas.rect(region.x, region.y, region.width, 1, colour::border);

    // Five pieces show at a time; the bar scrolls to keep the selected one in view, with arrows at the ends.
    int first = bn::clamp(_build->selected - (build_visible / 2), 0, bn::max(piece_kinds - build_visible, 0));

    for(int column = 0; column < build_visible && first + column < piece_kinds; ++column)
    {
        int index = first + column;
        const piece_info& piece = info(piece_kind(index));
        int x = build_slots_left + (column * build_slot_step);
        bool selected = index == _build->selected;
        slot(x, build_slots_top, piece.icon, selected);
        _canvas.text_centered(x + 8, build_labels_top, piece.short_name, selected ? colour::accent : colour::quiet);
    }

    for(int row = 0; row < 5; ++row)
    {
        int reach = row < 3 ? row : 4 - row;

        if(first > 0)
        {
            _canvas.rect(build_slots_left - 3 + (2 - reach), build_slots_top + 5 + row, reach + 1, 1, colour::quiet);
        }

        if(first + build_visible < piece_kinds)
        {
            _canvas.rect(build_info_left - 9, build_slots_top + 5 + row, reach + 1, 1, colour::quiet);
        }
    }

    _canvas.rect(build_info_left - 6, region.y + 4, 1, region.height - 8, colour::border);
    _canvas.text(build_info_left, build_slots_top + 2, _build->title, colour::accent);

    for(int line = 0; line < 2; ++line)
    {
        const build_line& value = _build->lines[line];
        int x = build_info_left;
        int y = build_slots_top + 10 + (line * 8);

        for(int index = 0; index < value.count; ++index)
        {
            if(index)
            {
                x = _canvas.text(x, y, " ` ", colour::quiet);
            }

            x = _canvas.text(x, y, value.text[index], value.colour[index]);
        }
    }
}

void hud::_draw_clock()
{
    const area& region = _drawn[CLOCK];
    char text[24];
    clock_text(text, _minute);
    _canvas.text(small_box(_canvas, region, colour::border), region.y + 3, text, colour::text);
}

void hud::_draw_noise()
{
    const area& region = _drawn[NOISE];
    int x = small_box(_canvas, region, colour::border);
    x = _canvas.text(x, region.y + 3, "NOISE", colour::text) + 3;
    int bars = lit_bars(_target->noise_radius());
    int bottom = region.y + 3 + 5;

    for(int index = 0; index < noise_bars; ++index)
    {
        int height = noise_bar_heights[index];
        _canvas.rect(x + (index * 3), bottom - height, 2, height,
                     index < bars ? noise_bar_colours[index] : colour::empty);
    }
}

void hud::_draw_status()
{
    // Milestone debug aid: movement mode and ground, CPU now and the peak over the last second.
    const area& region = _drawn[STATUS];
    const player& target = *_target;
    int x = small_box(_canvas, region, colour::border);
    int next = _canvas.text(x, region.y + 3, mode_name(target.mode()), colour::text);
    _canvas.text(next + 4, region.y + 3, info(target.ground()).name, colour::quiet);

    char text[32];
    status_line(text, _cpu, _shown_peak);
    _canvas.text(x, region.y + 3 + canvas::line_height, text, colour::text);
}

void hud::set_drive(const char* line1, const char* line2)
{
    _driving = line1 && line2;

    if(_driving)
    {
        ui::append(_drive[0], 0, line1);
        ui::append(_drive[1], 0, line2);
    }
}

void hud::_draw_drive()
{
    // Driving: fuel left, then the car's condition (or why the engine is off).
    const area& region = _drawn[DRIVE];
    int x = small_box(_canvas, region, colour::border);
    _canvas.text(x, region.y + 3, _drive[0], colour::text);
    _canvas.text(x, region.y + 3 + canvas::line_height, _drive[1], colour::quiet);
}

void hud::_draw_moodles()
{
    // Spec: up to 6 moodles in a row bottom-left, only while active, worst to the left. Each slot has the level
    // colour as border, with level pips under it (mockup 01 has them inside; there they cover the 12 px icon).
    const survival& body = _target->body();
    int shown = 0;

    for(int level = 4; level >= 1 && shown < max_moodles; --level)
    {
        for(int index = 0; index < int(moodle::COUNT) && shown < max_moodles; ++index)
        {
            if(body.level(moodle(index)) != level)
            {
                continue;
            }

            int x = margin + (shown * slot_step);
            int level_colour = moodle_level_colours[level - 1];
            _canvas.panel(x, slots_top, slot_size, slot_size, colour::fill, level_colour);
            _canvas.icon(x + 1, slots_top + 1, item_icon(int(item_icon::MOODLE_HUNGRY) + index), 2, 2, 12, 12);

            for(int pip = 0; pip < 4; ++pip)
            {
                _canvas.rect(x + (pip * moodle_pip_step), slots_top + slot_size + 1, 2, moodle_pip_height,
                             pip < level ? level_colour : colour::empty);
            }

            ++shown;
        }
    }
}

void hud::_draw_hotbar()
{
    const player& target = *_target;
    const inventory& items = target.items();

    for(int slot = 0; slot < inventory::hotbar_slots; ++slot)
    {
        int x = hotbar_left + (slot * slot_step);
        item_id id = items.hotbar(slot);
        _canvas.panel(x, slots_top, slot_size, slot_size, colour::panel,
                      slot == target.selected_slot() ? colour::text : colour::border);

        if(id != item_id::NONE && items.available(id))
        {
            _canvas.icon(x + 1, slots_top + 1, info(id).icon, 2, 2, 12, 12);

            // A flashlight's battery under its slot (mockup 02): yellow while it is on.
            if(const item_stack* found = id == item_id::FLASHLIGHT ? flashlight_stack(items) : nullptr)
            {
                const item_stack& torch = *found;
                int eighths = light::flashlight_eighths(torch);
                int colour = eighths <= 1 ? colour::ui_red : light::flashlight_on(torch) ? colour::accent :
                                                                                         colour::quiet;
                _canvas.rect(x, slots_top + slot_size + 1, slot_size, charge_height, colour::empty);
                _canvas.rect(x, slots_top + slot_size + 1, (eighths * slot_size) / 8, charge_height, colour);
            }
        }

        char digit[2] = { char('1' + slot), 0 };
        _canvas.text(x + 2, slots_top + 1, digit, colour::quiet);
    }

    // Equipped item in an accent box, with its condition bar underneath (spec: condition shows on the
    // equipped icon).
    const item_stack& hand = items.equipped(equip_slot::HAND);
    _canvas.panel(equipped_left, equipped_top, equipped_size, equipped_size, colour::panel, colour::accent);
    _canvas.icon(equipped_left + 2, equipped_top + 2, hand.empty() ? item_icon::FISTS : info(hand.id).icon,
                 1, 1, 14, 14);

    if(int rounds = target.gun_rounds(); rounds >= 0)
    {
        // Mockup 11: "12/30" in a small box over the equipped slot: grey while reloading, red when empty.
        char text[12];
        int length = ui::append_int(text, 0, rounds);
        length = ui::append(text, length, "/");
        ui::append_int(text, length, target.gun_rounds_carried());
        int width = canvas::text_width(text) + 6;
        int x = equipped_left + equipped_size - width;
        int y = equipped_top - ammo_box_height - 1;
        _canvas.panel(x, y, width, ammo_box_height, colour::fill, colour::border);
        _canvas.text(x + 3, y + 2, text, target.reloading() ? colour::quiet :
                                         rounds == 0 || target.gun_jammed_now() ? colour::ui_red : colour::text);
    }

    if(! hand.empty() && info(hand.id).durability > 0)
    {
        int eighths = target.condition_eighths();
        _canvas.rect(equipped_left, condition_top, equipped_size, condition_height, colour::empty);
        _canvas.rect(equipped_left, condition_top, (eighths * equipped_size) / 8, condition_height,
                     eighths <= low_condition_eighths ? colour::ui_red : colour::ui_green);
    }
}

void hud::_draw_toast()
{
    _draw_toasts();
}

void hud::_draw_toasts()
{
    // Each box as wide as its text, centred, the newest at the bottom; the border in its kind's colour (normal ones
    // light grey, so they stand out over menu panels).
    for(int index = 0; index < _toast_count; ++index)
    {
        const toast_line& line = _toasts[index];
        int width = canvas::text_width(line.text) + ((1 + small_box_padding) * 2);
        int y = toast_bottom - small_box_height -
                ((_toast_count - 1 - index) * (small_box_height + toast_gap));
        int border = line.bank == hud_tiles::bank_normal ? int(colour::quiet) : border_colour(line.bank);
        int x = small_box(_canvas, area{ (canvas::width - width) / 2, y, width, small_box_height }, border);
        _canvas.text(x, y + 3, line.text, style_colour(line.bank));
    }
}

void hud::_tick_toasts()
{
    int kept = 0;

    for(int index = 0; index < _toast_count; ++index)
    {
        if(--_toasts[index].timer > 0)
        {
            _toasts[kept++] = _toasts[index];
        }
    }

    _toast_count = kept;
}

int hud::_toast_key() const
{
    if(! _toast_count)
    {
        return -1;
    }

    int key = _toast_count;

    for(int index = 0; index < _toast_count; ++index)
    {
        key = (key * 31) + _toasts[index].serial;
    }

    return key & 0x7FFFFFFF;
}

area hud::_toast_area() const
{
    int width = 0;

    for(int index = 0; index < _toast_count; ++index)
    {
        width = bn::max(width, canvas::text_width(_toasts[index].text));
    }

    if(! _toast_count)
    {
        return area();
    }

    width += (1 + small_box_padding) * 2;
    int height = (_toast_count * small_box_height) + ((_toast_count - 1) * toast_gap);
    return { (canvas::width - width) / 2, toast_bottom - height, width, height };
}

void hud::_draw_message()
{
    const area& region = _drawn[MESSAGE];
    int x = small_box(_canvas, region, colour::accent);

    for(int line = 0; line < _message_lines; ++line)
    {
        _canvas.text(x, region.y + 3 + (line * canvas::line_height), _message[line], colour::text);
    }
}

void hud::message(const char* line1, const char* line2, const char* line3, int frames)
{
    const char* lines[] = { line1, line2, line3 };
    _message_lines = 0;

    for(const char* line : lines)
    {
        if(line)
        {
            int index = 0;

            for(; line[index] && index < max_message_length; ++index)
            {
                _message[_message_lines][index] = line[index];
            }

            _message[_message_lines][index] = 0;
            ++_message_lines;
        }
    }

    _message_timer = frames;
    ++_message_serial;
}

void hud::_draw_eating()
{
    // Visible refill while eating (spec: a short animation and a visible meter refill).
    const area& region = _drawn[EATING];
    int x = small_box(_canvas, region, colour::border);
    _canvas.text(x, region.y + 3, _target->drinking() ? "DRINKING" : "EATING", colour::text);
    meter(_canvas, region.x + region.width - 1 - small_box_padding - meter_width, region.y + 3 + 1,
          _target->eat_progress_eighths(), colour::ui_green);
}

void hud::_draw_grab()
{
    // Struggle box: the meter to fill by mashing A, and the time left before the bite.
    const area& region = _drawn[GRAB];
    const player& target = *_target;
    int x = small_box(_canvas, region, colour::ui_red);
    int meter_x = region.x + region.width - 1 - small_box_padding - meter_width;
    _canvas.text(x, region.y + 3, "MASH A", colour::accent);
    _canvas.text(x, region.y + 3 + canvas::line_height, "BITE", colour::ui_red);
    meter(_canvas, meter_x, region.y + 3 + 1, target.grab_meter_eighths(), colour::ui_green);
    meter(_canvas, meter_x, region.y + 3 + canvas::line_height + 1, target.grab_time_eighths(), colour::ui_red);
}

void hud::_draw_gun()
{
    const area& region = _drawn[GUN];
    const player& target = *_target;

    if(_work_x >= 0)
    {
        // Mockup 12: a plain bar filling, no sweet spot.
        _canvas.panel(region.x, region.y, region.width, region.height, colour::fill, colour::border);
        _canvas.rect(region.x + 3, region.y + 3, reload_track, 3, colour::empty);
        _canvas.rect(region.x + 3, region.y + 3, _work_fill, 3, colour::quiet);
        return;
    }

    if(target.reloading())
    {
        int length = bn::max(target.reload_length(), 1);
        int x = region.x + 3;
        int y = region.y + 3;
        int filled = (target.reload_frame() * reload_track) / length;
        int sweet_x = (target.sweet_start() * reload_track) / length;
        int sweet_width = bn::max((target.sweet_length() * reload_track) / length, 1);
        _canvas.panel(region.x, region.y, region.width, region.height, colour::fill, colour::border);
        _canvas.rect(x, y, reload_track, 3, colour::empty);
        _canvas.rect(x, y, filled, 3, colour::quiet);
        _canvas.rect(x + sweet_x, y, sweet_width, 3, colour::ui_green);
        _canvas.rect(x + bn::min(filled, reload_track - 1), y - 1, 1, 5, colour::text);
        return;
    }

    if(target.clearing_jam())
    {
        _canvas.panel(region.x, region.y, region.width, region.height, colour::fill, colour::border);
        _canvas.text(region.x + 3, region.y + 2, "CLEARING", colour::quiet);
        return;
    }

    // Jammed: "JAMMED  B CLEAR" in a red box.
    _canvas.panel(region.x, region.y, region.width, region.height, colour::fill, colour::ui_red);
    int x = _canvas.text(region.x + 3, region.y + 2, "JAMMED", colour::ui_red) + 6;
    x = _canvas.text(x, region.y + 2, "B", colour::text) + 4;
    _canvas.text(x, region.y + 2, "CLEAR", colour::quiet);
}

void hud::set_gun_box(int center_x, int head_y)
{
    _gun_x = center_x;
    _gun_y = head_y;
}

void hud::set_work_bar(int center_x, int head_y, int filled, int length)
{
    _work_x = length > 0 ? center_x : -1;
    _work_y = head_y;
    _work_fill = length > 0 ? bn::clamp((filled * reload_track) / length, 0, reload_track) : 0;
}

void hud::_draw_prompt()
{
    // Accent box above the target (mockup 01: "[A] SEARCH").
    const area& region = _drawn[PROMPT];
    _canvas.panel(region.x, region.y, region.width, region.height, colour::fill, colour::accent);
    _canvas.text(region.x + 3, region.y + 2, _prompt, colour::accent);
}

void hud::set_perf_visible(bool visible)
{
    _perf_visible = visible;
}

void hud::set_menu_backdrop(bool enabled)
{
    if(enabled)
    {
        _backdrop = true;
        _canvas.rect(0, 0, canvas::width, canvas::height, colour::fill);
        _canvas.frame(3, 3, canvas::width - 6, canvas::height - 6, colour::border);
    }
    else
    {
        end_screen();
    }
}

void hud::toast(const char* text, int bank, int frames)
{
    // The same text while it still shows: its timer starts again, nothing moves.
    for(int index = 0; index < _toast_count; ++index)
    {
        toast_line& line = _toasts[index];
        int at = 0;

        while(at < max_toast_length && text[at] && line.text[at] == text[at])
        {
            ++at;
        }

        if(line.text[at] == (at < max_toast_length ? text[at] : 0) && line.bank == bank)
        {
            line.timer = bn::max(line.timer, frames);
            return;
        }
    }

    // A fourth pushes the oldest out.
    if(_toast_count == max_toasts)
    {
        for(int index = 1; index < max_toasts; ++index)
        {
            _toasts[index - 1] = _toasts[index];
        }

        --_toast_count;
    }

    toast_line& line = _toasts[_toast_count++];
    int index = 0;

    for(; text[index] && index < max_toast_length; ++index)
    {
        line.text[index] = text[index];
    }

    line.text[index] = 0;
    line.bank = bank;
    line.timer = frames;
    line.serial = ++_toast_serial;
}

void hud::set_prompt(int center_x, int target_top_y, const char* text)
{
    if(center_x < 0)
    {
        _prompt_x = -1;
        return;
    }

    int index = 0;

    for(; text[index] && index < max_prompt_length; ++index)
    {
        _prompt[index] = text[index];
    }

    _prompt[index] = 0;
    _prompt_x = center_x;
    _prompt_y = bn::clamp(target_top_y - prompt_gap - prompt_height, prompt_min_top, prompt_max_top);
}

void hud::death_screen(const char* cause, int kills, int minutes_survived, int distance_tiles)
{
    // Spec: "This is how you died": days survived, kills, distance walked, cause.
    set_menu_backdrop(true);
    constexpr int center = canvas::width / 2;
    _canvas.text_centered(center, 34, "THIS IS HOW YOU DIED", colour::quiet);
    _canvas.text_centered(center, 50, cause, colour::ui_red);

    char text[32];
    int length = ui::append(text, 0, "SURVIVED ");
    length = ui::append_int(text, length, minutes_survived / (24 * 60));
    length = ui::append(text, length, " DAYS ");
    length = ui::append_int(text, length, (minutes_survived / 60) % 24);
    ui::append(text, length, " H");
    _canvas.text_centered(center, 74, text, colour::text);

    length = ui::append(text, 0, "KILLS ");
    ui::append_int(text, length, kills);
    _canvas.text_centered(center, 86, text, colour::text);

    // Tiles are about a metre (spec: 4,096 tiles in ~12 minutes of jogging).
    length = ui::append(text, 0, "WALKED ");
    length = ui::append_int(text, length, distance_tiles / 1000);
    text[length++] = '.';
    text[length++] = char('0' + ((distance_tiles / 100) % 10));
    ui::append(text, length, " KM");
    _canvas.text_centered(center, 98, text, colour::text);
    _canvas.text_centered(center, 126, "A: NEW SURVIVOR", colour::accent);
}

void hud::begin_screen()
{
    // See-through: the game dims the world behind (game::_update_menu_dim) and the screen draws its panels.
    _backdrop = true;
    _canvas.clear();
}

void hud::end_screen()
{
    _backdrop = false;
    _screen_toast_key = -1;
    _screen_toast_area = area();
    _canvas.clear();
    _reset();
}

void hud::box(int column0, int row0, int column1, int row1, int bank)
{
    _canvas.panel((column0 * 8) + 3, (row0 * 8) + 3, ((column1 - column0) * 8) + 2, ((row1 - row0) * 8) + 2,
                  colour::fill, border_colour(bank));
}

void hud::text(int column, int row, const char* value, int width, int bank)
{
    _canvas.rect(column * 8, row * 8, width * 8, 8, colour::fill);
    _canvas.text(column * 8, (row * 8) + 2, value, style_colour(bank));
}

void hud::slot(int x, int y, item_icon icon, bool selected)
{
    _canvas.icon(x, y, icon);

    if(selected)
    {
        _canvas.frame(x, y, 16, 16, colour::accent);
    }
}

void hud::tab_bar(int selected)
{
    // Spec: menu tabs Gear, Health, Craft, Skills, Map (L/R to switch); playtest 2 added Settings, which playtest 3
    // made Menu (settings and the guide). Mockup 04: a bordered strip with L1 and R1 at the ends, the current one an
    // accent block; with six tabs they sit 5 px apart (mockup 18).
    constexpr const char* tabs[] = { "GEAR", "HEALTH", "CRAFT", "SKILLS", "MAP", "MENU" };
    constexpr int tab_count = 6;
    constexpr int left = 12;
    constexpr int top = 4;
    constexpr int bar_width = canvas::width - (left * 2);
    constexpr int bar_height = 14;
    constexpr int gap = 5;
    constexpr int block_padding = 3;
    constexpr int text_y = top + 5;

    _canvas.panel(left, top, bar_width, bar_height, colour::fill, colour::border);
    _canvas.text(left + 5, text_y, ui::button::l, colour::quiet);
    _canvas.text_right(left + bar_width - 6, text_y, ui::button::r, colour::quiet);

    // Every tab keeps its place whichever is selected (playtest 1: they used to shift as the block moved).
    int total = (block_padding * 2) + (gap * (tab_count - 1));

    for(const char* name : tabs)
    {
        total += canvas::text_width(name);
    }

    int x = ((canvas::width - total) / 2) + block_padding;

    for(int index = 0; index < tab_count; ++index)
    {
        int width = canvas::text_width(tabs[index]);

        if(index == selected)
        {
            _canvas.rect(x - block_padding, text_y - 1, width + (block_padding * 2), 7, colour::accent);
            _canvas.text(x, text_y, tabs[index], colour::fill);
        }
        else
        {
            _canvas.text(x, text_y, tabs[index], colour::quiet);
        }

        x += width + gap;
    }
}

void hud::slot_count(int x, int y, int value)
{
    // On a dark backing so it reads over the icon (mockup 03).
    char text[8];
    int length = value < 10 ? ui::append(text, 0, "x") : 0;
    ui::append_int(text, length, value);
    int right = x + 14;
    int width = canvas::text_width(text);
    _canvas.rect(right - width, y + 9, width + 2, 7, colour::fill);
    _canvas.text_right(right, y + 10, text, colour::text);
}

void hud::slot_corner(int x, int y, const char* text, bool accent)
{
    _canvas.text(x + 2, y + 2, text, accent ? colour::accent : colour::quiet);
}

bool hud::screen_toast()
{
    // In the same place as in the world (playtest 4). Any change (one gone, one added: the others move up) needs
    // the screen redrawn under them; they are drawn again over it.
    _tick_toasts();
    int key = _toast_key();
    bool changed = _screen_toast_key >= 0 && key != _screen_toast_key;

    // Where they were goes back to the screen's backdrop: not every screen repaints the gaps between its panels.
    if(changed && ! _screen_toast_area.empty())
    {
        _canvas.rect(_screen_toast_area.x, _screen_toast_area.y, _screen_toast_area.width,
                     _screen_toast_area.height, canvas::backdrop);
    }

    _screen_toast_key = key;
    _screen_toast_area = _toast_area();
    _draw_toasts();
    return changed;
}

void hud::hints(int y, const hint* items, int count, bool centered)
{
    constexpr int button_gap = 4;
    constexpr int hint_gap = 10;
    int total = hint_gap * (count - 1);

    for(int index = 0; index < count; ++index)
    {
        total += canvas::text_width(items[index].button) + button_gap + canvas::text_width(items[index].action);
    }

    int x = centered ? (canvas::width - total) / 2 : 12;

    for(int index = 0; index < count; ++index)
    {
        x = _canvas.text(x, y, items[index].button, colour::text) + button_gap;
        x = _canvas.text(x, y, items[index].action, colour::quiet) + hint_gap;
    }
}

}
