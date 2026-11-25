#ifndef KX_FLASH_H
#define KX_FLASH_H

#include "bn_color.h"
#include "bn_sprite_palette_item.h"

namespace kx
{

// The reserved all-white OBJ palette for the 2-frame hit flash (spec: Hardware budget > Palettes).
constexpr bn::color flash_colors[16] = {
    bn::color(31, 0, 31), bn::color(31, 31, 31), bn::color(31, 31, 31), bn::color(31, 31, 31),
    bn::color(31, 31, 31), bn::color(31, 31, 31), bn::color(31, 31, 31), bn::color(31, 31, 31),
    bn::color(31, 31, 31), bn::color(31, 31, 31), bn::color(31, 31, 31), bn::color(31, 31, 31),
    bn::color(31, 31, 31), bn::color(31, 31, 31), bn::color(31, 31, 31), bn::color(31, 31, 31),
};

constexpr bn::sprite_palette_item flash_palette_item(flash_colors, bn::bpp_mode::BPP_4);

}

#endif
