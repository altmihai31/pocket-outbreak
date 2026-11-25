#ifndef KX_WORLD_RENDERER_H
#define KX_WORLD_RENDERER_H

#include "bn_bg_palette_ptr.h"
#include "bn_camera_ptr.h"
#include "bn_fixed.h"
#include "bn_optional.h"
#include "bn_regular_bg_ptr.h"
#include "bn_regular_bg_map_ptr.h"
#include "bn_vector.h"

#include "kx_world.h"

namespace kx
{

// What the world's display effects follow, given once a frame (world_renderer::update_effects).
struct world_effects
{
    bool inside = false;            // the building the player is in, world pixels
    int left = 0;
    int top = 0;
    int width = 0;
    int height = 0;
    bn::fixed haze;                 // fog: the white haze's strength (day only)
    int darkness = 0;               // night: fade steps of 16 (kx_light), 0 by day
    bool flash = false;             // a lightning flash: the night lifts for a moment
    bool lit_inside = false;        // night: the building's lights are on
};

// Streams the world into three 256x256 px wraparound maps (BG3 ground, BG2 walls, BG1 canopy): one 2 KB screenblock
// each (milestone 14; they were 512x512 before, and the 18 KB saved went to a tileset twice the size). The screen
// shows at most 31x21 hardware tiles, so a 32x32 map always has a spare row and column to write the next ones into.
// Only the hardware-tile rows and columns that scroll into view are written, straight into VRAM.
class world_renderer
{

public:
    static constexpr int map_cells = 32;                  // 32x32 hardware tiles = 256x256 px per layer

    explicit world_renderer(const bn::camera_ptr& camera);

    // camera_left / camera_top: world pixel at the screen's top-left corner.
    void update(int camera_left, int camera_top);

    void set_visible(bool visible);

    // Menu screens (playtest 1, mockup 03): the world shows behind their panels, faded towards black. The world's
    // own palette fades, so the HUD layer and the night, fog and roof blending are untouched.
    void set_dimmed(bool dimmed);

    // Roof, fog and night, once a frame. The GBA blends one way at a time, so:
    // - By day the roof fades (transparency) and fog hazes the world white (fade), never both at once.
    // - At night the world fades to black. The roof of the building the player is in is not drawn (its cells are
    //   written empty) instead of fading, fog adds darkness instead of haze, and a window over a lit building and
    //   the light window sprites switch the fade off.
    void update_effects(const world_effects& value);

    // Night state, for the night bench's log.
    [[nodiscard]] bool night() const
    {
        return _night;
    }

    [[nodiscard]] bool roof_hidden() const
    {
        return _hide_right > _hide_left;
    }

    [[nodiscard]] bool room_lit() const
    {
        return _room_lit;
    }

    [[nodiscard]] bool cutaway() const
    {
        return _cutaway;
    }

    [[nodiscard]] int roof_level() const
    {
        return _roof_level;
    }

    // Seasons: grass, leaves and the forest floor turn autumn brown, then snow white (percent each).
    void set_season(int autumn_percent, int winter_percent);

    // Title backdrop: every layer pixelated by the hardware mosaic and darkened by the blending fade (the HUD
    // layer stays sharp). Global effects: the caller resets them (mosaic stretch, fade alpha) when done.
    void set_backdrop(bn::fixed mosaic_stretch);

    // Redraws one world tile (after a decal) if it is inside the streamed window.
    void refresh_tile(int tile_x, int tile_y);

    ~world_renderer();

    #if KX_BENCH
        // Benches (milestone 14): the visible cells of every layer worked out again and compared with VRAM; the
        // number that differ (0 when scrolling and the effects left nothing stale).
        [[nodiscard]] int verify() const;
    #endif

private:
    struct layer_bg
    {
        bn::regular_bg_map_ptr map;
        bn::regular_bg_ptr bg;
    };

    bn::vector<layer_bg, int(world::layer::COUNT)> _layers;
    bn::optional<bn::bg_palette_ptr> _palette;
    bool _hazed = false;
    int _season_key = -1;
    bn::camera_ptr _camera;
    int _roof_level;                // fade_frames: roof shown, 0: gone
    int _roof_left = 0;
    int _roof_top = 0;
    int _roof_right = 0;
    int _roof_bottom = 0;
    bool _cutaway = false;          // window and blending set up
    bool _night = false;            // fading to black
    bool _room_lit = false;         // the window over the lit building is up
    int _hide_left = 0;             // night: the roof cells drawn empty (world cells, right and bottom exclusive)
    int _hide_top = 0;
    int _hide_right = 0;
    int _hide_bottom = 0;
    int _cell_x = 0;
    int _cell_y = 0;
    bool _drawn = false;

    // Spec: entering a building fades its roof (BG1) out over 8 frames, and leaving fades it back. A window over
    // the building limits the fade to its roof, so the tree canopies on the same layer stay.
    void _update_roof(bool inside, int left, int top, int width, int height);
    void _begin_cutaway();

    // Fog: the world layers faded towards white (blending), off while a roof is cut away (indoors).
    void _update_haze(bn::fixed alpha);

    void _update_night(const world_effects& value);
    void _end_night();
    void _hide_roof(int left, int top, int right, int bottom);
    void _draw_cells(int cell_x, int cell_y, int columns, int rows, int only_layer = -1);
    void _redraw_rect(int left, int top, int right, int bottom, int only_layer);
    void _end_cutaway();
};

}

#endif
