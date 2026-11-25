#ifndef KX_VECTOR_H
#define KX_VECTOR_H

#include "bn_algorithm.h"
#include "bn_fixed_point.h"
#include "bn_math.h"

#include "kx_tuning.h"

namespace kx
{

// Octagonal length estimate: exact on the axes and diagonals, within 8% elsewhere. No square roots.
[[nodiscard]] inline bn::fixed approx_length(bn::fixed x, bn::fixed y)
{
    bn::fixed ax = bn::abs(x);
    bn::fixed ay = bn::abs(y);
    return bn::max(ax, ay) + (bn::min(ax, ay) * milli(414));
}

[[nodiscard]] inline bn::fixed approx_length(const bn::fixed_point& value)
{
    return approx_length(value.x(), value.y());
}

// Smallest divisor that is safe for bn::fixed / bn::fixed, which divides the divisor by 64 first (half
// precision): anything under 1/64 becomes a division by zero.
[[nodiscard]] constexpr bn::fixed min_divisor()
{
    return bn::fixed::from_data(256);   // 1/16
}

// Unit vector; vectors shorter than min_divisor() count as zero.
[[nodiscard]] inline bn::fixed_point normalized(const bn::fixed_point& value)
{
    bn::fixed length = approx_length(value);

    if(length < min_divisor())
    {
        return bn::fixed_point();
    }

    return bn::fixed_point(value.x() / length, value.y() / length);
}

// Unit vector of an 8-way direction (-1, 0, 1 per axis).
[[nodiscard]] inline bn::fixed_point direction_unit(int dx, int dy)
{
    bn::fixed scale = dx && dy ? milli(707) : bn::fixed(1);
    return bn::fixed_point(scale * dx, scale * dy);
}

[[nodiscard]] inline bn::fixed dot(const bn::fixed_point& a, const bn::fixed_point& b)
{
    return (a.x() * b.x()) + (a.y() * b.y());
}

// Sprite z order for depth sorting by feet position: 8 px screen bands, so the number of Butano sort layers
// stays small (BN_CFG_SPRITES_MAX_SORT_LAYERS). Lower z draws in front.
[[nodiscard]] inline int depth_z(int world_y, int camera_top)
{
    int screen_y = bn::clamp(world_y - camera_top, -16, 175);
    return -(screen_y >> 3);
}

// True if `offset` lies within a cone of `arc_degrees` around the unit `direction`.
[[nodiscard]] inline bool in_arc(const bn::fixed_point& direction, const bn::fixed_point& offset, int arc_degrees)
{
    if(arc_degrees >= 360)
    {
        return true;
    }

    bn::fixed length = approx_length(offset);

    if(length == 0)
    {
        return true;
    }

    return dot(direction, offset) >= bn::degrees_lut_cos(arc_degrees / 2) * length;
}

}

#endif
