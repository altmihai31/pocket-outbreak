"""Playtest 1 mockup: more zombie looks without more palettes. Drawn from the game's own zombie frames and outfit
colours, saved at 3x.

    python tools/mockup_zombie_looks.py      -> mockups/16-zombie-looks.png

The GBA has 16 sprite palettes and the game already uses most of them (the player, 5 zombie outfits, the hit flash,
effects, weapons, light, animals, vehicles...), so more outfits as palettes would run out. Each zombie palette has
four unused colours (indices 12-15), though. The plan: three more bodies drawn into the zombie sheet that use those
four for a second hair colour and a second shirt per outfit. Five outfits x four bodies = 20 looks from the same 5
palettes, and each can also face either way when walking up or down.

A. Today's body: short dark hair, the outfit's shirt.
B. Long hair (second hair colour), the outfit's second shirt.
C. Bald, the shirt torn open: skin and blood through it, one sleeve gone.
D. A beanie in the outfit's second colour, the outfit's shirt.
"""
import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, 'assets'))

from mockup_character import ACCENT, QUIET, TEXT, Screen  # noqa: E402
from mockup_doors import GRIDS, caption  # noqa: E402
import characters as ch  # noqa: E402
from common import hexc  # noqa: E402
import world  # noqa: E402

ROOT = os.path.join(HERE, '..', 'mockups')
TILE = 16

# Spare zombie palette indices: second hair colour, second shirt (base, shade, light).
Z_HAIR2, Z_SHIRT2, Z_SHIRT2_SHADE, Z_SHIRT2_LIGHT = 12, 13, 14, 15
KEYS = dict(ch.ZOMBIE_KEYS, H=Z_HAIR2, J=Z_SHIRT2, Q=Z_SHIRT2_SHADE, L=Z_SHIRT2_LIGHT)
SECOND = {   # per outfit: second hair, second shirt (base, shade, light)
    'CIVILIAN': ('c8b878', '8a6a3a', '5e4828', 'a8885a'),
    'POLICE': ('9a9a96', '7aa6c8', '4f7896', 'a8c8e0'),
    'SOLDIER': ('6b4a2e', '8a7a52', '5e5236', 'a89a6a'),
    'DOCTOR': ('c8c8c0', '5f8a78', '3f6a5a', '7fa898'),
    'FARMER': ('a07040', '3d5a80', '2a3f5e', '5a7aa0'),
}


def palette(outfit):
    name, areas = next(o for o in ch.ZOMBIE_OUTFITS if o[0] == outfit)
    colours = [hexc(areas[i]) if i in areas else c for i, c in enumerate(ch.ZOMBIE_PALETTE)]
    for index, value in zip((Z_HAIR2, Z_SHIRT2, Z_SHIRT2_SHADE, Z_SHIRT2_LIGHT), SECOND[outfit]):
        colours[index] = hexc(value)
    return colours


def swap(rows, table, y0=0, y1=16):
    return [r.translate(str.maketrans(table)) if y0 <= y < y1 else r for y, r in enumerate(rows)]


def body(top, facing, kind):
    """The zombified upper body (12 rows) for a body kind."""
    rows = ch.zombify(top, facing)
    if kind == 'B':
        rows = swap(rows, {'c': 'H', 'h': 'H'}, 0, 7)
        rows = swap(rows, {'j': 'J', 'd': 'Q'}, 7, 12)
        if facing in ('down', 'side'):
            # Hair falling past the face.
            for y in (4, 5, 6):
                row = list(rows[y])
                left = row.index('o')
                right = len(row) - 1 - row[::-1].index('o')
                if facing == 'down':
                    row[left] = 'H'
                    row[left - 1] = 'o'
                row[right] = 'H'
                if right + 1 < 16:
                    row[right + 1] = 'o'
                rows[y] = ''.join(row)
        else:
            rows = swap(rows, {'s': 'H'}, 3, 7)
    elif kind == 'C':
        rows = swap(rows, {'c': 's', 'h': 'S'}, 0, 7)
        torn = {'down': [(5, 8), (6, 8), (6, 9), (7, 9), (8, 9), (6, 10), (7, 10), (9, 10), (10, 9), (10, 10)],
                'up': [(6, 9), (7, 9), (8, 10), (9, 10), (9, 9)],
                'side': [(6, 8), (7, 9), (8, 9), (6, 10), (7, 10), (8, 10)]}[facing]
        for x, y in torn:
            row = list(rows[y])
            if row[x] in 'jdk':
                row[x] = 'S' if (x + y) % 2 else 's'
            rows[y] = ''.join(row)
        row = list(rows[9])
        row[min(i for i, c in enumerate(row) if c in 'jd')] = 's'     # a bare arm
        rows[9] = ''.join(row)
    elif kind == 'D':
        rows = swap(rows, {'c': 'J'}, 0, 3)
        row = list(rows[3])
        row = [('Q' if c in 'hsc' else c) for c in row]     # the turned-up brim
        rows[3] = ''.join(row)
    return rows


