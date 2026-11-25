"""Animals plan, stage 1 mockup: the new animals' sprites (boar, wolf, chicken, goat, crow) in the animal palette,
then four scenes, saved at 3x.

    python tools/mockup_animals.py         -> mockups/19-animals.png

The animals come from tools/assets/animal_art.py (the wolf, boar, goat and deer 32x16, the hen and rabbit 16x16); crows
are 8x8, like the birds. Side views face right (mirrored for left).
"""
import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, 'assets'))

import animal_art  # noqa: E402
import animals  # noqa: E402
from mockup_character import ACCENT, QUIET, TEXT, Screen  # noqa: E402
from mockup_doors import GRIDS, caption, sprite  # noqa: E402
import world  # noqa: E402

ROOT = os.path.join(HERE, '..', 'mockups')

ART = animal_art.FRAMES

CROW = animal_art.CROW   # 8x8, side, facing right: perched, pecking, wings up, wings down


def parse(rows):
    size = len(rows[0])
    assert all(len(r) == size for r in rows), [len(r) for r in rows]
    return [[animals.ANIMAL_KEYS[c] for c in r] for r in rows]


def draw(s, rows, feet_x, feet_y, mirror=False, scale=1, dark=0):
    grid = parse(rows)
    height, width = len(grid), len(grid[0])
    pal = animals.ANIMAL_PALETTE
    for y in range(height):
        for x in range(width):
            value = grid[y][width - 1 - x if mirror else x]
            if value:
                colour = tuple(c * (100 - dark) // 100 for c in pal[value][:3])
                for dy in range(scale):
                    for dx in range(scale):
                        px = feet_x - (width // 2) * scale + x * scale + dx
                        py = feet_y - height * scale + y * scale + dy
                        if 0 <= px < 240 and 0 <= py < 160:
                            s.pixels[px, py] = colour


DECALS = {name: grid for name, grid, bank in world.world_graphics()[0] if bank == 1}


def put(s, name, tx, ty, dark=0):
    grid, palette = (DECALS[name], world.WORLD_PALETTE_DECALS) if name in DECALS else (GRIDS[name], world.WORLD_PALETTE)
    for y in range(16):
        for x in range(16):
            value = grid[y][x]
            if value:
                px, py = tx * 16 + x, ty * 16 + y
                if 0 <= px < 240 and 0 <= py < 160:
                    s.pixels[px, py] = tuple(c * (100 - dark) // 100 for c in palette[value][:3])


def fill(s, name, dark=0):
    for ty in range(10):
        for tx in range(15):
            put(s, name if not isinstance(name, list) else name[(tx * 7 + ty * 3) % len(name)], tx, ty, dark)


def tree(s, tx, ty, dark=0):
    put(s, 'TREE_TRUNK', tx, ty, dark)
    put(s, 'CANOPY_BOTTOM', tx, ty, dark)
    put(s, 'CANOPY_TOP', tx, ty - 1, dark)


def sheet():
    s = Screen()
    s.rect(0, 0, 240, 160, world.WORLD_PALETTE[1])
    rows = [('WOLF', 'wolf', ['side_stand', 'side_run', 'dead']),
            ('BOAR', 'boar', ['side_stand', 'side_run', 'dead']),
            ('GOAT', 'goat', ['side_stand', 'side_step', 'dead']),
            ('HEN', 'hen', ['side_stand', 'side_peck', 'side_flap', 'dead'])]
    for row, (label, name, frames) in enumerate(rows):
        y = 4 + row * 39
        s.text(4, y + 14, label, TEXT)
        if len(frames) == 3:
            for column, frame in enumerate(frames):
                draw(s, ART[name][frame], 64 + column * 56, y + 34, scale=2)
        else:
            for column, frame in enumerate(frames):
                draw(s, ART[name][frame], 52 + column * 38, y + 34, scale=2)
        draw(s, ART[name][frames[0]], 222, y + 28)
    s.text(214, 4, '1X', QUIET)
    return s


def views_panel():
    s = Screen()
    s.rect(0, 0, 240, 160, world.WORLD_PALETTE[1])
    names = ['wolf', 'boar', 'goat', 'hen', 'deer']
    for column, name in enumerate(names):
        x = 26 + column * 46
        s.text(x - 12, 4, name.upper(), TEXT)
        for row, frame in enumerate(['down_stand', 'down_step', 'up_stand']):
            draw(s, ART[name][frame], x, 46 + row * 36, scale=2)
    s.text(4, 128, 'DEER', TEXT)
    for column, frame in enumerate(['side_stand', 'side_step', 'carcass']):
        draw(s, ART['deer'][frame], 52 + column * 34, 152)
    s.text(150, 128, 'RABBIT', TEXT)
    for column, frame in enumerate(['sit', 'hop']):
        draw(s, ART['rabbit'][frame], 172 + column * 22, 152)
    return s


def crows_panel():
    s = Screen()
    fill(s, ['GRASS_A', 'GRASS_A', 'GRASS_B'])
    for tx in range(15):
        put(s, 'ROAD', tx, 5)
        put(s, 'ROAD_LINE_H' if tx % 2 else 'ROAD', tx, 6)
        put(s, 'ROAD', tx, 7)
    s.text(4, 4, 'CROW', TEXT)
    for column, frame in enumerate(CROW):
        draw(s, frame, 20 + column * 28, 44, scale=3)
    for column, frame in enumerate(CROW):
        draw(s, frame, 140 + column * 12, 30)
    # Corpses on the road, crows on and round them; one taking off.
    for x, y in ((90, 104), (130, 112)):
        put(s, 'ROAD_CORPSE', x // 16, (y - 1) // 16)
    for x, y, frame, mirror in ((86, 100, 1, False), (98, 103, 0, True), (124, 108, 0, False),
                                (138, 110, 1, True), (150, 90, 2, False), (112, 112, 3, True)):
        draw(s, CROW[frame], x, y, mirror)
    sprite(s, 'player', 12, 200, 110, mirror=True)
    return s


def boar_panel():
    s = Screen()
    fill(s, ['FOREST_FLOOR', 'FOREST_FLOOR', 'GRASS_LUSH'])
    for tx, ty in ((1, 2), (4, 1), (9, 2), (12, 1), (2, 8), (13, 7), (7, 9)):
        tree(s, tx, ty)
    draw(s, ART['boar']['side_run'], 96, 98, mirror=False)
    sprite(s, 'player', 12, 140, 100, mirror=True)
    # The charge: dust behind it.
    for x, y in ((76, 97), (70, 95), (80, 99), (66, 98)):
        s.pixels[x, y] = (0xb8, 0xa8, 0x90)
    s.rect(4, 4, 92, 11, (0x0b, 0x0e, 0x14))
    s.text(7, 7, 'A BOAR CHARGES', ACCENT)
    draw(s, ART['boar']['dead'], 196, 140)
    return s


def wolves_panel():
    s = Screen()
    dark = 55
    fill(s, ['GRASS_A', 'GRASS_DRY', 'GRASS_A'], dark)
    for tx, ty in ((1, 2), (12, 1), (13, 8), (2, 9)):
        tree(s, tx, ty, dark)
    # The fire's light: the ground round it as by day, fading out.
    for y in range(160):
        for x in range(240):
            d = ((x - 120) ** 2 + (y - 92) ** 2) ** 0.5
            if d < 44:
                ty, tx = y // 16, x // 16
                name = ['GRASS_A', 'GRASS_DRY', 'GRASS_A'][(tx * 7 + ty * 3) % 3]
                value = GRIDS[name][y % 16][x % 16]
                lit = world.WORLD_PALETTE[value][:3]
                keep = 0 if d < 30 else int(55 * (d - 30) / 14)
                s.pixels[x, y] = tuple(c * (100 - keep) // 100 for c in lit)
    put(s, 'CAMPFIRE', 7, 5)
    for x, y, colour in ((119, 86, (0xe8, 0xc5, 0x47)), (120, 85, (0xe8, 0xc5, 0x47)), (121, 87, (0xe0, 0x90, 0x3a)),
                         (118, 88, (0xe0, 0x90, 0x3a)), (120, 88, (0xd0, 0x45, 0x3a)), (122, 89, (0xd0, 0x45, 0x3a)),
                         (120, 84, (0xf2, 0xdc, 0x7a)), (117, 89, (0xd0, 0x45, 0x3a))):
        s.pixels[x, y] = colour
    sprite(s, 'player', 0, 118, 76)
    # The pack circling outside the firelight.
    for x, y, frame, mirror in ((60, 60, 'side_run', False), (176, 70, 'side_stand', True), (150, 124, 'side_run', True)):
        draw(s, ART['wolf'][frame], x, y, mirror, dark=20)
    for x, y in ((68, 47), (167, 57)):   # eyeshine on the two facing the light
        s.pixels[x, y] = (0xe8, 0xc5, 0x47)
    s.rect(4, 4, 98, 11, (0x0b, 0x0e, 0x14))
    s.text(7, 7, 'WOLVES AT NIGHT', ACCENT)
    return s


def farm_panel():
    s = Screen()
    fill(s, ['GRASS_A', 'GRASS_B', 'GRASS_DRY'])
    for ty in range(6, 10):
        for tx in range(9, 15):
            put(s, 'SOIL', tx, ty)
    for tx in range(1, 8):
        put(s, 'FENCE_H' if 'FENCE_H' in GRIDS else 'GRASS_A', tx, 8)
    for x, y, frame, mirror in ((40, 60, 'side_stand', False), (60, 72, 'side_peck', True), (84, 64, 'side_flap', False),
                                (52, 100, 'side_peck', False)):
        draw(s, ART['hen'][frame], x, y, mirror)
    draw(s, ART['goat']['side_stand'], 140, 60, True)
    draw(s, ART['goat']['side_step'], 176, 84)
    sprite(s, 'player', 12, 110, 120, mirror=True)
    s.rect(4, 4, 70, 11, (0x0b, 0x0e, 0x14))
    s.text(7, 7, 'FARM ANIMALS', ACCENT)
    return s


def main():
    panels = [
        (sheet(), [('1 WOLF, BOAR, GOAT, HEN', ACCENT), ('STAND, MOVE, DEAD (HEN: PECK, FLAP)', QUIET)]),
        (crows_panel(), [('2 CROWS ON CORPSES', ACCENT), ('PERCH, PECK, WINGS UP, DOWN', QUIET)]),
        (boar_panel(), [('3 A BOAR CHARGES IN THE WOODS', ACCENT), ('A GORE, THEN IT TURNS AWAY', QUIET)]),
        (wolves_panel(), [('4 A PACK CIRCLES THE CAMPFIRE', ACCENT), ('EYES IN THE DARK; FIRE KEEPS THEM OFF', QUIET)]),
        (farm_panel(), [('5 CHICKENS AND GOATS ON A FARM', ACCENT), ('CATCH, BUTCHER, EGGS AND MILK', QUIET)]),
        (views_panel(), [('6 FRONT AND BACK; DEER, RABBIT', ACCENT), ('FACING YOU, A STEP, FACING AWAY', QUIET)]),
    ]
    gap = 4
    columns = 3
    rows = 2
    sheet_image = Image.new('RGB', (240 * columns + gap * (columns - 1), (160 + 22) * rows + gap), (0x05, 0x07, 0x0b))
    for index, (screen, lines) in enumerate(panels):
        x = (index % columns) * (240 + gap)
        y = (index // columns) * (160 + 22 + gap)
        sheet_image.paste(screen.image, (x, y))
        sheet_image.paste(caption(240, lines), (x, y + 160))
    sheet_image = sheet_image.resize((sheet_image.width * 3, sheet_image.height * 3), Image.NEAREST)
    sheet_image.save(os.path.join(ROOT, '19-animals.png'))
    print('wrote mockups/19-animals.png')


if __name__ == '__main__':
    main()
