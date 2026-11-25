#ifndef KX_LIGHT_H
#define KX_LIGHT_H

#include "bn_camera_ptr.h"
#include "bn_fixed_point.h"
#include "bn_optional.h"
#include "bn_sprite_ptr.h"

#include "kx_items.h"
#include "kx_tuning.h"

namespace kx
{

// Night (milestone 12.5, docs/milestone-12.5-plan.md). The world layers and the world's sprites fade towards black
// with the hardware fade (16 steps); the HUD stays bright. Inside a light window (a window sprite, or the rectangle
// over a lit building) the fade is switched off, so lit things show at full brightness.
enum class light_param : int
{
    NIGHT_STEPS,
    FOG_STEPS,
    DUSK_START,
    DAWN_END,
    RAMP_MINUTES,
    BATTERY_MINUTES,
    FOUND_CHARGE_MIN,
    CAMPFIRE_NOISE,
    LIT_SIGHT_PX,
    LIT_CHASE_SIGHT_PX,
    COUNT
};

inline constexpr tuning_entry light_tuning_defaults[] = {
    tune_int("Night darkness /16", 10, 0, 16),
    tune_int("Fog at night +/16", 2, 0, 6),
    tune_int("Dusk starts (min)", 19 * 60, 0, (24 * 60) - 10, 10),
    tune_int("Dawn ends (min)", 7 * 60, 0, (24 * 60) - 10, 10),
    tune_int("Dusk, dawn minutes", 60, 10, 240, 10),
    tune_int("Battery (minutes)", 360, 10, 2000, 10),           // plan: about 6 in-game hours of light
    tune_int("Found charge min %", 20, 0, 100, 5),              // found flashlights: this much up to full
    tune_int("Campfire noise tiles", 12, 0, 60),                // each night hour, as if this loud (hordes)
    tune_int("Lit sight px", 96, 0, 320, 8),                    // a lit player is seen from 6 tiles at night
    tune_int("Lit chase sight px", 160, 0, 320, 8),             // and kept in sight this far
};

static_assert(sizeof(light_tuning_defaults) / sizeof(tuning_entry) == int(light_param::COUNT));

[[nodiscard]] tuning_set& light_tuning();

[[nodiscard]] inline int tuning(light_param param)
{
    return light_tuning().get(int(param)).integer();
}

namespace light
{
    // The hardware fade's steps: 0 is daylight, 16 black.
    constexpr int full_steps = 16;

    // How dark the world is at this minute of the day: ramps up over the dusk minutes, holds through the night,
    // ramps down to the dawn's end. Fog adds steps at night, in proportion to the darkness.
    // shift_minutes: dusk falls this much later and dawn ends this much sooner (Eagle-eyed, Short-sighted).
    [[nodiscard]] int darkness(int minute_of_day, bool fog, int shift_minutes = 0);

    // Whether the world's sprites take part in the night fade. Set once a frame, before the sprites update, from
    // the same darkness the world renderer gets, so both change in the same frame.
    [[nodiscard]] bool sprites_dark();
    void set_sprites_dark(bool dark);

    // What the lights cover this frame, for the world's sprites (playtest 3). The light windows switch the fade off
    // for the ground; a sprite under them is left out of the fade by the code instead (blending off), because not
    // every emulator applies a window's rule to a faded sprite (on the Miyoo the player stayed dark in a lit room).
    // Cleared and refilled each frame by the game (the lit room) and light_sprites (the glows and beams), in world
    // pixels.
    void clear_lit();
    void add_lit_rect(int left, int top, int right, int bottom);
    void add_lit_disc(const bn::fixed_point& centre, int radius);
    void add_lit_beam(const bn::fixed_point& apex, int angle, int percent);

    // Whether a light covers this point (a sprite's centre).
    [[nodiscard]] bool lit(const bn::fixed_point& position);

    // Whether any light was on this frame or the one before: shading kept between frames is then redone every frame.
    [[nodiscard]] bool lights_on();

    // A world sprite (the player, zombies, vehicles, particles, rain): darkened at night unless a light covers it.
    // UI sprites (edge arrows, the build cursor, menu text) and light sources (flames) are left alone. A sprite with
    // no camera (rain, the smoke marker) is placed on the screen, not in the world, and isn't tested.
    inline void shade(bn::sprite_ptr& sprite)
    {
        bool dark = sprites_dark() && ! (sprite.camera() && lit(sprite.position()));

        if(sprite.blending_enabled() != dark)
        {
            sprite.set_blending_enabled(dark);
        }
    }

    // A flashlight keeps its state in its item's extra field: bit 15 on, the rest the battery's charge in in-game
    // minutes. Items are saved whole, so it needs no save block of its own.
    constexpr uint16_t flashlight_on_bit = 0x8000;

    [[nodiscard]] inline bool flashlight_on(const item_stack& stack)
    {
        return stack.extra & flashlight_on_bit;
    }

    [[nodiscard]] inline int flashlight_charge(const item_stack& stack)
    {
        return stack.extra & ~flashlight_on_bit;
    }

    inline void set_flashlight(item_stack& stack, bool on, int charge)
    {
        stack.extra = uint16_t((on ? flashlight_on_bit : 0) | (charge & ~flashlight_on_bit));
    }

    // The charge in eighths, for the hotbar's bar (a flashlight with any charge left shows at least one).
    [[nodiscard]] int flashlight_eighths(const item_stack& stack);

    // Degrees counterclockwise from east, as the hardware turns sprites, for a facing of (-1..1, -1..1).
    [[nodiscard]] int facing_angle(int facing_x, int facing_y);
}

// What lights the night this frame (all off by day).
struct light_view
{
    static constexpr int max_fires = 2;

    bool flashlight = false;        // a beam from the apex at the angle (degrees counterclockwise from east)
    bn::fixed_point flashlight_apex;
    int flashlight_angle = 0;
    int flashlight_percent = 100;   // its reach (Eagle-eyed 125, Short-sighted 75)
    bn::fixed_point holder;         // and a small glow around whoever holds it
    bool headlights = false;        // a car's beam
    bn::fixed_point headlight_apex;
    int headlight_angle = 0;
    int fires = 0;                  // round glows on burning campfires
    bn::fixed_point fire[max_fires];
};

// The light windows: window sprites that switch the night fade off where they cover the screen. The beams are one
// frame drawn at double size and turned by the hardware (an affine matrix each).
class light_sprites
{

public:
    explicit light_sprites(const bn::camera_ptr& camera);

    void update(const light_view& value);

    // Whether any light window is up.
    [[nodiscard]] bool active() const
    {
        return _flashlight || _flashlight_far || _holder || _headlights || _fires[0] || _fires[1];
    }

private:
    bn::camera_ptr _camera;
    bn::optional<bn::sprite_ptr> _flashlight;
    bn::optional<bn::sprite_ptr> _flashlight_far;   // beyond the 128 px one frame can reach (Eagle-eyed)
    bn::optional<bn::sprite_ptr> _holder;
    bn::optional<bn::sprite_ptr> _headlights;
    bn::optional<bn::sprite_ptr> _fires[light_view::max_fires];
    int _flashlight_angle = 0;      // eased towards the facing, so turning sweeps the beam round

    void _beam(bn::optional<bn::sprite_ptr>& sprite, bool on, const bn::fixed_point& apex, int angle,
               int percent = 100);
    void _glow(bn::optional<bn::sprite_ptr>& sprite, bool on, const bn::fixed_point& centre, int frame);
};

}

#endif