def frames(kind):
    out = {}
    for facing, top, legs in (('down', ch.DOWN_TOP, ch.FRONT_LEGS), ('up', ch.UP_TOP, ch.FRONT_LEGS),
                              ('side', ch.SIDE_TOP, ch.SIDE_LEGS)):
        upper = body(top, facing, kind)
        for pose in ('stand', 'step_a', 'step_b', 'lunge'):
            leg = pose if pose in legs else 'step_a'
            out[facing, pose] = ch.parse_frame(ch.zombie_arms(facing, upper + legs[leg], pose), KEYS)
    return out


FRAMES = {kind: frames(kind) for kind in 'ABCD'}


def draw(s, kind, outfit, facing, pose, feet_x, feet_y, mirror=False, scale=1):
    grid = FRAMES[kind][facing, pose]
    colours = palette(outfit)
    for y in range(16):
        for x in range(16):
            value = grid[y][15 - x if mirror else x]
            if value:
                for dy in range(scale):
                    for dx in range(scale):
                        px = feet_x - 8 * scale + x * scale + dx
                        py = feet_y - 16 * scale + y * scale + dy
                        if 0 <= px < 240 and 0 <= py < 160:
                            s.pixels[px, py] = colours[value]


def put(s, name, tx, ty):
    grid = GRIDS[name]
    for y in range(TILE):
        for x in range(TILE):
            value = grid[y][x]
            if value:
                s.pixels[tx * TILE + x, ty * TILE + y] = world.WORLD_PALETTE[value]


def street(s):
    for ty in range(10):
        for tx in range(15):
            if ty in (4, 5, 6):
                put(s, 'ROAD_LINE_H' if ty == 5 and tx % 2 else 'ROAD', tx, ty)
            elif ty in (3, 7):
                put(s, 'PAVEMENT', tx, ty)
            else:
                put(s, 'GRASS_A' if (tx * 7 + ty * 3) % 5 else 'GRASS_B', tx, ty)


# A crowd crossing the street: (x, y, facing, pose, mirror, outfit, body)
CROWD = [
    (30, 70, 'side', 'step_a', False, 'CIVILIAN', 'B'), (52, 62, 'down', 'stand', False, 'CIVILIAN', 'C'),
    (70, 84, 'side', 'step_b', True, 'FARMER', 'A'), (92, 74, 'down', 'step_a', True, 'CIVILIAN', 'D'),
    (112, 96, 'up', 'step_b', False, 'POLICE', 'B'), (130, 66, 'side', 'lunge', False, 'CIVILIAN', 'A'),
    (150, 90, 'down', 'step_b', False, 'DOCTOR', 'C'), (170, 76, 'side', 'stand', True, 'CIVILIAN', 'B'),
    (190, 100, 'down', 'stand', True, 'FARMER', 'D'), (208, 70, 'side', 'step_a', True, 'CIVILIAN', 'C'),
    (40, 112, 'up', 'stand', True, 'SOLDIER', 'D'), (88, 120, 'down', 'step_b', False, 'CIVILIAN', 'A'),
    (140, 124, 'side', 'step_b', False, 'CIVILIAN', 'B'), (196, 128, 'up', 'step_a', True, 'POLICE', 'C'),
]


def crowd(varied):
    s = Screen()
    street(s)
    for x, y, facing, pose, mirror, outfit, kind in sorted(CROWD, key=lambda z: z[1]):
        if not varied:
            kind, mirror = 'A', mirror and facing == 'side'
        draw(s, kind, outfit, facing, pose, x, y, mirror)
    return s


def lineup():
    s = Screen()
    outfits = [name for name, _ in ch.ZOMBIE_OUTFITS]
    s.text(4, 4, 'BODY', QUIET)
    for column, outfit in enumerate(outfits):
        s.text_centered(54 + column * 42, 4, outfit, TEXT)
    for row, kind in enumerate('ABCD'):
        y = 44 + row * 36
        s.text(6, y - 18, kind, ACCENT)
        for column, outfit in enumerate(outfits):
            draw(s, kind, outfit, 'down', 'stand', 46 + column * 42, y, scale=2)
            draw(s, kind, outfit, 'side', 'stand', 66 + column * 42, y, scale=1)
    return s


def main():
    panels = [
        (crowd(False), [('1 NOW: ONE BODY, 5 OUTFITS', ACCENT), ('MOST OF A TOWN LOOKS ALIKE', QUIET)]),
        (crowd(True), [('2 FOUR BODIES, SAME 5 PALETTES', ACCENT), ('AND UP/DOWN WALKERS MIRRORED', QUIET)]),
        (lineup(), [('3 THE 20 LOOKS: BODIES A-D', ACCENT), ('B: LONG HAIR, 2ND SHIRT. C: TORN. D: BEANIE', QUIET)]),
    ]
    gap = 4
    width = 240 * 3 + gap * 2
    height = 160 + 22
    sheet = Image.new('RGB', (width, height), (0x05, 0x07, 0x0b))
    for index, (screen, lines) in enumerate(panels):
        x = index * (240 + gap)
        sheet.paste(screen.image, (x, 0))
        sheet.paste(caption(240, lines), (x, 160))
    sheet = sheet.resize((sheet.width * 3, sheet.height * 3), Image.NEAREST)
    sheet.save(os.path.join(ROOT, '16-zombie-looks.png'))
    print('wrote mockups/16-zombie-looks.png')


if __name__ == '__main__':
    main()
