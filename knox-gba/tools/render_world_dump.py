"""Renders the world dump of the travel bench (make BENCH=7): the macro map and the 3x3 chunk cache around the
player at the end of the route, to check world generation without a screen.

    mgba-sdl -C logToFile=1 -C logFile=kx.log -l 255 knox_travel.gba
    python tools/render_world_dump.py kx.log out_folder

Writes macro.png (4 px per 64x64-tile cell) and one chunks_N.png per dumped view (the cached chunks at 1 px per
pixel, all three layers composed; missing chunks stay black). View 0 is where the route ended, the others are around
the first settlements.
"""
import os
import re
import sys

from PIL import Image

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'assets'))

from world import (FLOOR_VARIANTS, ROOF_VARIANTS, W_TRANS, WALL_VARIANTS, WORLD_PALETTE,  # noqa: E402
                   WORLD_PALETTE_DECALS, flip_kind, paint_kind, variant_bank, world_graphics)

CELLS = 64
CHUNK_TILES = 32

# Macro map colours (land, then zone and roads on top), like the debug MAP page.
LAND_COLOURS = [(74, 144, 217), (127, 176, 224), (127, 208, 138), (90, 160, 100), (42, 80, 50), (168, 176, 188)]
ZONE_COLOURS = {1: (176, 42, 32), 2: (232, 197, 71), 3: (224, 144, 58), 4: (224, 90, 58)}
ROAD_COLOUR = (232, 228, 216)


def render_macro(rows, scale=4):
    image = Image.new('RGB', (CELLS * scale, CELLS * scale))
    pixels = image.load()
    for cell_y, values in rows.items():
        for cell_x, value in enumerate(values):
            land, zone, roads = value & 7, (value >> 3) & 7, (value >> 8) & 15
            colour = ZONE_COLOURS.get(zone, ROAD_COLOUR if roads else LAND_COLOURS[min(land, 5)])
            for y in range(scale):
                for x in range(scale):
                    pixels[cell_x * scale + x, cell_y * scale + y] = colour
    return image


def cell_bits(x, y):
    """kx_world_stream.bn_iwram.cpp: the bits deciding a cell's random flips."""
    value = ((x * 0x9e3779b1) ^ (y * 0x85ebca77)) & 0xFFFFFFFF
    return value ^ (value >> 15)


def render_chunks(slots, layers):
    """Draws the chunks the way the streaming does: building colour banks and random flips included."""
    gfx, _, _ = world_graphics()
    palettes = [WORLD_PALETTE, WORLD_PALETTE_DECALS] + [variant_bank(v) for v in
                                                       ROOF_VARIANTS + WALL_VARIANTS + FLOOR_VARIANTS]
    flips = [flip_kind(name) for name, _, _ in gfx]
    paints = [paint_kind(name) if bank == 0 else 0 for name, _, bank in gfx]
    first_banks = [2, 2 + len(ROOF_VARIANTS), 2 + len(ROOF_VARIANTS) + len(WALL_VARIANTS)]
    chunks = list(slots.values())
    min_x = min(x for x, _ in chunks)
    min_y = min(y for _, y in chunks)
    max_x = max(x for x, _ in chunks)
    max_y = max(y for _, y in chunks)
    size = CHUNK_TILES * 16
    image = Image.new('RGB', ((max_x - min_x + 1) * size, (max_y - min_y + 1) * size))
    pixels = image.load()

    for slot, (chunk_x, chunk_y) in slots.items():
        left = (chunk_x - min_x) * size
        top = (chunk_y - min_y) * size
        styles = layers[slot][3]
        for layer in range(3):
            for row, values in layers[slot][layer].items():
                for column, graphic in enumerate(values):
                    _, grid, bank = gfx[graphic]
                    paint = paints[graphic]
                    if paint and row in styles:
                        variant = (styles[row][column] >> ((paint - 1) * 2)) & 3
                        if variant:
                            bank = first_banks[paint - 1] + variant - 1
                    palette = palettes[bank]
                    tile_x = (chunk_x * CHUNK_TILES) + column
                    tile_y = (chunk_y * CHUNK_TILES) + row
                    mirror = flips[graphic] == 3 and cell_bits(tile_x, 0x5eed) & (1 << 20)
                    for quarter_y in range(2):
                        for quarter_x in range(2):
                            bits = cell_bits(tile_x * 2 + quarter_x, tile_y * 2 + quarter_y)
                            flip_x = (flips[graphic] in (1, 2) and bits & (1 << 10)) or mirror
                            flip_y = flips[graphic] == 2 and bits & (1 << 11)
                            source_x = 1 - quarter_x if mirror else quarter_x
                            for y in range(8):
                                for x in range(8):
                                    sx = 7 - x if flip_x else x
                                    sy = 7 - y if flip_y else y
                                    index = grid[quarter_y * 8 + sy][source_x * 8 + sx]
                                    if index != W_TRANS:
                                        pixels[left + column * 16 + quarter_x * 8 + x,
                                               top + row * 16 + quarter_y * 8 + y] = palette[index]
    return image


def main():
    log, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    macro = {}
    views = {}
    view = 0

    with open(log, encoding='utf-8', errors='replace') as f:
        for line in f:
            m = re.search(r'KXMACRO (\d+) (\d+) ([0-9A-F]+)', line)
            if m:
                cell_y, part, data = int(m.group(1)), int(m.group(2)), m.group(3)
                row = macro.setdefault(cell_y, [0] * CELLS)
                for index in range(16):
                    row[part * 16 + index] = int(data[index * 4:(index + 1) * 4], 16)
                continue
            m = re.search(r'KXCHUNK view (\d+)', line)
            if m:
                view = int(m.group(1))
                views[view] = ({}, {})
                continue
            m = re.search(r'KXCHUNK slot (\d+) (\d+) (\d+)', line)
            if m:
                views[view][0][int(m.group(1))] = (int(m.group(2)), int(m.group(3)))
                continue
            m = re.search(r'KXCHUNK (\d+) (\d+) (\d+) ([0-9A-F]+)', line)
            if m:
                slot, layer, row, data = int(m.group(1)), int(m.group(2)), int(m.group(3)), m.group(4)
                views[view][1].setdefault(slot, [{}, {}, {}, {}])[layer][row] = \
                    [int(data[i * 2:i * 2 + 2], 16) for i in range(CHUNK_TILES)]

    if macro:
        render_macro(macro).save(os.path.join(out, 'macro.png'))
    for number, (slots, layers) in views.items():
        if slots:
            render_chunks(slots, layers).save(os.path.join(out, 'chunks_%d.png' % number))
    print('macro rows', len(macro), 'views', {number: len(slots) for number, (slots, _) in views.items()})


if __name__ == '__main__':
    main()
