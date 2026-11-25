#include "kx_horde_edges.h"

#include "bn_sprite_items_fx.h"

#include "kx_game.h"
#include "kx_horde_tuning.h"
#include "kx_sfx.h"

namespace kx
{

namespace
{
    // The arrow frames follow the 4 fx kinds (3 frames each): right, up-right, up.
    constexpr int arrow_right = 12;
    constexpr int arrow_diagonal = 13;
    constexpr int arrow_up = 14;
    constexpr int arrow_bg_priority = 1;        // over roofs, under the HUD and menus

    static_assert(bn::sprite_items::fx.tiles_item().graphics_count() > arrow_up);

    struct placement
    {
        int x;
        int y;
        int frame;
        bool flip_x;
        bool flip_y;
        int pan;        // screen x the groan is panned to
    };

    // East first, counter-clockwise (kx::hordes::edge_count).
    constexpr placement placements[hordes::edge_directions] = {
        { horde::edge_x, 0, arrow_right, false, false, 150 },
        { horde::edge_x, -horde::edge_y, arrow_diagonal, false, false, 110 },
        { 0, -horde::edge_y, arrow_up, false, false, 0 },
        { -horde::edge_x, -horde::edge_y, arrow_diagonal, true, false, -110 },
        { -horde::edge_x, 0, arrow_right, true, false, -150 },
        { -horde::edge_x, horde::edge_y, arrow_diagonal, true, true, -110 },
        { 0, horde::edge_y, arrow_up, false, true, 0 },
        { horde::edge_x, horde::edge_y, arrow_diagonal, false, true, 110 },
    };
}

void horde_edges::update(game& owner)
{
    ++_frame;

    int loudest = -1;
    int loudest_count = 0;

    for(int direction = 0; direction < hordes::edge_directions; ++direction)
    {
        int count = hordes::edge_count(direction);
        bn::optional<bn::sprite_ptr>& arrow = _arrows[direction];

        if(count < horde::edge_min)
        {
            if(arrow)
            {
                arrow->set_visible(false);
            }

            continue;
        }

        if(! arrow)
        {
            const placement& value = placements[direction];
            arrow = bn::sprite_items::fx.create_sprite(value.x, value.y, value.frame);
            arrow->set_horizontal_flip(value.flip_x);
            arrow->set_vertical_flip(value.flip_y);
            arrow->set_bg_priority(arrow_bg_priority);
        }

        // Pulses: shown for the first half of each period; bigger hordes pulse faster.
        int big = bn::min(count, horde::edge_big);
        int period = horde::edge_pulse_frames - ((horde::edge_pulse_frames * 3 / 4) * big / horde::edge_big);
        arrow->set_visible((_frame % period) < (period / 2) + 2);

        if(count > loudest_count)
        {
            loudest = direction;
            loudest_count = count;
        }
    }

    // Distant groans from the biggest waiting crowd (lost on the hard of hearing).
    if(loudest < 0 || sfx::hearing_percent() < 100)
    {
        _groan_timer = 0;
        return;
    }

    if(--_groan_timer <= 0)
    {
        _groan_timer = horde::groan_min_frames + owner.random().get_int(horde::groan_extra_frames);
        bn::fixed volume = bn::fixed(bn::min(loudest_count, horde::edge_big)) / (horde::edge_big * 3) + bn::fixed(1) / 8;
        sfx::play(sfx::id::GROAN, volume, placements[loudest].pan);
    }
}

void horde_edges::hide()
{
    for(bn::optional<bn::sprite_ptr>& arrow : _arrows)
    {
        arrow.reset();
    }

    _groan_timer = 0;
}

}
