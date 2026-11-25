#include "kx_zombies.h"

#include "bn_math.h"

#include "kx_world.h"

namespace kx
{

namespace
{
    constexpr int half_width = 4;
    constexpr int height = 5;
    constexpr int player_clearance_x = 6;
    constexpr int player_clearance_y = 4;

    [[nodiscard]] bool blocked(bn::fixed x, bn::fixed y)
    {
        int left = (x - half_width).floor_integer();
        int right = (x + half_width).ceil_integer() - 1;
        int top = (y - height).floor_integer();
        int bottom = y.ceil_integer() - 1;
        return world::box_blocked(left, top, right, bottom);
    }

    // A body on the ground: as wide as its lying sprite.
    constexpr int lying_half_width = 7;
    constexpr int lying_height = 6;
    constexpr int lying_nudge_px = 3;

    [[nodiscard]] bool lying_blocked(bn::fixed x, bn::fixed y)
    {
        int left = (x - lying_half_width).floor_integer();
        int right = (x + lying_half_width).ceil_integer() - 1;
        int top = (y - lying_height).floor_integer();
        int bottom = y.ceil_integer() - 1;
        return world::box_blocked(left, top, right, bottom);
    }

    // Moves by delta one axis at a time; returns the new position.
    [[nodiscard]] bn::fixed_point slide(const bn::fixed_point& position, const bn::fixed_point& delta)
    {
        bn::fixed x = position.x();
        bn::fixed y = position.y();

        if(delta.x() != 0 && ! blocked(x + delta.x(), y))
        {
            x += delta.x();
        }

        if(delta.y() != 0 && ! blocked(x, y + delta.y()))
        {
            y += delta.y();
        }

        return bn::fixed_point(x, y);
    }
}

void zombie_manager::_move_all(const bn::fixed_point& player_feet, int interval)
{
    bn::fixed friction = tuning(zombie_param::KNOCKBACK_FRICTION);
    bn::fixed separation = tuning(zombie_param::SEPARATION_PX);
    bn::fixed push = bn::fixed(1) / 2;
    int count = _zombies.size();

    for(int index = 0; index < count; ++index)
    {
        zombie& value = _zombies[index];

        if(value.state == zombie_state::GRAB)
        {
            continue;
        }

        if(value.velocity != bn::fixed_point())
        {
            value.position = slide(value.position, value.velocity);
        }

        bool easing = value.state == zombie_state::DYING && value.timer <= dying_ease_frames;

        if(easing)
        {
            // The last frames of dying: onto the corpse decal's spot in this tile (where game::add_decal puts it),
            // the body centred across the tile with its feet 10 px down.
            value.knock = bn::fixed_point();
            int tile_x = value.position.x().floor_integer() >> world::tile_shift;
            int tile_y = (value.position.y().floor_integer() - 1) >> world::tile_shift;
            bn::fixed_point anchor((tile_x << world::tile_shift) + 8, (tile_y << world::tile_shift) + 10);
            value.position = value.timer <= 0 ? anchor :
                             value.position + ((anchor - value.position) / (value.timer + 1));
        }
        else if(value.knock != bn::fixed_point())
        {
            value.position = slide(value.position, value.knock);
            value.knock *= friction;

            if(bn::abs(value.knock.x()) < bn::fixed(1) / 16 && bn::abs(value.knock.y()) < bn::fixed(1) / 16)
            {
                value.knock = bn::fixed_point();
            }
        }

        // A body knocked down or dying beside a wall shifts off it, so its sprite doesn't lie over the wall.
        if(! easing && (value.state == zombie_state::DOWN || value.state == zombie_state::DYING) &&
                lying_blocked(value.position.x(), value.position.y()))
        {
            for(int step = 1; step <= lying_nudge_px; ++step)
            {
                bn::fixed x = value.position.x();
                bn::fixed y = value.position.y();

                if(! lying_blocked(x + step, y) && ! blocked(x + step, y))
                {
                    value.position.set_x(x + 1);
                    break;
                }

                if(! lying_blocked(x - step, y) && ! blocked(x - step, y))
                {
                    value.position.set_x(x - 1);
                    break;
                }
            }
        }

        // Standing zombies never overlap the player's feet.
        if(value.state != zombie_state::DYING && value.state != zombie_state::DOWN &&
                value.state != zombie_state::CRAWL)
        {
            bn::fixed dx = value.position.x() - player_feet.x();
            bn::fixed dy = value.position.y() - player_feet.y();

            if(bn::abs(dx) < player_clearance_x && bn::abs(dy) < player_clearance_y)
            {
                value.position = slide(value.position, bn::fixed_point(dx >= 0 ? push : -push, 0));
            }
        }
    }

    if(separation == 0)
    {
        return;
    }

    // Separation is staggered like decisions: each zombie pushes its neighbours every `interval` frames.
    for(int first = 0; first < count; ++first)
    {
        zombie& a = _zombies[first];

        if((_frame + first) % interval != 0 || a.state == zombie_state::GRAB || a.state == zombie_state::DYING)
        {
            continue;
        }

        for(int second = 0; second < count; ++second)
        {
            if(second == first)
            {
                continue;
            }

            zombie& b = _zombies[second];
            bn::fixed dx = b.position.x() - a.position.x();
            bn::fixed dy = b.position.y() - a.position.y();

            if(bn::abs(dx) >= separation || bn::abs(dy) * 2 >= separation ||
                    b.state == zombie_state::GRAB || b.state == zombie_state::DYING)
            {
                continue;
            }

            bn::fixed_point offset = bn::abs(dx) >= bn::abs(dy) ?
                        bn::fixed_point(dx >= 0 ? push : -push, 0) : bn::fixed_point(0, dy >= 0 ? push : -push);
            a.position = slide(a.position, bn::fixed_point() - offset);
            b.position = slide(b.position, offset);
        }
    }
}

}
